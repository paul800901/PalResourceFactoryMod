#include "SettlementRuntime.hpp"
#include "AncientBreederSettlement.hpp"
#include "AncientBreederInputGuard.hpp"
#include "ProcessorState.hpp"
#include "AncientConveyor.hpp"
#include <mutex>
#include "AncientBreederVisual.hpp"
#include "AncientBreederSkin.hpp"
#include "FacilityPower.hpp"
#include "BuildWorkBalance.hpp"

namespace {
#include "ProcessorNative.hpp" // Native conveyor duration/capacity; no processor hooks.
// Custom-ID frontend and completed eggs verified in game on 2026-09-07.
// Settlement remains a solo integration test, not a verified release.
constexpr bool ancient_production_enabled = true;

class AncientBreeder final : public CppUserModBase {
    struct Machine {
        FWeakObjectPtr model;
        prf::processor::State conveyor; // Incubation remains entirely native.
        std::unique_ptr<ancient_breeder::PendingSettlement> pending;
        int pending_slot{-1};
        std::string pending_egg;
        std::set<std::string> quarantined;
        bool paused{};
    };
    ancient_breeder::InputGuard guard;
    ancient_breeder::MenuVisual menu_visual;
    bool menu_visual_failed{};
    ancient_breeder::Skin skin;
    bool skin_failed{};
    bool work_balanced{};
    double balance_wait{};
    std::map<st::Guid, Machine> machines;
    std::vector<FWeakObjectPtr> candidates;
    std::mutex mutex;
    UClass* model_class{};
    Hook::GlobalCallbackId discovery{Hook::ERROR_ID}, inputs{Hook::ERROR_ID}, tick{Hook::ERROR_ID}, unload{Hook::ERROR_ID};
    Hook::GlobalCallbackId indicators{Hook::ERROR_ID};
    bool indicator_failed{};
    bool ready{}, checking_input{};
    double elapsed{};

