#pragma once
// Source-only egg removal and original ground delivery, shared with the
// automatic processor. No quarantined facility runtime is linked.
namespace st = prf::settlement;

template<class T> T read_field(UObject* object, const wchar_t* name, int expected_offset = -1) {
    require(live(object), "Invalid object while reading settlement state");
    auto* property = object->GetPropertyByNameInChain(name);
    require(property && property->GetElementSize() == sizeof(T) &&
        (expected_offset < 0 || property->GetOffset_Internal() == expected_offset), "Settlement field layout differs");
    T value{};
    std::memcpy(&value, property->ContainerPtrToValuePtr<void>(object), sizeof(value));
    return value;
}

inline bool read_bool_field(UObject* object, const wchar_t* name) {
    require(live(object), "Invalid object while reading boolean field");
    auto* property = CastField<FBoolProperty>(object->GetPropertyByNameInChain(name));
    require(property, "Settlement boolean field unavailable");
    return property->GetPropertyValueInContainer(object);
}

template<class T> st::Array<T> view(std::vector<T>& values) {
    require(values.size() <= INT32_MAX, "Native array count overflow");
    return {values.data(), static_cast<int32_t>(values.size()), static_cast<int32_t>(values.size())};
}
template<class T> struct OwnedNativeArray {
    st::Array<T> array{};
    ~OwnedNativeArray() { if (array.data) (*GMalloc)->Free(array.data); }
    OwnedNativeArray() = default;
    OwnedNativeArray(const OwnedNativeArray&) = delete;
    void validate() const {
        require(array.num >= 0 && array.capacity >= array.num && (array.num == 0 || array.data), "Invalid native planning output");
    }
};

struct SettlementEntries {
    using Plan = uint8_t (*)(UObject*, const st::Guid*, const st::Array<st::ItemState>*,
        const st::Array<st::Consume>*, st::Array<st::Change>*, st::Array<st::ItemCount>*);
    using Apply = void (*)(UObject*, const st::Array<st::Change>*, uint8_t);
    using Access = bool (*)(UObject*, const st::Guid*, const void*);
    using DisposeDynamic = void (*)(UObject*, const st::Array<st::DynamicId>*);
    // v1.0.4 vanilla converter 0x3006f20 calls this six-argument helper after
    // GetMeatCutItemInfo_withProbability. FName is passed BY VALUE in RDX.
    // It owns its temporary item containers / async drop-model requests.
    using SpawnGround = void (*)(UObject*, NativeName, int32_t,
        const NativeLocation*, const NativeLocation*, bool);
    Plan plan{};
    Apply apply{};
    Access access{};
    DisposeDynamic dispose_dynamic{};
    SpawnGround spawn_ground{};
    const void* options{};
    SettlementEntries() {
        // Also enforces the original successful preview's PE/version signature.
        static_cast<void>(resolve_native());
        auto* base = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
        auto entry = [&](size_t rva, std::array<uint8_t, 16> bytes) {
            require(std::memcmp(base + rva, bytes.data(), bytes.size()) == 0, "Settlement native entry differs; disabled");
            return base + rva;
        };
        plan = reinterpret_cast<Plan>(entry(0x2fc0ff0,
            {0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0x6c}));
        apply = reinterpret_cast<Apply>(entry(0x2fb5680,
            {0x48,0x89,0x5c,0x24,0x08,0x44,0x88,0x44,0x24,0x18,0x55,0x56,0x57,0x41,0x54,0x41}));
        access = reinterpret_cast<Access>(entry(0x2fafbf0,
            {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x6c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x57}));
        dispose_dynamic = reinterpret_cast<DisposeDynamic>(entry(0x2ed8c80,
            {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x30,0x48}));
        spawn_ground = reinterpret_cast<SpawnGround>(entry(0x2fa5ed0,
            {0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x18,0x55,0x57,0x41,0x54,0x41,0x56}));
        // Immutable default options passed by v1.0.4 incubator claim (0x303d3ad).
        options = base + 0x940ca10;
    }
};

