#include "FacilityState.hpp"

#include <DynamicOutput/Output.hpp>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/Hooks/Hooks.hpp>
#include <Unreal/UObjectGlobals.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <deque>
#include <optional>
#include <random>
#include <ranges>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct ParamBuffer {
    RC::Unreal::UFunction* function{};
    std::vector<std::byte> bytes;

    explicit ParamBuffer(RC::Unreal::UFunction* in_function)
        : function(in_function),
          bytes(in_function == nullptr ? 0U : static_cast<std::size_t>(in_function->GetParmsSize()), std::byte{0}) {
        if (function == nullptr) return;
        for (RC::Unreal::TFieldIterator<RC::Unreal::FProperty> it{function}; it; ++it) {
            auto* property = *it;
            if (property != nullptr && property->HasAnyPropertyFlags(RC::Unreal::CPF_Parm)) {
                property->InitializeValue_InContainer(bytes.data());
            }
        }
    }

    ~ParamBuffer() {
        if (function == nullptr) return;
        for (RC::Unreal::TFieldIterator<RC::Unreal::FProperty> it{function}; it; ++it) {
            auto* property = *it;
            if (property != nullptr && property->HasAnyPropertyFlags(RC::Unreal::CPF_Parm)) {
                property->DestroyValue_InContainer(bytes.data());
            }
        }
    }

    ParamBuffer(const ParamBuffer&) = delete;
    ParamBuffer& operator=(const ParamBuffer&) = delete;
};

struct Reward { RC::Unreal::FName item_id; int32_t count{}; };
struct PendingJob {
    int32_t slot_index{};
    std::string key;
    std::vector<Reward> rewards;
    Clock::time_point due_at{};
};
struct FacilityRuntime {
    RC::Unreal::UObject* egg_container{};
    std::deque<PendingJob> jobs;
    std::unordered_set<std::string> queued_keys;
    Clock::time_point next_due{};
};
struct SlotMutation {
    RC::Unreal::UObject* slot{};
    RC::Unreal::FName item_id;
    int32_t new_count{};
    bool replace_item{};
};

[[nodiscard]] auto find_function(const RC::StringType& path) -> RC::Unreal::UFunction* {
    return RC::Unreal::UObjectGlobals::StaticFindObject<RC::Unreal::UFunction*>(nullptr, nullptr, path);
}
[[nodiscard]] auto find_default_object(const RC::StringType& path) -> RC::Unreal::UObject* {
    return RC::Unreal::UObjectGlobals::StaticFindObject<RC::Unreal::UObject*>(nullptr, nullptr, path);
}
[[nodiscard]] auto find_class(const RC::StringType& path) -> RC::Unreal::UClass* {
    return RC::Unreal::UObjectGlobals::StaticFindObject<RC::Unreal::UClass*>(nullptr, nullptr, path);
}
[[nodiscard]] auto name_string(const RC::Unreal::FName& value) -> std::string {
    return RC::to_string(value.ToString());
}
[[nodiscard]] auto property_in_chain(RC::Unreal::UObject* object, const wchar_t* name) -> RC::Unreal::FProperty* {
    return object == nullptr ? nullptr : object->GetPropertyByNameInChain(name);
}
[[nodiscard]] auto struct_property(RC::Unreal::UStruct* owner, const wchar_t* name) -> RC::Unreal::FProperty* {
    return owner == nullptr ? nullptr : owner->GetPropertyByName(name);
}

auto call_no_args(RC::Unreal::UObject* context, RC::Unreal::UFunction* function) -> void {
    if (context == nullptr || function == nullptr) return;
    ParamBuffer params{function};
    context->ProcessEvent(function, params.bytes.data());
}

[[nodiscard]] auto call_object_return(
    RC::Unreal::UObject* context, RC::Unreal::UFunction* function, RC::Unreal::UObject* world_context = nullptr
) -> RC::Unreal::UObject* {
    using namespace RC::Unreal;
    if (context == nullptr || function == nullptr) return nullptr;
    ParamBuffer params{function};
    if (world_context != nullptr) {
        if (auto* input = CastField<FObjectProperty>(function->GetPropertyByName(STR("WorldContextObject")))) {
            *input->ContainerPtrToValuePtr<UObject*>(params.bytes.data()) = world_context;
        }
    }
    context->ProcessEvent(function, params.bytes.data());
    auto* result = CastField<FObjectProperty>(function->GetReturnProperty());
    return result == nullptr ? nullptr : *result->ContainerPtrToValuePtr<UObject*>(params.bytes.data());
}

