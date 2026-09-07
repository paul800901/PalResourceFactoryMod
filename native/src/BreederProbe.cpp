#include "SettlementRuntime.hpp"
#include "BreederEffects.hpp"
#include <Unreal/Engine/UDataTable.hpp>

namespace {
#include "AncientBreederReadback.hpp"
// Temporary integration check: queries original farms only on an explicit key.
// No inventory writes, work assignment, RNG, egg generation or hooked breeding.
void set_object(Parameters& args, const wchar_t* name, UObject* value) {
    auto* field = CastField<FObjectProperty>(args.function->GetPropertyByName(name));
    require(field, "Breeding query argument differs");
    *field->ContainerPtrToValuePtr<UObject*>(args.data.data()) = value;
}
float float_result(Parameters& args) {
    auto* field = CastField<FFloatProperty>(args.function->GetReturnProperty());
    require(field, "Breeding query result differs");
    return *field->ContainerPtrToValuePtr<float>(args.data.data());
}
class BreederProbe final : public CppUserModBase {
    std::atomic<bool> requested{};
    Hook::GlobalCallbackId tick{Hook::ERROR_ID};
    void inspect_production_inputs(UObject* model) {
        // Only the reflected eligibility query; never the native consuming helper.
        Parameters cake{find<UFunction*>(STR("/Script/Pal.PalMapObjectBreedFarmModel:CanConsumeBreedItem"))};
        model->ProcessEvent(cake.function, cake.data.data());
        auto* result = cake.function->GetReturnProperty();
        auto* item = CastField<FStructProperty>(cake.function->GetPropertyByName(STR("ConsumableItem")));
        require(result && result->GetElementSize() == 1 && item, "Breed item query layout differs");
        const auto status = *result->ContainerPtrToValuePtr<uint8_t>(cake.data.data());
        auto* item_data = item->ContainerPtrToValuePtr<void>(cake.data.data());
        auto* amount = CastField<FIntProperty>(item->GetStruct()->GetPropertyByName(STR("Num")));
        require(amount, "Breed item amount differs");
        const auto count = *amount->ContainerPtrToValuePtr<int32_t>(item_data);
        Output::send<LogLevel::Normal>(STR("[PRFBreederProbe] CAKE query-status={} requested-count={} query-only=true\n"), status, count);

        const auto container = ManualSettlement::model_container(model);
        for (const auto& slot : container.slots) {
            if (slot.state.count <= 0) continue;
            const auto name = slot.state.item.name;
            Output::send<LogLevel::Normal>(STR("[PRFBreederProbe] INVENTORY slot={} item={} count={}\n"),
                slot.id.index, FName{name.index, name.number}.ToString(), slot.state.count);
            const auto effect = native_breeder_effect(model, NativeName{name.index, name.number});
            if (!effect) continue;
            auto* type = find<UScriptStruct*>(STR("/Script/Pal.PalBreedingItemEffectData"));
            Output::send<LogLevel::Normal>(STR("[PRFBreederProbe] CAKE_EFFECT item={} breed-count={} talent-min={} talent-max={} mutation-percent={} rank-bonus={} inherit-active={} passive-override={} query-only=true\n"),
                FName{name.index,name.number}.ToString(), effect->field<int32_t>(type, STR("BreedCount")),
                effect->field<int32_t>(type, STR("TalentBonusMin")), effect->field<int32_t>(type, STR("TalentBonusMax")),
                effect->field<float>(type, STR("MutationRateBonusPercent")), effect->field<int32_t>(type, STR("CombiRankBonus")),
                effect->field<uint8_t>(type, STR("bInheritAllActiveSkills")), effect->field<int32_t>(type, STR("PassiveInheritCountOverride")));
        }
        auto* eggs = CastField<FArrayProperty>(model->GetPropertyByNameInChain(STR("SpawnedEggInstanceIds")));
        require(eggs && eggs->GetInner()->GetElementSize() == sizeof(st::Guid), "Spawned egg IDs layout differs");
        FScriptArrayHelper ids{eggs, eggs->ContainerPtrToValuePtr<void>(model)};
        Output::send<LogLevel::Normal>(STR("[PRFBreederProbe] WORLD_EGGS tracked={} capacity={}\n"),
            ids.Num(), read_field<int32_t>(model, STR("ExistPalEggMaxNum")));
        for (int32_t i = 0; i < ids.Num(); ++i) {
            std::array<uint32_t, 4> id{};
            std::memcpy(id.data(), ids.GetRawPtr(i), sizeof(id));
            Output::send<LogLevel::Normal>(STR("[PRFBreederProbe] EGG_ID index={} id={:08x}{:08x}{:08x}{:08x}\n"),
                i, id[0], id[1], id[2], id[3]);
        }
        // Read actual reflection metadata; do not initialize or generate offspring.
        for (const auto* path : {STR("/Script/Pal.PalIndividualCharacterSaveParameter"),
                                 STR("/Script/Pal.PalBreedingItemEffectData")}) {
            auto* type = find<UScriptStruct*>(path);
            Output::send<LogLevel::Normal>(STR("[PRFBreederProbe] STRUCT name={} size={}\n"), type->GetName(), type->GetStructureSize());
            if (type->GetName() == STR("PalBreedingItemEffectData")) {
                for (TFieldIterator<FProperty> it{type}; it; ++it)
                    Output::send<LogLevel::Normal>(STR("[PRFBreederProbe] EFFECT_FIELD name={} offset={} size={}\n"),
                        (*it)->GetName(), (*it)->GetOffset_Internal(), (*it)->GetElementSize());
            }
        }
    }
    void inspect() {
        inspect_ancient_blueprint();
        auto* utility = find<UObject*>(STR("/Script/Pal.Default__PalBreedingUtility"));
        std::vector<UObject*> models;
        UObjectGlobals::FindAllOf(STR("PalMapObjectBreedFarmModel"), models);
        int inspected{};
        for (auto* model : models) {
            if (!live(model) || !model->GetWorld() || read_bool_field(model, STR("bDisposed"))) continue;
            auto* pal = find<UObject*>(STR("/Script/Pal.Default__PalUtility"));
            if (!bool_call(pal, STR("/Script/Pal.PalUtility:IsServer"), model)) continue;
            if (bool_call(pal, STR("/Script/Pal.PalUtility:IsDedicatedServer"), model) ||
                bool_call(pal, STR("/Script/Pal.PalUtility:IsOpenListenServer"), model)) continue;
            auto* workee = object_call(model, STR("/Script/Pal.PalMapObjectConcreteModelBase:GetWorkeeModule"));
            if (!live(workee)) continue;
            auto* work = object_call(workee, STR("/Script/Pal.PalMapObjectWorkeeModule:GetWork"));
            if (!live(work)) continue;
            Parameters pair{find<UFunction*>(STR("/Script/Pal.PalBreedingUtility:CanProceedBreeding"))};
            set_object(pair, STR("Work"), work);
            utility->ProcessEvent(pair.function, pair.data.data());
            auto* pair_return = CastField<FBoolProperty>(pair.function->GetReturnProperty());
            require(pair_return, "Breeding eligibility result differs");
            const bool eligible = pair_return->GetPropertyValueInContainer(pair.data.data());

            Parameters assigned{find<UFunction*>(STR("/Script/Pal.PalWorkBase:GetAssignedCharacters"))};
            work->ProcessEvent(assigned.function, assigned.data.data());
            auto* slots = CastField<FArrayProperty>(assigned.function->GetPropertyByName(STR("IndividualSlots")));
            require(slots && CastField<FObjectProperty>(slots->GetInner()), "Breeding assigned slots differ");
            FScriptArrayHelper list{slots, slots->ContainerPtrToValuePtr<void>(assigned.data.data())};

            auto* base = object_call(model, STR("/Script/Pal.PalMapObjectConcreteModelBase:GetBaseCampModelBelongTo"));
            Parameters buff{find<UFunction*>(STR("/Script/Pal.PalBreedingUtility:CalcBreedBuffRate"))};
            set_object(buff, STR("Work"), work);
            set_object(buff, STR("BaseCampModel"), base);
            utility->ProcessEvent(buff.function, buff.data.data());
            const float rate = float_result(buff);
            const auto duration = read_field<float>(model, STR("BreedRequiredRealTime"));
            const auto progress = read_field<float>(model, STR("BreedProgressTime"));
            require(std::isfinite(duration) && duration > 0 && std::isfinite(rate) && rate > 0,
                "Invalid native breeding duration or buff");
            Output::send<LogLevel::Normal>(STR("[PRFBreederProbe] FARM={} assigned={} native-pair-eligible={} duration={} progress={} native-buff={} read-only=true\n"),
                model->GetName(), list.Num(), eligible, duration, progress, rate);
            inspect_production_inputs(model);
            ++inspected;
        }
        Output::send<LogLevel::Normal>(STR("[PRFBreederProbe] DONE farms={} cake-consumed=0 offspring-generated=0 parent-writes=0\n"), inspected);
    }
public:
    BreederProbe() {
        ModName = STR("PalResourceFactoryBreederProbe");
        ModVersion = STR("0.3.2-ancient-initialization-read-only");
        ModDescription = STR("Manual native breeding parameter check; no production behavior");
        ModAuthors = STR("Paulus");
    }
    ~BreederProbe() override { if (tick != Hook::ERROR_ID) static_cast<void>(Hook::UnregisterCallback(tick)); }
    void on_unreal_init() override {
        tick = Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>&, UEngine*, float, bool) {
                if (!requested.exchange(false)) return;
                try { inspect(); }
                catch (const std::exception& e) {
                    Output::send<LogLevel::Error>(STR("[PRFBreederProbe] REFUSED {}\n"), to_wstring(e.what()));
                }
            }, Hook::FCallbackOptions{false, false, ModName, STR("ManualBreedingReadback")});
        if (tick == Hook::ERROR_ID) return;
        register_keydown_event(Input::Key::F7, {Input::ModifierKey::CONTROL, Input::ModifierKey::ALT},
            [this] { requested.store(true); });
        Output::send<LogLevel::Normal>(STR("[PRFBreederProbe] READY version=0.3.2 Ctrl+Alt+F7 original-ancient-initialization-and-farm-read-only solo-only=true\n"));
    }
};
}
extern "C" {
__declspec(dllexport) RC::CppUserModBase* start_mod() { return new BreederProbe(); }
__declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }
}