inline std::filesystem::path settlement_directory() {
    HMODULE own{};
    require(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&settlement_directory), &own), "Cannot locate settlement test module");
    std::vector<wchar_t> buffer(32768);
    const auto length = GetModuleFileNameW(own, buffer.data(), static_cast<DWORD>(buffer.size()));
    require(length > 0 && length < buffer.size(), "Cannot locate settlement receipt directory");
    return std::filesystem::path{std::wstring{buffer.data(), length}}.parent_path().parent_path() / "settlements";
}

inline std::string egg_key(const st::DynamicId& id) {
    require(id.local != st::Guid{}, "Egg has no stable dynamic identity");
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (auto part : {id.world, id.local}) for (auto word : part.words) out << std::setw(8) << word;
    return out.str();
}

// One append-only receipt per original dynamic egg ID. No edits to game saves.
// A partial write, interrupted RNG, or interrupted apply refuses further replay.
class ReceiptFile {
    HANDLE file{INVALID_HANDLE_VALUE};
public:
    std::string key;
    std::filesystem::path path;
    explicit ReceiptFile(const st::DynamicId& id) : key(egg_key(id)), path(settlement_directory() / (key + ".txt")) {}
    ~ReceiptFile() { if (file != INVALID_HANDLE_VALUE) CloseHandle(file); }
    ReceiptFile(const ReceiptFile&) = delete;
    std::optional<st::Receipt> load() {
        if (!std::filesystem::exists(path)) return std::nullopt;
        file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        require(file != INVALID_HANDLE_VALUE, "Settlement receipt is in use or cannot be read");
        LARGE_INTEGER length{};
        require(GetFileSizeEx(file, &length) && length.QuadPart > 0 && length.QuadPart < INT32_MAX,
            "Invalid settlement receipt length");
        std::string text(static_cast<size_t>(length.QuadPart), '\0');
        DWORD read{};
        require(ReadFile(file, text.data(), static_cast<DWORD>(text.size()), &read, nullptr) && read == text.size(),
            "Incomplete settlement receipt read");
        return st::parse_receipt(text, key);
    }
    void create_claim() {
        require(file == INVALID_HANDLE_VALUE, "Settlement was already claimed");
        std::filesystem::create_directories(path.parent_path());
        file = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
        require(file != INVALID_HANDLE_VALUE, "Cannot create exclusive settlement RNG claim");
        append("PRF_SETTLEMENT_1\n\"" + key + "\"\nRESOLVING\n");
    }
    void append(const std::string& text) {
        require(file != INVALID_HANDLE_VALUE && text.size() <= MAXDWORD, "Receipt is not writable");
        DWORD written{};
        require(WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
            written == text.size() && FlushFileBuffers(file), "Settlement receipt flush failed; no replay");
    }
};

