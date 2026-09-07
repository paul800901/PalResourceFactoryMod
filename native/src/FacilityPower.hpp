#pragma once
// Only called after the caller has established the exact custom building ID.
namespace facility_power {
inline UObject* child_of_class(UObject* owner, const wchar_t* array_name, UClass* type) {
    if (!live(owner)) return nullptr;
    auto* field=CastField<FArrayProperty>(owner->GetPropertyByNameInChain(array_name));
    require(field && CastField<FObjectProperty>(field->GetInner()), "Base energy array unavailable");
    FScriptArrayHelper values{field,field->ContainerPtrToValuePtr<void>(owner)};
    for (int i=0; i<values.Num(); ++i) {
        UObject* value{};
        std::memcpy(&value,values.GetRawPtr(i),sizeof(value));
        if (live(value) && value->IsA(type)) return value;
    }
    return nullptr;
}
inline bool constructed(UObject* model) {
    auto* actor = object_call(model, STR("/Script/Pal.PalMapObjectConcreteModelBase:GetActor"));
    if (!live(actor)) return false;
    auto* state = actor->GetPropertyByNameInChain(STR("CurrentState"));
    return state && state->GetElementSize()==1 && *state->ContainerPtrToValuePtr<uint8_t>(actor)>=3;
}
inline UObject* module(UObject* model) {
    return object_call(model, STR("/Script/Pal.PalMapObjectConcreteModelBase:GetEnergyModule"));
}
inline bool available(UObject* model) {
    auto* energy = module(model);
    return live(energy) && bool_call(energy, STR("/Script/Pal.PalMapObjectEnergyModule:CanConsumeEnergy"));
}
inline bool update(UObject* model, float rate) {
    auto* energy = module(model);
    // A plain storage model does not request an energy module in its native
    // initialization. The custom processor uses that model for its egg-only
    // storage. Attach the game's own module, then run its native initialization
    // and base registration; never emulate a powered state or debit storage.
    if (!live(energy)) {
        auto* actor=object_call(model,STR("/Script/Pal.PalMapObjectConcreteModelBase:GetActor"));
        auto* id=live(actor) ? CastField<FNameProperty>(actor->GetPropertyByNameInChain(STR("BuildObjectId"))) : nullptr;
        if (!id || id->ContainerPtrToValuePtr<FName>(actor)->ToString()!=STR("PRF_EggResourceProcessor")) return false;
        auto* camp=object_call(model,STR("/Script/Pal.PalMapObjectConcreteModelBase:GetBaseCampModelBelongTo"));
        // A loaded camp can exist before its electric function. Do not attach
        // until native registration can actually reach the consumer list.
        auto* camp_energy=child_of_class(camp,STR("ModuleArray"),find<UClass*>(STR("/Script/Pal.PalBaseCampModuleEnergy")));
        if (!child_of_class(camp_energy,STR("FunctionArray"),find<UClass*>(STR("/Script/Pal.PalBaseCampModuleEnergy_Electric")))) return false;
        auto* base=reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
        // Inspected on 1.0.4.102642, SHA256 44b6295e...83443.
        constexpr uint8_t create_prefix[]{0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x55,0x57,0x41,0x55,0x41,0x56};
        constexpr uint8_t init_prefix[]{0x48,0x89,0x5c,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xd9};
        constexpr uint8_t register_prefix[]{0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0xf9};
        require(std::memcmp(base+0x3007580,create_prefix,sizeof(create_prefix))==0 &&
            std::memcmp(base+0x3039a70,init_prefix,sizeof(init_prefix))==0 &&
            std::memcmp(base+0x303d750,register_prefix,sizeof(register_prefix))==0,"Native energy lifecycle differs from inspected game");
        int32_t register_call{};
        std::memcpy(&register_call,base+0x303d750+0x67,sizeof(register_call));
        require(base[0x303d750+0x66]==0xe8 && 0x303d750+0x6b+register_call==0x2d952b0,
            "Native energy registration no longer calls AddUnique consumer");
        energy=reinterpret_cast<UObject*(*)(UObject*,UClass*)>(base+0x3007580)(model,find<UClass*>(STR("/Script/Pal.PalMapObjectEnergyModule")));
        require(live(energy) && module(model)==energy,"Native energy module attachment failed");
        reinterpret_cast<void(*)(UObject*)>(base+0x3039a70)(energy);
        // 303d750 -> 2d952b0 adds the consumer GUID uniquely and synchronizes
        // native power state. 3043000 -> 2d98b40 REMOVES the GUID.
        reinterpret_cast<void(*)(UObject*)>(base+0x303d750)(energy);
        Output::send<LogLevel::Normal>(STR("[PRFProcessor] ENERGY_MODULE_ATTACHED native-base-registration=true\n"));
    }
    for (const auto* name : {STR("ConsumeEnergySpeed"), STR("CurrentConsumeEnergySpeed")}) {
        auto* field = CastField<FFloatProperty>(energy->GetPropertyByNameInChain(name));
        require(field != nullptr, "Energy consumption field unavailable");
        *field->ContainerPtrToValuePtr<float>(energy) = rate;
    }
    auto* required = CastField<FBoolProperty>(energy->GetPropertyByNameInChain(STR("bRequiredConsumeEnergy")));
    require(required != nullptr, "Energy requirement field unavailable");
    required->SetPropertyValueInContainer(energy, true);
    return bool_call(energy, STR("/Script/Pal.PalMapObjectEnergyModule:CanConsumeEnergy"));
}
}
