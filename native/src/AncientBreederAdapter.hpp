#pragma once
#include <memory>

// Game-thread primitive for the native ancient breeder route. Not a hook or
// production loop. Does not link the parked farm generation/internal-egg code.
namespace ancient_breeder {
inline void require_target(UObject* model) {
    require(live(model) && model->GetWorld() && !read_bool_field(model, STR("bDisposed")),
        "Ancient breeder is unavailable");
    require(model->IsA(find<UClass*>(STR("/Script/Pal.PalMapObjectMultiHatchingEggWithBreedModel"))),
        "Second machine must use the native ancient breeder model");
    Parameters args{find<UFunction*>(STR("/Script/Pal.PalMapObjectConcreteModelBase:TryGetMapObjectId"))};
    auto* ret = args.function->GetReturnProperty();
    require(ret && ret->GetElementSize() == sizeof(NativeName), "Building identity layout differs");
    model->ProcessEvent(args.function, args.data.data());
    NativeName id{};
    std::memcpy(&id, ret->ContainerPtrToValuePtr<void>(args.data.data()), sizeof(id));
    require(FName{id.index, id.number}.ToString() == STR("PRF_ResourceBreedingFacility"),
        "Adapter must not operate on the original ancient breeder or first machine");
    require(bool_call(find<UObject*>(STR("/Script/Pal.Default__PalUtility")),
        STR("/Script/Pal.PalUtility:IsServer"), model), "Client cannot settle offspring");
}

struct Completed {
    st::SlotId slot;
    st::DynamicId egg;
    RootedTemporary individual;
    Completed(UObject* model, const ManualSettlement::Slot& source, UScriptStruct* type, void* save)
        : slot(source.id), egg(source.state.item.dynamic),
          individual(find<UClass*>(STR("/Script/Pal.PalIndividualCharacterParameter")),
              reinterpret_cast<UObject*>(model->GetWorld())) {
        auto* dest = CastField<FStructProperty>(individual.object->GetPropertyByNameInChain(STR("SaveParameter")));
        auto* actor = CastField<FObjectProperty>(individual.object->GetPropertyByNameInChain(STR("IndividualActor")));
        require(dest && dest->GetStruct().Get() == type && dest->GetOffset_Internal() == 0x3d0 &&
            actor && actor->GetOffset_Internal() == 0x318 &&
            *actor->ContainerPtrToValuePtr<UObject*>(individual.object) == nullptr,
            "Transient offspring parameter layout differs");
        // CopyScriptStruct preserves the native completed genetics and owned
        // allocations. No RNG, hatch call, Pal actor or Palbox registration.
        type->CopyScriptStruct(dest->ContainerPtrToValuePtr<void>(individual.object), save);
    }
};

// Lightweight tick discovery: no transient UObject and no per-slot container
// traversal while native incubation is still running.
inline std::vector<int> completed_slots(UObject* model) {
    require_target(model);
    auto* rep = CastField<FStructProperty>(model->GetPropertyByNameInChain(STR("RepInfoArray")));
    auto* items = rep ? CastField<FArrayProperty>(rep->GetStruct()->GetPropertyByName(STR("Items"))) : nullptr;
    auto* inner = items ? CastField<FStructProperty>(items->GetInner()) : nullptr;
    require(inner && inner->GetStruct()->GetStructureSize() == 0x398, "Native completion list layout differs");
    auto* index = CastField<FIntProperty>(inner->GetStruct()->GetPropertyByName(STR("SlotIndex")));
    auto* save = CastField<FStructProperty>(inner->GetStruct()->GetPropertyByName(STR("HatchedCharacterSaveParameter")));
    auto* character = save ? CastField<FNameProperty>(save->GetStruct()->GetPropertyByName(STR("CharacterID"))) : nullptr;
    require(index && character && character->GetElementSize() == sizeof(NativeName), "Native completion list fields differ");
    FScriptArrayHelper entries{items, items->ContainerPtrToValuePtr<void>(rep->ContainerPtrToValuePtr<void>(model))};
    std::vector<int> result;
    for (int i = 0; i < entries.Num(); ++i) {
        auto* entry = entries.GetRawPtr(i);
        NativeName name{};
        std::memcpy(&name, character->ContainerPtrToValuePtr<void>(save->ContainerPtrToValuePtr<void>(entry)), sizeof(name));
        if (name.index != 0) result.push_back(*index->ContainerPtrToValuePtr<int32_t>(entry));
    }
    return result;
}

// Snapshot only a native completed entry whose identity still matches its egg.
// A caller must not keep raw FastArray pointers across a native operation.
inline std::unique_ptr<Completed> completed(UObject* model, int slot_index) {
    require_target(model);
    const auto container = ManualSettlement::model_container(model); // Egg module, NOT BreedItemContainer.
    require(slot_index >= 0 && static_cast<size_t>(slot_index) < container.slots.size(), "Invalid breeder egg slot");
    const auto& source = container.slots.at(static_cast<size_t>(slot_index));
    require(source.state.count == 1 && source.state.item.dynamic != st::DynamicId{}, "Missing unique native egg identity");
    auto* rep = CastField<FStructProperty>(model->GetPropertyByNameInChain(STR("RepInfoArray")));
    auto* items = rep ? CastField<FArrayProperty>(rep->GetStruct()->GetPropertyByName(STR("Items"))) : nullptr;
    auto* inner = items ? CastField<FStructProperty>(items->GetInner()) : nullptr;
    require(inner && inner->GetStruct()->GetStructureSize() == 0x398, "Native egg entry layout differs");
    auto* type = inner->GetStruct().Get();
    auto* index = CastField<FIntProperty>(type->GetPropertyByName(STR("SlotIndex")));
    auto* egg = CastField<FObjectProperty>(type->GetPropertyByName(STR("PalEggData")));
    auto* save = CastField<FStructProperty>(type->GetPropertyByName(STR("HatchedCharacterSaveParameter")));
    require(index && index->GetOffset_Internal() == 0xc && egg && egg->GetOffset_Internal() == 0x18 &&
        save && save->GetOffset_Internal() == 0x20 && save->GetStruct()->GetStructureSize() == 0x370,
        "Native completed egg fields differ");
    auto* character = CastField<FNameProperty>(save->GetStruct()->GetPropertyByName(STR("CharacterID")));
    require(character && character->GetElementSize() == sizeof(NativeName), "Completed character field differs");
    FScriptArrayHelper entries{items, items->ContainerPtrToValuePtr<void>(rep->ContainerPtrToValuePtr<void>(model))};
    for (int i = 0; i < entries.Num(); ++i) {
        auto* entry = entries.GetRawPtr(i);
        if (*index->ContainerPtrToValuePtr<int32_t>(entry) != slot_index) continue;
        void* data = save->ContainerPtrToValuePtr<void>(entry);
        NativeName name{};
        std::memcpy(&name, character->ContainerPtrToValuePtr<void>(data), sizeof(name));
        if (name.index == 0) return {}; // Still incubating; never generate a replacement.
        auto* dynamic = *egg->ContainerPtrToValuePtr<UObject*>(entry);
        require(live(dynamic) && read_field<st::DynamicId>(dynamic, STR("ID")) == source.state.item.dynamic,
            "Completed offspring does not match the current egg slot");
        return std::make_unique<Completed>(model, source, save->GetStruct().Get(), data);
    }
    return {};
}
// Caller contract: the exact dynamic egg must already have a durable RNG claim.
// This primitive neither claims nor consumes. Never retry a failed/interrupted
// invocation. The eventual production caller must persist these rows before
// native source consumption, and block the original obtain path throughout.
inline std::vector<st::SavedDrop> calculate_claimed(UObject* model, int slot_index, st::DynamicId claimed_egg) {
    require_target(model);
    auto* calculate = resolve_native(); // Current executable identity and butcher entry.
    auto offspring = completed(model, slot_index);
    require(offspring && offspring->egg == claimed_egg, "Claimed completed offspring is no longer present");
    auto* utility = find<UObject*>(STR("/Script/Pal.Default__PalUtility"));
    auto* database = object_call(utility, STR("/Script/Pal.PalUtility:GetDatabaseCharacterParameter"), model);
    require(live(database), "Native butcher database unavailable");
    NativeDrops drops{};
    struct Release { NativeDrops& value; ~Release() { if (value.data) (*GMalloc)->Free(value.data); } } release{drops};
    require(GMalloc && *GMalloc, "Engine allocator unavailable");
    const NativeLocation location{};
    calculate(database, &drops, offspring->individual.object, &location, nullptr);
    require(drops.num >= 0 && drops.capacity >= drops.num && (drops.num == 0 || drops.data),
        "Invalid native butcher output; do not reroll");
    std::vector<st::SavedDrop> result;
    result.reserve(static_cast<size_t>(drops.num));
    for (int i = 0; i < drops.num; ++i) {
        const auto& row = drops.data[i];
        result.push_back({to_string(FName{row.item.index, row.item.number}.ToString()), row.count});
    }
    return result;
}
} // namespace ancient_breeder