class ManualSettlement {
public:
    struct Slot {
        UObject* object{};
        st::SlotId id{};
        st::ItemState state{};
    };
    struct Container { UObject* object{}; st::Guid id{}; std::vector<Slot> slots; };
private:
    bool original_single;
    bool ancient_breeder;
    UObject* model;
    UObject* utility;
    UObject* manager;
    UObject* save_manager;
    UObject* dynamic_subsystem;
    Container source;
    Slot egg;
    SettlementEntries native;
    ReceiptFile receipt;
    std::vector<st::SavedDrop> saved;
    bool ready{};
    std::optional<st::Phase> ancient_prior_phase;
    std::vector<FWeakObjectPtr> lifetime;

public:
    static Slot read_slot(UObject* object) {
        Slot result;
        result.object = object;
        result.id.container = read_field<st::Guid>(object, STR("ContainerId"), 0x11c);
        result.id.index = read_field<int32_t>(object, STR("SlotIndex"), 0x118);
        result.state.item = read_field<st::ItemId>(object, STR("ItemId"), 0x12c);
        result.state.count = read_field<int32_t>(object, STR("StackCount"), 0x154);
        result.state.corruption = read_field<float>(object, STR("CorruptionProgressValue"), 0x158);
        return result;
    }
    static Container read_container(UObject* object) {
        Container result;
        result.object = object;
        result.id = read_field<st::Guid>(object, STR("ID"), 0x38);
        require(result.id != st::Guid{}, "Container has no saved identity");
        auto* slots = CastField<FArrayProperty>(object->GetPropertyByNameInChain(STR("ItemSlotArray")));
        require(slots && slots->GetOffset_Internal() == 0x70 && CastField<FObjectProperty>(slots->GetInner()),
            "Native container slot array differs");
        FScriptArrayHelper values{slots, slots->ContainerPtrToValuePtr<void>(object)};
        for (int32_t i = 0; i < values.Num(); ++i) {
            auto value = read_slot(*reinterpret_cast<UObject**>(values.GetRawPtr(i)));
            require(value.id.container == result.id && value.id.index == i, "Container slot identity mismatch");
            result.slots.push_back(value);
        }
        return result;
    }
    static Container model_container(UObject* selected) {
        auto* module = object_call(selected, STR("/Script/Pal.PalMapObjectConcreteModelBase:GetItemContainerModule"));
        require(live(module), "Selected model has no item container module");
        auto* container = object_call(module, STR("/Script/Pal.PalMapObjectItemContainerModule:GetContainer"));
        return read_container(container);
    }
    static UObject* world_dynamic_subsystem(UObject* selected) {
        std::vector<UObject*> objects;
        UObjectGlobals::FindAllOf(STR("PalDynamicItemWorldSubsystem"), objects);
        UObject* result{};
        for (auto* object : objects) if (live(object) && object->GetWorld() == selected->GetWorld()) {
            require(!result, "Ambiguous dynamic item world subsystem");
            result = object;
        }
        require(result, "Dynamic item world subsystem unavailable");
        return result;
    }
private:
    void check_context() {
        for (const auto& object : lifetime) require(object.Get() != nullptr, "Settlement object expired; replay forbidden");
        require(live(model) && model->GetWorld(), "Test world was unloaded");
        require(bool_call(utility, STR("/Script/Pal.PalUtility:IsServer"), model) &&
            !bool_call(utility, STR("/Script/Pal.PalUtility:IsDedicatedServer"), model) &&
            !bool_call(utility, STR("/Script/Pal.PalUtility:IsOpenListenServer"), model),
            "Manual settlement test is restricted to a local single-player world");
        require(live(manager) && live(save_manager) && live(dynamic_subsystem), "Settlement subsystem unavailable");
        // The inspected apply defers container notifications while this native
        // batch flag is set. Do not consume an egg before its reset can run.
        require(*reinterpret_cast<const uint8_t*>(reinterpret_cast<const std::byte*>(manager) + 0x2d8) == 0,
            "Original container manager is batching notifications; retry later");
        require(!bool_call(save_manager, STR("/Script/Pal.PalSaveGameManager:IsWorldAutoSaving")),
            "World save in progress; retry after it finishes");
        require(!read_bool_field(source.object, STR("bIgnoreOnSave")), "Egg container is not persistent");
        const auto current = read_slot(egg.object);
        require(current.id == egg.id && current.state.item == egg.state.item && current.state.count == 1,
            "Selected egg changed; no settlement");
        if (original_single) {
            auto* hatched = read_field<UObject*>(model, STR("HatchedPalEggData"), 0x5f8);
            require(live(hatched) && read_field<st::DynamicId>(hatched, STR("ID")) == egg.state.item.dynamic,
                "Completed offspring does not belong to the selected egg");
        } else {
            auto* fn = find<UFunction*>(STR("/Script/Pal.PalMapObjectConcreteModelBase:TryGetMapObjectId"));
            Parameters args{fn};
            auto* ret = fn->GetReturnProperty();
            require(ret && ret->GetElementSize() == 8, "Map object ID return layout differs");
            model->ProcessEvent(fn, args.data.data());
            NativeName id{};
            std::memcpy(&id, ret->ContainerPtrToValuePtr<void>(args.data.data()), 8);
            require(FName{id.index, id.number}.ToString() == (ancient_breeder
                ? STR("PRF_ResourceBreedingFacility") : STR("PRF_EggResourceProcessor")),
                "Automatic settlement building identity differs");
            if (ancient_breeder) require(model->IsA(find<UClass*>(STR("/Script/Pal.PalMapObjectMultiHatchingEggWithBreedModel"))),
                "Breeder settlement requires the native ancient model");
            require(!read_bool_field(model, STR("bDisposed")), "Processor was demolished");
        }
        require(native.access(manager, &source.id, native.options), "Original egg container access check refused");
    }
    NativeLocation ground_outlet() {
        auto* map_manager = object_call(utility, STR("/Script/Pal.PalUtility:GetMapObjectManager"), model);
        require(live(map_manager), "Original drop map-object manager unavailable");
        static_cast<void>(find<UClass*>(STR("/Script/Pal.PalMapObjectDropItemModel")));
        static_cast<void>(find<UClass*>(STR("/Script/Pal.PalMapObjectModelInitializeExtraParameterDropItem")));
        auto* fn = find<UFunction*>(STR("/Script/Pal.PalMapObjectConcreteModelBase:GetTransform"));
        auto* ret = CastField<FStructProperty>(fn->GetReturnProperty());
        require(ret && ret->GetElementSize() == 96, "Machine transform layout differs");
        auto* rotation = ret->GetStruct()->GetPropertyByName(STR("Rotation"));
        auto* translation = ret->GetStruct()->GetPropertyByName(STR("Translation"));
        require(rotation && rotation->GetElementSize() == 32 && translation && translation->GetElementSize() == 24,
            "Machine transform fields differ");
        Parameters params{fn};
        model->ProcessEvent(fn, params.data.data());
        auto* transform = ret->ContainerPtrToValuePtr<void>(params.data.data());
        std::array<double, 4> q{};
        std::array<double, 3> origin{};
        std::memcpy(q.data(), rotation->ContainerPtrToValuePtr<void>(transform), sizeof(q));
        std::memcpy(origin.data(), translation->ContainerPtrToValuePtr<void>(transform), sizeof(origin));
        const auto point = st::front_outlet(origin, q);
        return {point[0], point[1], point[2]};
    }
    st::Counts material_counts() {
        st::Counts result;
        auto* item_manager = object_call(utility, STR("/Script/Pal.PalUtility:GetItemIDManager"), model);
        require(live(item_manager), "Static item database unavailable");
        auto* fn = find<UFunction*>(STR("/Script/Pal.PalItemIDManager:GetStaticItemData"));
        auto* arg = fn->GetPropertyByName(STR("StaticItemId"));
        auto* ret = CastField<FObjectProperty>(fn->GetReturnProperty());
        require(arg && arg->GetElementSize() == 8 && ret, "Static item lookup layout differs");
        for (const auto& row : saved) {
            const auto wide = to_wstring(row.item);
            FName name{wide.c_str(), FNAME_Find};
            require(!name.IsNone(), "Saved material name is unavailable in this game; no substitution");
            const st::Name native_name{name.GetComparisonIndex().ToUnstableInt(), static_cast<uint32_t>(name.GetNumber())};
            Parameters params{fn};
            std::memcpy(arg->ContainerPtrToValuePtr<void>(params.data.data()), &native_name, 8);
            item_manager->ProcessEvent(fn, params.data.data());
            auto* item = *ret->ContainerPtrToValuePtr<UObject*>(params.data.data());
            require(live(item), "Original static material data unavailable");
            require(!bool_call(item, STR("/Script/Pal.PalStaticItemDataBase:HasDynamicItemClass")),
                "Drop needs native dynamic-item creation; this test leaves the egg and fixed result pending");
            st::add_count(result, native_name, row.count);
        }
        return result;
    }

public:
    explicit ManualSettlement(UObject* selected, int slot_index = -1, bool use_ancient_breeder = false)
        : original_single(slot_index == -1), ancient_breeder(use_ancient_breeder), model(selected), utility(find<UObject*>(STR("/Script/Pal.Default__PalUtility"))),
          manager(object_call(utility, STR("/Script/Pal.PalUtility:GetItemContainerManager"), model)),
          save_manager(object_call(utility, STR("/Script/Pal.PalUtility:GetSaveGameManager"), model)),
          dynamic_subsystem(world_dynamic_subsystem(model)), source(model_container(model)),
          egg(original_single
              ? (source.slots.size() == 1 ? source.slots.front() : throw std::runtime_error("Test requires exactly one original egg slot"))
              : ((ancient_breeder || source.slots.size() == 54) && slot_index >= 0 && static_cast<size_t>(slot_index) < source.slots.size()
                  ? source.slots.at(static_cast<size_t>(slot_index)) : throw std::runtime_error("Processor requires its own 54-slot input"))),
          receipt(egg.state.item.dynamic) {
        for (auto* object : {model, manager, save_manager, dynamic_subsystem, source.object, egg.object}) lifetime.emplace_back(object);
        require(!ancient_breeder || !original_single, "Ancient breeder requires an explicit egg slot");
        require(egg.state.count == 1 && GMalloc && *GMalloc, "Exactly one dynamic egg and engine allocator are required");
        check_context();
        // Resolve the native drop prerequisites before claiming RNG or consuming.
        static_cast<void>(ground_outlet());
    }
    bool load_or_claim() {
        if (auto old = receipt.load()) {
            if (ancient_breeder && old->phase != st::Phase::Ready) {
                ancient_prior_phase = old->phase;
                ready = old->phase == st::Phase::GroundRequested;
                Output::send<LogLevel::Warning>(STR("[PRFAncient] EXISTING_RECEIPT egg={} phase={} no-replay=true\n"),
                    to_wstring(receipt.key), static_cast<int>(old->phase));
                return true;
            }
            require(old->phase == st::Phase::Ready,
                "Egg already has a resolving, applying, verified or ground-requested receipt; replay forbidden");
            saved = std::move(old->drops);
            ready = true;
            Output::send<LogLevel::Normal>(STR("[PRFSettlement] FIXED_RESULT_REUSED egg={} rows={} RNG-called=false\n"),
                to_wstring(receipt.key), saved.size());
            return true;
        }
        receipt.create_claim();
        Output::send<LogLevel::Warning>(STR("[PRFSettlement] RNG_CLAIMED egg={}\n"), to_wstring(receipt.key));
        return false;
    }
    std::optional<st::Phase> prior_ancient_phase() const { return ancient_prior_phase; }
    void save_result(const NativeDrops& drops, bool defer_write = false) {
        require(!ready, "Native result already recorded");
        for (int32_t i = 0; i < drops.num; ++i) {
            const auto& row = drops.data[i];
            saved.push_back({to_string(FName{row.item.index, row.item.number}.ToString()), row.count});
        }
        ready = true;
        if (!defer_write) persist_result();
    }
    void save_breeder_result(std::vector<st::SavedDrop> drops) {
        require(ancient_breeder && !ready, "Breeder result already recorded or wrong transaction");
        saved = std::move(drops);
        ready = true;
    }
    // Disk-only methods: called serially on the worker, never concurrently with
    // game-thread access to this transaction. Durable barriers remain intact.
    void persist_result() {
        receipt.append(st::ready_record(saved));
        Output::send<LogLevel::Normal>(STR("[PRFSettlement] FIXED_RESULT_SAVED rows={} egg-retained=true\n"), saved.size());
    }
    void persist_applying() { receipt.append("APPLYING\n"); }
    void persist_ground_requested() { receipt.append("GROUND_REQUESTED\n"); }
    void validate_pending() { check_context(); }
    void attempt(bool durable_applying = false, bool remove_previously_settled = false) {
        require(!remove_previously_settled || (ancient_breeder && ancient_prior_phase == st::Phase::GroundRequested),
            "Only a terminal ancient receipt permits source-only reconciliation");
        require(ready, "No saved native result");
        check_context();
        // A world reload can restore an egg whose output was already requested.
        // Reconcile that source only; never request its materials a second time.
        const auto materials = remove_previously_settled ? decltype(material_counts()){} : material_counts();
        const auto outlet = ground_outlet();

        // Reuse the proven source-only native removal and completion notification.
        // No chest lookup, category filter, capacity check or destination write.
        const st::Array<st::ItemState> no_products{};
        std::vector<st::Consume> consumed{{egg.id, 1}};
        const auto input = view(consumed);
        OwnedNativeArray<st::Change> removal;
        OwnedNativeArray<st::ItemCount> leftovers;
        const auto status = native.plan(manager, &source.id, &no_products, &input, &removal.array, &leftovers.array);
        removal.validate();
        leftovers.validate();
        require(status <= 1 && removal.array.num == 1 && leftovers.array.num == 0, "Native egg removal plan refused");
        const auto& change = removal.array.data[0];
        require(change.target == egg.id && change.state.count == 0 && change.state.item == st::ItemId{} &&
            change.source.container == st::Guid{} && change.source.index == -1,
            "Native egg removal plan is not exactly the selected egg");

        // Native apply is void and can skip invalid slots. Check the exact source
        // immediately before it, and read its result back before requesting loot.
        check_context();
        const auto before = read_slot(egg.object);
        require(before.id == egg.id && before.state == egg.state, "Source changed; fixed result remains pending");
        if (!durable_applying) persist_applying();
        Output::send<LogLevel::Warning>(STR("[PRFSettlement] APPLY_BEGIN source-slots=1 output=vanilla-ground no-chest-writes=true\n"));
        native.apply(manager, &removal.array, 1); // Original Product operation; absolute source state.
        const auto after = read_slot(egg.object);
        require(after.id == egg.id && after.state.item == change.state.item && after.state.count == 0,
            "Native egg removal readback differs; receipt frozen, do not replay");
        if (original_single) {
            auto* completed = CastField<FStructProperty>(model->GetPropertyByNameInChain(STR("HatchedCharacterSaveParameter")));
            require(completed && completed->GetOffset_Internal() == 0x270, "Completion field unavailable after apply");
            st::Name ready_name{};
            std::memcpy(&ready_name, completed->ContainerPtrToValuePtr<void>(model), sizeof(ready_name));
            require(ready_name == st::Name{}, "Vanilla container notification did not clear completion; receipt frozen");
        }
        if (ancient_breeder) {
            // Native container notifications must release the old completed
            // record before any loot request. Never manually remove FastArray data.
            auto* rep = CastField<FStructProperty>(model->GetPropertyByNameInChain(STR("RepInfoArray")));
            auto* items = rep ? CastField<FArrayProperty>(rep->GetStruct()->GetPropertyByName(STR("Items"))) : nullptr;
            auto* inner = items ? CastField<FStructProperty>(items->GetInner()) : nullptr;
            auto* index = inner ? CastField<FIntProperty>(inner->GetStruct()->GetPropertyByName(STR("SlotIndex"))) : nullptr;
            require(index, "Ancient completion readback unavailable; receipt frozen");
            FScriptArrayHelper entries{items, items->ContainerPtrToValuePtr<void>(rep->ContainerPtrToValuePtr<void>(model))};
            for (int i = 0; i < entries.Num(); ++i)
                require(*index->ContainerPtrToValuePtr<int32_t>(entries.GetRawPtr(i)) != egg.id.index,
                    "Native egg removal did not clear the ancient completion; no loot request");
        }
        // Same dynamic-egg disposal function called after vanilla container apply.
        std::vector<st::DynamicId> removed_ids{egg.state.item.dynamic};
        const auto removed_input = view(removed_ids);
        native.dispose_dynamic(dynamic_subsystem, &removed_input);
        if (remove_previously_settled) {
            ready = false;
            Output::send<LogLevel::Warning>(STR("[PRFAncient] RESTORED_SOURCE_REMOVED egg={} RNG=0 additional-drops=0\n"), to_wstring(receipt.key));
            return;
        }
        Output::send<LogLevel::Warning>(STR("[PRFSettlement] EGG_READBACK_OK egg-consumed=1 live-Pal-created=0 outlet=({},{},{}) receipt={}\n"),
            outlet.x, outlet.y, outlet.z, to_wstring(receipt.key));
        const NativeLocation release_direction{0.0, 0.0, 1.0}; // Same constant as vanilla converter.
        for (const auto& [name, count] : materials) {
            Output::send<LogLevel::Warning>(STR("[PRFSettlement] GROUND_REQUEST item={} count={} auto-pickup=true\n"),
                FName{name.index, name.number}.ToString(), count);
            native.spawn_ground(model, {name.index, name.number}, static_cast<int32_t>(count),
                &outlet, &release_direction, true);
        }
        // The vanilla helper returns void after queuing native map-object creation.
        // No guessed success, no second spawn attempt, no instant save before drops
        // exist. The player must verify pickup/hauling, then exit normally.
        if (!durable_applying) persist_ground_requested();
        ready = false;
        Output::send<LogLevel::Warning>(STR("[PRFSettlement] GROUND_REQUESTED types={} egg-consumed=1 live-Pal-created=0 ground-visible=UNVERIFIED hauling=UNVERIFIED no-replay=true\n"),
            materials.size());
    }
};