class PalResourceFactoryNative final : public RC::CppUserModBase {
public:
    PalResourceFactoryNative()
        : processor_config_(prf::egg_processor_config()), breeder_config_(prf::breeding_factory_config()),
          random_(std::random_device{}()) {
        ModName = STR("PalResourceFactoryNative");
        ModVersion = STR("0.3.4-interaction-bootstrap-preview");
        ModDescription = STR("Server-side irreversible egg resource processing preview");
        ModAuthors = STR("Paulus");
    }

    ~PalResourceFactoryNative() override {
        using namespace RC::Unreal;
        if (hatch_finish_function_ != nullptr && hatch_finish_hook_id_ >= 0) {
            static_cast<void>(hatch_finish_function_->UnregisterHook(hatch_finish_hook_id_));
        }
        if (container_update_function_ != nullptr && container_update_hook_id_ >= 0) {
            static_cast<void>(container_update_function_->UnregisterHook(container_update_hook_id_));
        }
        if (engine_tick_hook_id_ != Hook::ERROR_ID) static_cast<void>(Hook::UnregisterCallback(engine_tick_hook_id_));
    }

    auto on_unreal_init() -> void override {
        using namespace RC;
        using namespace RC::Unreal;

        engine_tick_hook_id_ = Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>&, UEngine*, float, bool) {
                if (try_activate_processing_contract()) process_pending_jobs();
            },
            Hook::FCallbackOptions{false, false, STR("PalResourceFactoryNative"), STR("ProcessPendingResources")}
        );
        if (engine_tick_hook_id_ == Hook::ERROR_ID) {
            Output::send<LogLevel::Error>(
                STR("[PalResourceFactory] bootstrap failed: engine tick hook unavailable; do not use either custom building.\n")
            );
            return;
        }
        Output::send<LogLevel::Warning>(
            STR("[PalResourceFactory] bootstrap ready global_event_hook=false; waiting for gameplay reflection objects.\n")
        );
    }