    bool ours(UObject* object) {
        if (!live(object) || !object->GetWorld() || read_bool_field(object, STR("bDisposed"))) return false;
        const auto id = returned<NativeName>(object, STR("/Script/Pal.PalMapObjectConcreteModelBase:TryGetMapObjectId"));
        return FName{id.index, id.number}.ToString() == STR("PRF_ResourceBreedingFacility");
    }
    void discover() {
        std::vector<FWeakObjectPtr> values;
        { std::scoped_lock lock{mutex};
          std::erase_if(candidates, [](const auto& weak) { return weak.Get() == nullptr; });
          values = candidates; }
        auto* utility = find<UObject*>(STR("/Script/Pal.Default__PalUtility"));
        for (const auto& weak : values) {
            auto* model = weak.Get();
            if (!ours(model)) continue;
            // Placement creates a different actor from the B-menu preview.
            // Skin that actor during construction too, without admitting it to
            // production or touching its container/power/settlement state.
            if (!skin_failed) try { skin.refresh(model); }
            catch (const std::exception& e) {
                skin_failed=true;
                Output::send<LogLevel::Warning>(STR("[PRFAncient] SKIN_FAILED {} production-unchanged=true\n"),to_wstring(e.what()));
            }
            if (!facility_power::constructed(model)) continue;
            if (!bool_call(utility, STR("/Script/Pal.PalUtility:IsServer"), model) ||
                bool_call(utility, STR("/Script/Pal.PalUtility:IsDedicatedServer"), model) ||
                bool_call(utility, STR("/Script/Pal.PalUtility:IsOpenListenServer"), model)) continue;
            const auto instance = read_field<st::Guid>(model, STR("InstanceId"));
            if (instance == st::Guid{} || machines.contains(instance)) continue;
            auto* module = object_call(model, STR("/Script/Pal.PalMapObjectConcreteModelBase:GetItemContainerModule"));
            if (!live(module)) continue;
            auto* container = object_call(module, STR("/Script/Pal.PalMapObjectItemContainerModule:GetContainer"));
            if (!live(container) || read_field<st::Guid>(container, STR("ID")) == st::Guid{}) continue;
            auto& machine = machines[instance];
            machine.model = weak;
            try {
                guard.attach(model); // Before any claim or native butcher call.
                const auto source = ManualSettlement::read_container(container);
                menu_visual.attach(model,source.id);
                require(!source.slots.empty() && source.slots.size() <= machine.conveyor.jobs.size(),
                    "Ancient egg capacity exceeds the existing conveyor adapter");
                Output::send<LogLevel::Warning>(STR("[PRFAncient] ATTACHED egg-slots={} native-breeding=true native-incubation=true settlement-enabled={} solo-test=true\n"), source.slots.size(), ancient_production_enabled);
            } catch (const std::exception& e) {
                machine.paused = true;
                Output::send<LogLevel::Error>(STR("[PRFAncient] ATTACH_REFUSED {}\n"), to_wstring(e.what()));
            }
        }
    }
    void update(double seconds) {
        auto* utility = find<UObject*>(STR("/Script/Pal.Default__PalUtility"));
        for (auto it = machines.begin(); it != machines.end();) {
            auto& machine = it->second;
            auto* model = machine.model.Get();
            if (!live(model) || read_bool_field(model, STR("bDisposed"))) { it = machines.erase(it); continue; }
            ++it;
            if (machine.paused) continue;
            try {
                if (bool_call(find<UObject*>(STR("/Script/Engine.Default__GameplayStatics")),
                    STR("/Script/Engine.GameplayStatics:IsGamePaused"), model)) continue;
                auto* save = object_call(utility, STR("/Script/Pal.PalUtility:GetSaveGameManager"), model);
                if (!live(save) || bool_call(save, STR("/Script/Pal.PalSaveGameManager:IsWorldAutoSaving"))) continue;
                const auto power_source = ManualSettlement::model_container(model);
                const auto power_completed = ancient_breeder::completed_slots(model);
                bool incubating = false;
                for (size_t i = 0; i < power_source.slots.size(); ++i)
                    if (power_source.slots[i].state.count > 0 &&
                        std::find(power_completed.begin(), power_completed.end(), static_cast<int>(i)) == power_completed.end())
                        incubating = true;
                if (!facility_power::update(model, incubating ? 1000.0f : 500.0f)) continue;
                if (machine.pending) {
                    // A disk write may span arbitrarily many ticks. Do not
                    // readmit the consumed egg from the pre-poll snapshot or
                    // treat native slot reuse as replacement of a locked job.
                    if (!machine.pending->poll()) continue;
                    if (machine.pending->is_quarantined()) machine.quarantined.insert(machine.pending_egg);
                    if (machine.conveyor.jobs[machine.pending_slot].egg == machine.pending_egg)
                        machine.conveyor.empty(machine.pending_slot);
                    machine.pending.reset();
                    continue; // Next tick reads post-settlement native state.
                }
                const auto& source = power_source;
                require(source.slots.size() <= machine.conveyor.jobs.size(), "Ancient egg capacity changed");
                const auto& completed_slots = power_completed;
                std::array<std::string, 54> current{};
                for (size_t i = 0; i < source.slots.size(); ++i)
                    if (source.slots[i].state.count > 0)
                        current[i] = egg_key(source.slots[i].state.item.dynamic);
                ancient_breeder::reconcile_conveyor(machine.conveyor, current);
                // Admission happens only after the native completion save exists.
                // Existing jobs do not allocate transient parameters each frame.
                for (size_t i = 0; i < source.slots.size(); ++i) {
                    const auto& slot = source.slots[i];
                    if (slot.state.count == 0) { machine.conveyor.empty(i); continue; }
                    const auto key = egg_key(slot.state.item.dynamic);
                    if (machine.quarantined.contains(key)) continue;
                    auto& job = machine.conveyor.jobs[i];
                    if (job.egg == key) continue;
                    if (std::find(completed_slots.begin(), completed_slots.end(), static_cast<int>(i)) == completed_slots.end()) continue;
                    auto finished = ancient_breeder::completed(model, static_cast<int>(i));
                    if (finished) machine.conveyor.inserted(i, key, 0);
                }
                const auto due = machine.conveyor.advance(seconds, disassembly_interval(), disassembly_capacity());
                if (due && !machine.pending) {
                    const auto& slot = source.slots.at(*due);
                    machine.pending_slot = static_cast<int>(*due);
                    machine.pending_egg = machine.conveyor.jobs[*due].egg;
                    machine.pending = std::make_unique<ancient_breeder::PendingSettlement>(model,
                        machine.pending_slot, slot.state.item.dynamic);
                }
            } catch (const std::exception& e) {
                machine.paused = true;
                machine.pending.reset(); // Join disk-only write; never retry RNG/apply.
                Output::send<LogLevel::Error>(STR("[PRFAncient] PAUSED {} no-replay=true\n"), to_wstring(e.what()));
            }
        }
    }
public:
    AncientBreeder() {
        ModName = STR("PalResourceFactoryAncientBreeder");
        ModVersion = STR("0.1.17-slot-lifecycle");
        ModDescription = STR("Native ancient breeding and incubation with closed offspring settlement; solo test only");
        ModAuthors = STR("Paulus");
    }
    ~AncientBreeder() override {
        for (auto id : {discovery, inputs, tick, unload, indicators}) if (id != Hook::ERROR_ID) static_cast<void>(Hook::UnregisterCallback(id));
        machines.clear();
    }
    void on_unreal_init() override {
        try {
            SettlementEntries native;
            model_class = find<UClass*>(STR("/Script/Pal.PalMapObjectMultiHatchingEggWithBreedModel"));
            guard.initialize();
            indicators = Hook::RegisterProcessEventPostCallback(
                [this](Hook::TCallbackIterationData<void>&, UObject* context, UFunction* fn, void* params) {
                    if (!skin_failed) try { skin.after_event(context,fn); }
                    catch (const std::exception& e) { skin_failed=true; Output::send<LogLevel::Warning>(STR("[PRFAncient] SKIN_FAILED {}\n"),to_wstring(e.what())); }
                    if (indicator_failed) return;
                    try { guard.hide_egg_indicator(context, fn, params); }
                    catch (const std::exception& e) {
                        indicator_failed = true;
                        Output::send<LogLevel::Warning>(STR("[PRFAncient] INDICATOR_HIDE_FAILED {} input-lock-unchanged=true\n"), to_wstring(e.what()));
                    }
                }, Hook::FCallbackOptions{false, false, ModName, STR("AncientEggIndicator")});
            require(indicators != Hook::ERROR_ID, "Ancient indicator hook unavailable");
            inputs = Hook::RegisterProcessEventPreCallback(
                [this](Hook::TCallbackIterationData<void>& call, UObject* context, UFunction* fn, void* params) {
                    if (checking_input) return;
                    checking_input = true;
                    struct Reset { bool& flag; ~Reset() { flag = false; } } reset{checking_input};
                    if (!skin_failed) try { skin.on_event(context,fn); }
                    catch (const std::exception& e) {
                        skin_failed=true;
                        Output::send<LogLevel::Warning>(STR("[PRFAncient] SKIN_FAILED {}\n"),to_wstring(e.what()));
                    }
                    try {
                        if (guard.reject(context, fn, params)) {
                            call.PreventOriginalFunctionCall();
                            Output::send<LogLevel::Normal>(STR("[PRFAncient] INPUT_LOCKED {}\n"), fn->GetName());
                        }
                    } catch (const std::exception& e) {
                        // Runtime verification is required before installation:
                        // an unknown request must not globally block other mods.
                        ready = false;
                        Output::send<LogLevel::Error>(STR("[PRFAncient] INPUT_GUARD_FAILED {} production-disabled=true\n"), to_wstring(e.what()));
                    }
                }, Hook::FCallbackOptions{false, false, ModName, STR("AncientClosedOutput")});
            require(inputs != Hook::ERROR_ID, "Ancient input guard hook unavailable");
            discovery = Hook::RegisterStaticConstructObjectPostCallback(
                [this](Hook::TCallbackIterationData<UObject*>& call, const FStaticConstructObjectParameters& params) {
                    if (params.Class && params.Class->GetName() == STR("WBP_IngameMenu_Incubator_Multiple_C"))
                        if (auto* widget = call.GetCurrentResolvedReturnValue()) menu_visual.add(widget);
                    // The original ancient breeder uses a Blueprint-generated
                    // model subclass. Exact-class comparison misses models born
                    // after startup and after LoadMap clears the candidate list.
                    if (!params.Class || !params.Class->IsChildOf(model_class)) return;
                    if (auto* object = call.GetCurrentResolvedReturnValue()) {
                        std::scoped_lock lock{mutex}; candidates.emplace_back(object);
                    }
                }, Hook::FCallbackOptions{false, true, ModName, STR("AncientDiscovery")});
            require(discovery != Hook::ERROR_ID, "Ancient discovery hook unavailable");
            std::vector<UObject*> existing;
            // UE4SS FindAllOf includes the superclass chain (verified in its
            // current implementation); no second whole-world scan is needed.
            UObjectGlobals::FindAllOf(STR("PalMapObjectMultiHatchingEggWithBreedModel"), existing);
            { std::scoped_lock lock{mutex}; for (auto* object : existing) if (live(object)) candidates.emplace_back(object); }
            tick = Hook::RegisterEngineTickPostCallback(
                [this](Hook::TCallbackIterationData<void>&, UEngine*, float delta, bool idle) {
                    if (!ready || idle || !std::isfinite(delta) || delta <= 0) return;
                    elapsed += std::min(double(delta), 0.25);
                    if (elapsed < 0.1) return;
                    const auto step = std::exchange(elapsed, 0.0);
                    balance_wait += step;
                    if (!work_balanced && balance_wait>=3.0) try { balance_wait=0; work_balanced=apply_build_work_balance(); }
                    catch (const std::exception& e) {
                        Output::send<LogLevel::Warning>(STR("[PRFBalance] BUILD_WORK_FAILED {}\n"),to_wstring(e.what()));
                        work_balanced=true;
                    }
                    if (!skin_failed) try { skin.refresh_states(); }
                    catch (const std::exception& e) { skin_failed=true; Output::send<LogLevel::Warning>(STR("[PRFAncient] SKIN_FAILED {}\n"),to_wstring(e.what())); }
                    if (!menu_visual_failed) try { menu_visual.refresh(); }
                    catch (const std::exception& e) {
                        menu_visual_failed = true;
                        Output::send<LogLevel::Warning>(STR("[PRFAncient] MENU_VISUAL_FAILED {} server-guard-unchanged=true\n"), to_wstring(e.what()));
                    }
                    try {
                        discover();
                        if constexpr (ancient_production_enabled) update(step);
                    }
                    catch (const std::exception& e) {
                        ready = false;
                        Output::send<LogLevel::Error>(STR("[PRFAncient] DISABLED {}\n"), to_wstring(e.what()));
                    }
                }, Hook::FCallbackOptions{false, false, ModName, STR("AncientSettlementClock")});
            require(tick != Hook::ERROR_ID, "Ancient clock unavailable");
            unload = Hook::RegisterLoadMapPreCallback(
                [this](Hook::TCallbackIterationData<bool>&, UEngine*, FWorldContext&, FURL, UPendingNetGame*, FString&) {
                    machines.clear(); guard.clear(); menu_visual.clear(); menu_visual_failed = false; skin.clear(); skin_failed=false; indicator_failed=false; work_balanced=false; elapsed = 0;
                    std::scoped_lock lock{mutex}; candidates.clear();
                }, Hook::FCallbackOptions{false, false, ModName, STR("AncientUnload")});
            require(unload != Hook::ERROR_ID, "Ancient unload hook unavailable");
            ready = true;
            Output::send<LogLevel::Warning>(STR("[PRFAncient] READY version=0.1.17-slot-lifecycle scope=PRF_ResourceBreedingFacility settlement-enabled={} power=500/1000 game-verified=false\n"), ancient_production_enabled);
        } catch (const std::exception& e) {
            Output::send<LogLevel::Error>(STR("[PRFAncient] START_REFUSED {}\n"), to_wstring(e.what()));
        }
    }
};
} // namespace
extern "C" {
__declspec(dllexport) RC::CppUserModBase* start_mod() { return new AncientBreeder(); }
__declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }
}
