#include "SettlementRuntime.hpp"

namespace {

class DirectSettlementProbe final : public CppUserModBase {
    std::atomic<bool> requested{false};
    bool native_called{false};
    Hook::GlobalCallbackId tick{Hook::ERROR_ID};

    void preview() {
#if !defined(PRF_MANUAL_SETTLEMENT_TEST)
        require(!native_called, "One native sample per game session; restart before another sample");
#endif
        const auto calculate = resolve_native();
        auto* utility = find<UObject*>(STR("/Script/Pal.Default__PalUtility"));
        std::vector<UObject*> models;
        UObjectGlobals::FindAllOf(STR("PalMapObjectHatchingEggModel"), models);
        UObject* selected = nullptr;
        for (auto* model : models) {
            if (!live(model) || !model->GetWorld()) continue;
            auto* disposed = CastField<FBoolProperty>(model->GetPropertyByNameInChain(STR("bDisposed")));
            if (disposed && disposed->GetPropertyValueInContainer(model)) continue;
            if (!bool_call(utility, STR("/Script/Pal.PalUtility:IsServer"), model)) continue;
            // Original single-incubator completion writes this save at model+0x270
            // (0x3030160); its claim entry 0x302cf90 checks the same nonempty name.
            auto* candidate = CastField<FStructProperty>(model->GetPropertyByNameInChain(STR("HatchedCharacterSaveParameter")));
            require(candidate && candidate->GetOffset_Internal() == 0x270 &&
                candidate->GetStruct()->GetStructureSize() == 0x370, "Single incubator layout differs from native evidence");
            auto* name = candidate->GetStruct()->GetPropertyByName(STR("CharacterID"));
            require(name && name->GetOffset_Internal() == 0 && name->GetElementSize() == 8,
                "Single incubator readiness field differs");
            NativeName ready{};
            std::memcpy(&ready, candidate->ContainerPtrToValuePtr<void>(model), sizeof(ready));
            if (ready.index == 0) continue;
            require(selected == nullptr, "More than one completed single incubator; use a test world with just one");
            selected = model;
        }
        require(selected != nullptr, "No completed original single incubator found; do not claim the egg");
        auto* source = CastField<FStructProperty>(selected->GetPropertyByNameInChain(STR("HatchedCharacterSaveParameter")));
        require(source != nullptr, "Incubator offspring save parameter unavailable");
        auto* structure = source->GetStruct().Get();
        auto* character = structure->GetPropertyByName(STR("CharacterID"));
        auto* level = structure->GetPropertyByName(STR("Level"));
        require(character && level && character->GetElementSize() == 8 && level->GetElementSize() == 1 &&
            structure->GetStructureSize() == 0x370, "Offspring save layout differs from native evidence");
        void* save = source->ContainerPtrToValuePtr<void>(selected);
        NativeName id{};
        std::memcpy(&id, character->ContainerPtrToValuePtr<void>(save), sizeof(id));
        require(id.index != 0, "Egg has no resolved offspring data yet; no claim or generation was attempted");

        auto* database = object_call(utility, STR("/Script/Pal.PalUtility:GetDatabaseCharacterParameter"), selected);
        require(live(database) && GMalloc && *GMalloc, "Native database or engine allocator unavailable");
#if defined(PRF_MANUAL_SETTLEMENT_TEST)
        ManualSettlement transaction{selected};
        if (transaction.load_or_claim()) {
            transaction.attempt();
            return;
        }
#endif
        auto* parameter_class = find<UClass*>(STR("/Script/Pal.PalIndividualCharacterParameter"));
        Output::send<LogLevel::Normal>(STR("[PRFProbe] stage=construct-transient character={} level={}\n"),
            FName{id.index, id.number}.ToString(), *level->ContainerPtrToValuePtr<uint8_t>(save));
        RootedTemporary temporary{parameter_class, reinterpret_cast<UObject*>(selected->GetWorld())};
        auto* destination = CastField<FStructProperty>(temporary.object->GetPropertyByNameInChain(STR("SaveParameter")));
        auto* actor = CastField<FObjectProperty>(temporary.object->GetPropertyByNameInChain(STR("IndividualActor")));
        require(destination && actor && destination->GetStruct().Get() == structure &&
            destination->GetOffset_Internal() + character->GetOffset_Internal() == 0x3d0 &&
            destination->GetOffset_Internal() + level->GetOffset_Internal() == 0x3f0 &&
            actor->GetOffset_Internal() == 0x318, "Temporary individual layout differs from native getters");
        require(*actor->ContainerPtrToValuePtr<UObject*>(temporary.object) == nullptr,
            "Temporary parameter unexpectedly references an actor");
        structure->CopyScriptStruct(destination->ContainerPtrToValuePtr<void>(temporary.object), save);

        NativeLocation location{};
        auto* location_fn = find<UFunction*>(STR("/Script/Pal.PalMapObjectConcreteModelBase:GetMapObjectLocation"));
        Parameters location_params{location_fn};
        auto* out = CastField<FStructProperty>(location_fn->GetPropertyByName(STR("outVector")));
        require(out && out->GetElementSize() == sizeof(location), "Map object location ABI differs");
        selected->ProcessEvent(location_fn, location_params.data.data());
        std::memcpy(&location, out->ContainerPtrToValuePtr<void>(location_params.data.data()), sizeof(location));

        NativeDrops drops{};
        // No persistent object handle or world actor is created. The engine owns array allocation.
        struct ReleaseDrops {
            NativeDrops& drops;
            ~ReleaseDrops() { if (drops.data) (*GMalloc)->Free(drops.data); }
        } release{drops};
        native_called = true;
        Output::send<LogLevel::Warning>(STR("[PRFProbe] stage=native-call egg-removal=false item-award=false\n"));
        calculate(database, &drops, temporary.object, &location, nullptr);
        require(drops.num >= 0 && drops.capacity >= drops.num && (drops.num == 0 || drops.data),
            "Native output array is invalid");
        Output::send<LogLevel::Normal>(STR("[PRFProbe] stage=native-return drop-rows={}\n"), drops.num);
        for (int32_t i = 0; i < drops.num; ++i) {
            const auto& row = drops.data[i];
            Output::send<LogLevel::Normal>(STR("[PRFProbe] drop item={} count={}\n"),
                FName{row.item.index, row.item.number}.ToString(), row.count);
        }
#if defined(PRF_MANUAL_SETTLEMENT_TEST)
        transaction.save_result(drops);
        transaction.attempt();
#endif
    }

public:
    DirectSettlementProbe() {
#if defined(PRF_MANUAL_SETTLEMENT_TEST)
        ModName = STR("PalResourceFactorySettlementTest");
        ModVersion = STR("0.2.0-manual-ground-test");
        ModDescription = STR("Manual single-egg native ground-drop test; NOT the full facility mod");
#else
        ModName = STR("PalResourceFactoryProbe");
        ModVersion = STR("0.1.0-manual-preview");
        ModDescription = STR("Single-egg native butcher preview; never consumes or awards");
#endif
        ModAuthors = STR("Paulus");
    }
    ~DirectSettlementProbe() override {
        if (tick != Hook::ERROR_ID) static_cast<void>(Hook::UnregisterCallback(tick));
    }
    void on_unreal_init() override {
        // The input callback only sets a flag. All UObject work is done on the engine thread.
        tick = Hook::RegisterEngineTickPostCallback(
            [this](Hook::TCallbackIterationData<void>&, UEngine*, float, bool) {
                if (!requested.exchange(false)) return;
                try {
                    preview();
#if !defined(PRF_MANUAL_SETTLEMENT_TEST)
                    // Locals, engine-allocated output and temporary root have been released.
                    Output::send<LogLevel::Warning>(STR("[PRFProbe] PREVIEW_OK original-egg-retained=true materials-awarded=0; RNG sampled once.\n"));
#endif
                }
                catch (const std::exception& e) {
                    Output::send<LogLevel::Error>(STR("[{}] REFUSED {}\n"), ModName, to_wstring(e.what()));
                }
            }, Hook::FCallbackOptions{false, false, ModName, STR("ManualSingleEgg")});
        if (tick == Hook::ERROR_ID) {
            Output::send<LogLevel::Error>(STR("[PRFProbe] DISABLED game-thread callback unavailable\n"));
            return;
        }
#if defined(PRF_MANUAL_SETTLEMENT_TEST)
        register_keydown_event(Input::Key::F9, {Input::ModifierKey::CONTROL, Input::ModifierKey::ALT},
            [this]() { requested.store(true); });
        Output::send<LogLevel::Warning>(STR("[PRFSettlement] READY version=0.2.0-manual-ground-test Ctrl+Alt+F9 CONSUMES one completed single-incubator egg; output=vanilla-ground; disposable solo world ONLY. No auto processing.\n"));
#else
        register_keydown_event(Input::Key::F8, {Input::ModifierKey::CONTROL, Input::ModifierKey::ALT},
            [this]() { requested.store(true); });
        Output::send<LogLevel::Warning>(STR("[PRFProbe] READY Ctrl+Alt+F8; original single incubator only; manual preview, NOT full mod.\n"));
#endif
    }
};
} // namespace

extern "C" {
__declspec(dllexport) RC::CppUserModBase* start_mod() { return new DirectSettlementProbe(); }
__declspec(dllexport) void uninstall_mod(RC::CppUserModBase* mod) { delete mod; }
}