private:
    [[nodiscard]] auto try_activate_processing_contract() -> bool {
        using namespace RC;
        using namespace RC::Unreal;
        if (hooks_ready_) return true;

        const auto now = Clock::now();
        if (now < next_contract_probe_) return false;
        next_contract_probe_ = now + std::chrono::seconds{1};

        try_get_map_object_id_function_ = find_function(STR("/Script/Pal.PalMapObjectConcreteModelBase:TryGetMapObjectId"));
        hatching_model_class_ = find_class(STR("/Script/Pal.PalMapObjectHatchingEggModelBase"));
        get_base_camp_function_ = find_function(STR("/Script/Pal.PalMapObjectConcreteModelBase:GetBaseCampModelBelongTo"));
        hatch_finish_function_ = find_function(STR("/Script/Pal.PalMapObjectHatchingEggModelBase:OnFinishWorkInServer"));
        container_update_function_ = find_function(STR("/Script/Pal.PalMapObjectHatchingEggModelBase:OnUpdateContainerContentInServer"));
        get_database_function_ = find_function(STR("/Script/Pal.PalUtility:GetDatabaseCharacterParameter"));
        get_container_manager_function_ = find_function(STR("/Script/Pal.PalUtility:GetItemContainerManager"));
        get_drop_data_function_ = find_function(STR("/Script/Pal.PalDatabaseCharacterParameter:GetDropItemData"));
        try_get_container_function_ = find_function(STR("/Script/Pal.PalItemContainerManager:TryGetContainer"));
        get_max_stack_function_ = find_function(STR("/Script/Pal.PalItemSlot:GetMaxStack"));
        slot_on_rep_item_function_ = find_function(STR("/Script/Pal.PalItemSlot:OnRep_ItemId"));
        slot_on_rep_stack_function_ = find_function(STR("/Script/Pal.PalItemSlot:OnRep_StackCount"));
        container_on_rep_function_ = find_function(STR("/Script/Pal.PalItemContainer:OnRep_ItemSlotArray"));
        model_on_rep_function_ = find_function(STR("/Script/Pal.PalMapObjectHatchingEggModelBase:OnRepEggInfoArray"));
        pal_utility_default_ = find_default_object(STR("/Script/Pal.Default__PalUtility"));

        auto report_missing = [this](const char* name, const void* value) {
            if (value != nullptr || !reported_missing_symbols_.insert(name).second) return;
            RC::Output::send<RC::LogLevel::Error>(
                STR("[PalResourceFactory] reflection object not loaded yet: {}\n"), RC::to_wstring(name)
            );
        };
        report_missing("PalMapObjectHatchingEggModelBase", hatching_model_class_);
        report_missing("TryGetMapObjectId", try_get_map_object_id_function_);
        report_missing("GetBaseCampModelBelongTo", get_base_camp_function_);
        report_missing("OnFinishWorkInServer", hatch_finish_function_);
        report_missing("OnUpdateContainerContentInServer", container_update_function_);
        report_missing("GetDatabaseCharacterParameter", get_database_function_);
        report_missing("GetItemContainerManager", get_container_manager_function_);
        report_missing("GetDropItemData", get_drop_data_function_);
        report_missing("TryGetContainer", try_get_container_function_);
        report_missing("GetMaxStack", get_max_stack_function_);
        report_missing("OnRep_ItemId", slot_on_rep_item_function_);
        report_missing("OnRep_StackCount", slot_on_rep_stack_function_);
        report_missing("OnRep_ItemSlotArray", container_on_rep_function_);
        report_missing("OnRepEggInfoArray", model_on_rep_function_);
        report_missing("Default__PalUtility", pal_utility_default_);

        const std::array<UFunction**, 13> required{
            &try_get_map_object_id_function_, &get_base_camp_function_, &hatch_finish_function_,
            &container_update_function_, &get_database_function_, &get_container_manager_function_,
            &get_drop_data_function_, &try_get_container_function_, &get_max_stack_function_,
            &slot_on_rep_item_function_, &slot_on_rep_stack_function_, &container_on_rep_function_,
            &model_on_rep_function_
        };
        safety_contract_ready_ = hatching_model_class_ != nullptr
            && try_get_map_object_id_function_ != nullptr;
        reflection_contract_ready_ = safety_contract_ready_ && pal_utility_default_ != nullptr
            && std::ranges::all_of(required, [](auto* value) { return *value != nullptr; });
        if (!reflection_contract_ready_) return false;

        if (hatch_finish_hook_id_ < 0) {
            hatch_finish_hook_id_ = hatch_finish_function_->RegisterPostHook(
                [this](UnrealScriptFunctionCallableContext& context, void*) { scan_completed_hatches(context.Context); }
            );
        }
        if (container_update_hook_id_ < 0) {
            container_update_hook_id_ = container_update_function_->RegisterPostHook(
                [this](UnrealScriptFunctionCallableContext& context, void*) {
                    cache_egg_container(context);
                    scan_completed_hatches(context.Context);
                }
            );
        }
        hooks_ready_ = hatch_finish_hook_id_ >= 0 && container_update_hook_id_ >= 0;
        if (!hooks_ready_) {
            if (!hook_failure_reported_) {
                hook_failure_reported_ = true;
                Output::send<LogLevel::Error>(
                    STR("[PalResourceFactory] processing hook registration failed; retrying without enabling processing.\n")
                );
            }
            return false;
        }

        disassembly_interval_seconds_ = read_disassembly_interval();
        Output::send<LogLevel::Warning>(
            STR("[PalResourceFactory] processing hooks ready global_event_hook=false processor_slots={} breeder_slots={} interval={:.2f}s\n"),
            processor_config_.input_capacity, breeder_config_.incubation_capacity, disassembly_interval_seconds_
        );
        return true;
    }

    [[nodiscard]] auto try_get_map_object_id(RC::Unreal::UObject* context) const -> std::string {
        using namespace RC::Unreal;
        if (context == nullptr || !UObject::IsReal(context) || hatching_model_class_ == nullptr
            || !context->IsA(hatching_model_class_) || try_get_map_object_id_function_ == nullptr) return {};
        ParamBuffer params{try_get_map_object_id_function_};
        context->ProcessEvent(try_get_map_object_id_function_, params.bytes.data());
        auto* result = CastField<FNameProperty>(try_get_map_object_id_function_->GetReturnProperty());
        return result == nullptr ? std::string{} : name_string(*result->ContainerPtrToValuePtr<FName>(params.bytes.data()));
    }
    static auto is_custom_facility_id(const std::string& id) -> bool {
        return id == "PRF_EggResourceProcessor" || id == "PRF_ResourceBreedingFacility";
    }
    static auto is_processor_id(const std::string& id) -> bool { return id == "PRF_EggResourceProcessor"; }
    auto cache_egg_container(RC::Unreal::UnrealScriptFunctionCallableContext& call) -> void {
        using namespace RC::Unreal;
        if (!is_custom_facility_id(try_get_map_object_id(call.Context))) return;
        auto* property = CastField<FObjectProperty>(container_update_function_->GetPropertyByName(STR("Container")));
        if (property == nullptr) return;
        auto* container = *property->ContainerPtrToValuePtr<UObject*>(call.TheStack.Locals());
        if (container != nullptr) facilities_[call.Context].egg_container = container;
    }

    [[nodiscard]] auto read_disassembly_interval() const -> double {
        using namespace RC::Unreal;
        auto* defaults = find_default_object(STR("/Script/Pal.Default__PalMapObjectConvertCharacterToItemParameterComponent"));
        auto* property = CastField<FFloatProperty>(property_in_chain(defaults, L"RequiredConvertProcessTime"));
        if (property == nullptr) return 1.0;
        const auto value = static_cast<double>(*property->ContainerPtrToValuePtr<float>(defaults));
        return value > 0.0 ? value : 1.0;
    }

    [[nodiscard]] auto query_rewards(
        RC::Unreal::UObject* model, const RC::Unreal::FName& character_id, int32_t level
    ) -> std::optional<std::vector<Reward>> {
        using namespace RC::Unreal;
        auto* database = call_object_return(pal_utility_default_, get_database_function_, model);
        if (database == nullptr) return std::nullopt;
        ParamBuffer params{get_drop_data_function_};
        auto* character = CastField<FNameProperty>(get_drop_data_function_->GetPropertyByName(STR("CharacterID")));
        auto* level_input = CastField<FIntProperty>(get_drop_data_function_->GetPropertyByName(STR("Level")));
        auto* output = CastField<FStructProperty>(get_drop_data_function_->GetPropertyByName(STR("OutData")));
        if (character == nullptr || level_input == nullptr || output == nullptr) return std::nullopt;
        *character->ContainerPtrToValuePtr<FName>(params.bytes.data()) = character_id;
        *level_input->ContainerPtrToValuePtr<int32_t>(params.bytes.data()) = level;
        database->ProcessEvent(get_drop_data_function_, params.bytes.data());
        auto* success = CastField<FBoolProperty>(get_drop_data_function_->GetReturnProperty());
        if (success == nullptr || !success->GetPropertyValueInContainer(params.bytes.data())) return std::nullopt;

        auto* row_data = output->ContainerPtrToValuePtr<void>(params.bytes.data());
        auto* row_struct = output->GetStruct().Get();
        std::vector<Reward> rewards;
        std::uniform_real_distribution<float> chance{0.0F, 100.0F};
        for (int index = 1; index <= 10; ++index) {
            const auto suffix = std::to_wstring(index);
            auto* item = CastField<FNameProperty>(struct_property(row_struct, (L"ItemId" + suffix).c_str()));
            auto* rate = CastField<FFloatProperty>(struct_property(row_struct, (L"Rate" + suffix).c_str()));
            auto* minimum = CastField<FIntProperty>(struct_property(row_struct, (L"min" + suffix).c_str()));
            auto* maximum = CastField<FIntProperty>(struct_property(row_struct, (L"Max" + suffix).c_str()));
            if (item == nullptr || rate == nullptr || minimum == nullptr || maximum == nullptr) continue;
            const auto item_id = *item->ContainerPtrToValuePtr<FName>(row_data);
            const auto item_text = name_string(item_id);
            const auto rate_value = *rate->ContainerPtrToValuePtr<float>(row_data);
            const auto min_value = *minimum->ContainerPtrToValuePtr<int32_t>(row_data);
            const auto max_value = *maximum->ContainerPtrToValuePtr<int32_t>(row_data);
            if (item_text.empty() || item_text == "None" || rate_value <= 0.0F || max_value <= 0 || chance(random_) > rate_value) continue;
            std::uniform_int_distribution<int32_t> count{std::max(1, min_value), std::max(std::max(1, min_value), max_value)};
            rewards.push_back(Reward{item_id, count(random_)});
        }
        return rewards;
    }

    auto scan_completed_hatches(RC::Unreal::UObject* model) -> void {
        using namespace RC::Unreal;
        const auto id = try_get_map_object_id(model);
        if (!is_custom_facility_id(id)) return;
        auto* rep = CastField<FStructProperty>(property_in_chain(model, L"RepInfoArray"));
        auto* items_property = rep == nullptr ? nullptr : CastField<FArrayProperty>(struct_property(rep->GetStruct().Get(), L"Items"));
        if (items_property == nullptr) return;
        auto* rep_data = rep->ContainerPtrToValuePtr<void>(model);
        FScriptArrayHelper items{items_property, items_property->ContainerPtrToValuePtr<void>(rep_data)};
        auto* item_property = CastField<FStructProperty>(items_property->GetInner());
        auto* item_struct = item_property == nullptr ? nullptr : item_property->GetStruct().Get();
        auto* slot = CastField<FIntProperty>(struct_property(item_struct, L"SlotIndex"));
        auto* save = CastField<FStructProperty>(struct_property(item_struct, L"HatchedCharacterSaveParameter"));
        auto* character = save == nullptr ? nullptr : CastField<FNameProperty>(struct_property(save->GetStruct().Get(), L"CharacterID"));
        auto* level = save == nullptr ? nullptr : CastField<FByteProperty>(struct_property(save->GetStruct().Get(), L"Level"));
        if (slot == nullptr || save == nullptr || character == nullptr || level == nullptr) return;

        auto& runtime = facilities_[model];
        const auto now = Clock::now();
        for (int32_t index = 0; index < items.Num(); ++index) {
            const auto pending_limit = is_processor_id(id)
                ? processor_config_.input_capacity : breeder_config_.incubation_capacity;
            if (runtime.jobs.size() >= pending_limit) break;
            auto* item_data = items.GetRawPtr(index);
            const auto slot_index = *slot->ContainerPtrToValuePtr<int32_t>(item_data);
            auto* save_data = save->ContainerPtrToValuePtr<void>(item_data);
            const auto character_id = *character->ContainerPtrToValuePtr<FName>(save_data);
            const auto pal_level = static_cast<int32_t>(*level->ContainerPtrToValuePtr<uint8_t>(save_data));
            const auto character_text = name_string(character_id);
            if (character_text.empty() || character_text == "None") continue;
            const auto key = std::to_string(slot_index) + ":" + character_text;
            if (runtime.queued_keys.contains(key)) continue;
            auto rewards = query_rewards(model, character_id, pal_level);
            if (!rewards.has_value()) {
                RC::Output::send<RC::LogLevel::Error>(STR("[PalResourceFactory] vanilla drops unavailable for {} level {}; locked\n"),
                                                     RC::to_wstring(character_text), pal_level);
                continue;
            }
            // Reserve the key before invoking either OnRep path.  Clearing the
            // item container can synchronously broadcast its update delegate
            // and re-enter this scanner in single-player.  Without this guard,
            // the same completed hatch could be claimed recursively.
            runtime.queued_keys.insert(key);
            // Take irreversible ownership before scheduling any reward.  The
            // previous build left the vanilla hatch record alive until after
            // material delivery.  By then Palworld had often removed or moved
            // it itself, so the job froze and additional offspring accumulated
            // in slots 0..8.  Clearing the egg and consumed record here makes
            // the vanilla obtain button unable to create a living Pal.
            if (!clear_egg_slot(runtime, slot_index)) {
                RC::Output::send<RC::LogLevel::Error>(
                    STR("[PalResourceFactory] takeover waiting for egg container facility={} slot={}\n"),
                    RC::to_wstring(id), slot_index
                );
                runtime.queued_keys.erase(key);
                continue;
            }
            if (!remove_rep_info(model, slot_index)) {
                RC::Output::send<RC::LogLevel::Error>(
                    STR("[PalResourceFactory] takeover failed before reward facility={} slot={}\n"),
                    RC::to_wstring(id), slot_index
                );
                runtime.queued_keys.erase(key);
                continue;
            }
            const auto due = std::max(now, runtime.next_due) + std::chrono::duration_cast<Clock::duration>(
                std::chrono::duration<double>{disassembly_interval_seconds_});
            runtime.next_due = due;
            runtime.jobs.push_back(PendingJob{slot_index, key, std::move(*rewards), due});
            RC::Output::send<RC::LogLevel::Warning>(STR("[PalResourceFactory] takeover queued facility={} slot={} pal={} level={}\n"),
                                                   RC::to_wstring(id), slot_index, RC::to_wstring(character_text), pal_level);
        }
    }

    [[nodiscard]] auto resolve_base_containers(RC::Unreal::UObject* model) -> std::vector<RC::Unreal::UObject*> {
        using namespace RC::Unreal;
        std::vector<UObject*> result;
        auto* base = call_object_return(model, get_base_camp_function_);
        auto* manager = call_object_return(pal_utility_default_, get_container_manager_function_, model);
        auto* modules_property = CastField<FArrayProperty>(property_in_chain(base, L"ModuleArray"));
        if (base == nullptr || manager == nullptr || modules_property == nullptr) return result;
        FScriptArrayHelper modules{modules_property, modules_property->ContainerPtrToValuePtr<void>(base)};
        UObject* storage = nullptr;
        for (int32_t index = 0; index < modules.Num(); ++index) {
            auto* module = *reinterpret_cast<UObject**>(modules.GetRawPtr(index));
            if (module != nullptr && RC::to_string(module->GetFullName()).find("PalBaseCampModuleItemStorage") != std::string::npos) {
                storage = module;
                break;
            }
        }
        if (storage == nullptr) return result;

        auto resolve_info = [&](void* info_data, UStruct* info_struct) {
            auto* source = CastField<FStructProperty>(struct_property(info_struct, L"ContainerIdCache"));
            auto* target = CastField<FStructProperty>(try_get_container_function_->GetPropertyByName(STR("ContainerId")));
            auto* output = CastField<FObjectProperty>(try_get_container_function_->GetPropertyByName(STR("Container")));
            if (source == nullptr || target == nullptr || output == nullptr) return;
            ParamBuffer params{try_get_container_function_};
            source->CopyCompleteValue(target->ContainerPtrToValuePtr<void>(params.bytes.data()),
                                      source->ContainerPtrToValuePtr<void>(info_data));
            manager->ProcessEvent(try_get_container_function_, params.bytes.data());
            auto* container = *output->ContainerPtrToValuePtr<UObject*>(params.bytes.data());
            if (container != nullptr && std::ranges::find(result, container) == result.end()) result.push_back(container);
        };

        auto* infos_property = CastField<FArrayProperty>(property_in_chain(storage, L"ContainerInfos"));
        if (infos_property != nullptr) {
            FScriptArrayHelper infos{infos_property, infos_property->ContainerPtrToValuePtr<void>(storage)};
            auto* info = CastField<FStructProperty>(infos_property->GetInner());
            if (info != nullptr) for (int32_t index = 0; index < infos.Num(); ++index) resolve_info(infos.GetRawPtr(index), info->GetStruct().Get());
        }
        auto* guild = CastField<FStructProperty>(property_in_chain(storage, L"GuildContainerInfo"));
        if (guild != nullptr) resolve_info(guild->ContainerPtrToValuePtr<void>(storage), guild->GetStruct().Get());
        return result;
    }

    [[nodiscard]] auto get_max_stack(RC::Unreal::UObject* slot) const -> int32_t {
        using namespace RC::Unreal;
        ParamBuffer params{get_max_stack_function_};
        slot->ProcessEvent(get_max_stack_function_, params.bytes.data());
        auto* result = CastField<FIntProperty>(get_max_stack_function_->GetReturnProperty());
        return result == nullptr ? 0 : *result->ContainerPtrToValuePtr<int32_t>(params.bytes.data());
    }

    [[nodiscard]] auto plan_storage_mutations(
        const std::vector<RC::Unreal::UObject*>& containers, const std::vector<Reward>& rewards
    ) const -> std::optional<std::vector<SlotMutation>> {
        using namespace RC::Unreal;
        struct Candidate { UObject* slot{}; std::string item; int32_t count{}; int32_t maximum{}; };
        std::vector<Candidate> candidates;
        for (auto* container : containers) {
            auto* slots_property = CastField<FArrayProperty>(property_in_chain(container, L"ItemSlotArray"));
            if (slots_property == nullptr) continue;
            FScriptArrayHelper slots{slots_property, slots_property->ContainerPtrToValuePtr<void>(container)};
            for (int32_t index = 0; index < slots.Num(); ++index) {
                auto* slot = *reinterpret_cast<UObject**>(slots.GetRawPtr(index));
                auto* item = CastField<FStructProperty>(property_in_chain(slot, L"ItemId"));
                auto* count = CastField<FIntProperty>(property_in_chain(slot, L"StackCount"));
                auto* static_id = item == nullptr ? nullptr : CastField<FNameProperty>(struct_property(item->GetStruct().Get(), L"StaticId"));
                if (slot == nullptr || item == nullptr || count == nullptr || static_id == nullptr) continue;
                auto* item_data = item->ContainerPtrToValuePtr<void>(slot);
                const auto item_text = name_string(*static_id->ContainerPtrToValuePtr<FName>(item_data));
                const auto stack = *count->ContainerPtrToValuePtr<int32_t>(slot);
                candidates.push_back(Candidate{slot, item_text, stack, stack <= 0 ? 9999 : std::max(stack, get_max_stack(slot))});
            }
        }
        std::vector<SlotMutation> mutations;
        for (const auto& reward : rewards) {
            auto remaining = reward.count;
            const auto wanted = name_string(reward.item_id);
            for (auto& candidate : candidates) {
                if (remaining <= 0 || candidate.item != wanted || candidate.count >= candidate.maximum) continue;
                const auto added = std::min(remaining, candidate.maximum - candidate.count);
                candidate.count += added;
                remaining -= added;
                mutations.push_back(SlotMutation{candidate.slot, reward.item_id, candidate.count, false});
            }
            for (auto& candidate : candidates) {
                if (remaining <= 0 || (candidate.count > 0 && candidate.item != "None" && !candidate.item.empty())) continue;
                const auto added = std::min(remaining, candidate.maximum);
                candidate.item = wanted;
                candidate.count = added;
                remaining -= added;
                mutations.push_back(SlotMutation{candidate.slot, reward.item_id, candidate.count, true});
            }
            if (remaining > 0) return std::nullopt;
        }
        return mutations;
    }

    auto apply_storage_mutations(const std::vector<SlotMutation>& mutations) const -> bool {
        using namespace RC::Unreal;
        for (const auto& mutation : mutations) {
            auto* item = CastField<FStructProperty>(property_in_chain(mutation.slot, L"ItemId"));
            auto* count = CastField<FIntProperty>(property_in_chain(mutation.slot, L"StackCount"));
            if (item == nullptr || count == nullptr) return false;
            if (mutation.replace_item) {
                item->ClearValue_InContainer(mutation.slot);
                auto* static_id = CastField<FNameProperty>(struct_property(item->GetStruct().Get(), L"StaticId"));
                if (static_id == nullptr) return false;
                *static_id->ContainerPtrToValuePtr<FName>(item->ContainerPtrToValuePtr<void>(mutation.slot)) = mutation.item_id;
            }
            *count->ContainerPtrToValuePtr<int32_t>(mutation.slot) = mutation.new_count;
            call_no_args(mutation.slot, slot_on_rep_item_function_);
            call_no_args(mutation.slot, slot_on_rep_stack_function_);
        }
        return true;
    }

    auto clear_egg_slot(FacilityRuntime& runtime, int32_t slot_index) const -> bool {
        using namespace RC::Unreal;
        if (runtime.egg_container == nullptr || !UObject::IsReal(runtime.egg_container)) return false;
        auto* slots_property = CastField<FArrayProperty>(property_in_chain(runtime.egg_container, L"ItemSlotArray"));
        if (slots_property == nullptr) return false;
        FScriptArrayHelper slots{slots_property, slots_property->ContainerPtrToValuePtr<void>(runtime.egg_container)};
        if (slot_index < 0 || slot_index >= slots.Num()) return false;
        auto* slot = *reinterpret_cast<UObject**>(slots.GetRawPtr(slot_index));
        auto* item = CastField<FStructProperty>(property_in_chain(slot, L"ItemId"));
        auto* count = CastField<FIntProperty>(property_in_chain(slot, L"StackCount"));
        if (slot == nullptr || item == nullptr || count == nullptr) return false;
        item->ClearValue_InContainer(slot);
        *count->ContainerPtrToValuePtr<int32_t>(slot) = 0;
        call_no_args(slot, slot_on_rep_item_function_);
        call_no_args(slot, slot_on_rep_stack_function_);
        call_no_args(runtime.egg_container, container_on_rep_function_);
        return true;
    }

    auto remove_rep_info(RC::Unreal::UObject* model, int32_t slot_index) const -> bool {
        using namespace RC::Unreal;
        auto* rep = CastField<FStructProperty>(property_in_chain(model, L"RepInfoArray"));
        auto* items_property = rep == nullptr ? nullptr : CastField<FArrayProperty>(struct_property(rep->GetStruct().Get(), L"Items"));
        if (items_property == nullptr) return false;
        auto* rep_data = rep->ContainerPtrToValuePtr<void>(model);
        FScriptArrayHelper items{items_property, items_property->ContainerPtrToValuePtr<void>(rep_data)};
        auto* item = CastField<FStructProperty>(items_property->GetInner());
        auto* slot = item == nullptr ? nullptr : CastField<FIntProperty>(struct_property(item->GetStruct().Get(), L"SlotIndex"));
        if (slot == nullptr) return false;
        for (int32_t index = 0; index < items.Num(); ++index) {
            if (*slot->ContainerPtrToValuePtr<int32_t>(items.GetRawPtr(index)) == slot_index) {
                // UE4SS's memory-image array allocator removal helper is not exported by
                // this runtime build. Clear the replicated record in-place instead: the
                // empty CharacterID is our persistent consumed marker and cannot be
                // discovered as another completed hatch after a save/reload.
                items.ClearValues(index);
                call_no_args(model, model_on_rep_function_);
                return true;
            }
        }
        return false;
    }

    auto process_pending_jobs() -> void {
        using namespace RC;
        using namespace RC::Unreal;
        const auto now = Clock::now();
        // A completed record can remain behind while the two-slot processing
        // queue is full.  Rescan every tick so it is claimed as soon as one
        // processing position becomes available, without requiring another UI
        // or container event.
        std::vector<UObject*> models;
        models.reserve(facilities_.size());
        for (const auto& [model, runtime] : facilities_) {
            static_cast<void>(runtime);
            if (model != nullptr && UObject::IsReal(model)) models.push_back(model);
        }
        for (auto* model : models) scan_completed_hatches(model);
        for (auto iterator = facilities_.begin(); iterator != facilities_.end();) {
            auto* model = iterator->first;
            auto& runtime = iterator->second;
            if (model == nullptr || !UObject::IsReal(model)) {
                iterator = facilities_.erase(iterator);
                continue;
            }
            if (runtime.jobs.empty() || runtime.jobs.front().due_at > now) {
                ++iterator;
                continue;
            }
            auto& job = runtime.jobs.front();
            const auto containers = resolve_base_containers(model);
            const auto mutations = plan_storage_mutations(containers, job.rewards);
            if (!mutations.has_value()) {
                job.due_at = now + std::chrono::seconds{1};
                ++iterator;
                continue;
            }
            if (!apply_storage_mutations(*mutations)) {
                Output::send<LogLevel::Error>(STR("[PalResourceFactory] storage mutation failed after takeover; stop this test save\n"));
                job.due_at = Clock::time_point::max();
                ++iterator;
                continue;
            }
            for (auto* container : containers) call_no_args(container, container_on_rep_function_);
            Output::send<LogLevel::Warning>(STR("[PalResourceFactory] committed slot={} reward_types={}\n"),
                                           job.slot_index, job.rewards.size());
            runtime.queued_keys.erase(job.key);
            runtime.jobs.pop_front();
            ++iterator;
        }
    }

    prf::FacilityConfig processor_config_;
    prf::FacilityConfig breeder_config_;
    std::mt19937 random_;
    std::unordered_map<RC::Unreal::UObject*, FacilityRuntime> facilities_;
    RC::Unreal::UClass* hatching_model_class_{};
    RC::Unreal::UFunction* try_get_map_object_id_function_{};
    RC::Unreal::UFunction* get_base_camp_function_{};
    RC::Unreal::UFunction* hatch_finish_function_{};
    RC::Unreal::UFunction* container_update_function_{};
    RC::Unreal::UFunction* get_database_function_{};
    RC::Unreal::UFunction* get_container_manager_function_{};
    RC::Unreal::UFunction* get_drop_data_function_{};
    RC::Unreal::UFunction* try_get_container_function_{};
    RC::Unreal::UFunction* get_max_stack_function_{};
    RC::Unreal::UFunction* slot_on_rep_item_function_{};
    RC::Unreal::UFunction* slot_on_rep_stack_function_{};
    RC::Unreal::UFunction* container_on_rep_function_{};
    RC::Unreal::UFunction* model_on_rep_function_{};
    RC::Unreal::UObject* pal_utility_default_{};
    int32_t hatch_finish_hook_id_{-1};
    int32_t container_update_hook_id_{-1};
    RC::Unreal::Hook::GlobalCallbackId engine_tick_hook_id_{RC::Unreal::Hook::ERROR_ID};
    double disassembly_interval_seconds_{1.0};
    bool reflection_contract_ready_{false};
    bool safety_contract_ready_{false};
    bool hooks_ready_{false};
    bool hook_failure_reported_{false};
    Clock::time_point next_contract_probe_{};
    std::unordered_set<std::string> reported_missing_symbols_;
};

} // namespace

#define PAL_RESOURCE_FACTORY_API __declspec(dllexport)
extern "C" {
PAL_RESOURCE_FACTORY_API RC::CppUserModBase* start_mod() { return new PalResourceFactoryNative(); }
PAL_RESOURCE_FACTORY_API void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }
}
