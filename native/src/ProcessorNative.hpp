#pragma once
// Uses the current game's offspring generator and butcher implementation.
// No Pal actor, AI controller, Palbox entry or copied drop table is created.

template<class T> T returned(UObject* object, const wchar_t* path, UObject* world = nullptr) {
    auto* fn = find<UFunction*>(path);
    Parameters params{fn};
    params.set_world(world);
    auto* ret = fn->GetReturnProperty();
    require(ret && ret->GetElementSize() == sizeof(T), "Return value layout differs");
    object->ProcessEvent(fn, params.data.data());
    T value{};
    std::memcpy(&value, ret->ContainerPtrToValuePtr<void>(params.data.data()), sizeof(value));
    return value;
}

bool is_processor(UObject* model) {
    if (!live(model) || !model->GetWorld() || read_bool_field(model, STR("bDisposed"))) return false;
    if (read_field<st::Guid>(model, STR("InstanceId")) == st::Guid{}) return false;
    const auto name = returned<NativeName>(model, STR("/Script/Pal.PalMapObjectConcreteModelBase:TryGetMapObjectId"));
    return FName{name.index, name.number}.ToString() == STR("PRF_EggResourceProcessor");
}

UObject* dynamic_egg(const ManualSettlement::Slot& slot) {
    require(slot.state.count == 1, "Each processor slot must contain exactly one egg");
    auto* fn = find<UFunction*>(STR("/Script/Pal.PalItemSlot:TryGetDynamicItemData"));
    Parameters params{fn};
    slot.object->ProcessEvent(fn, params.data.data());
    auto* out = CastField<FObjectProperty>(fn->GetPropertyByName(STR("OutDynamicItemData")));
    require(out, "Dynamic egg lookup output unavailable");
    auto* egg = *out->ContainerPtrToValuePtr<UObject*>(params.data.data());
    require(live(egg) && egg->IsA(find<UClass*>(STR("/Script/Pal.PalDynamicPalEggItemDataBase"))) &&
        read_field<st::DynamicId>(egg, STR("ID")) == slot.state.item.dynamic, "Input is not the selected dynamic egg");
    return egg;
}

double incubation_seconds(UObject* model, UObject* egg) {
    auto* utility = find<UObject*>(STR("/Script/Pal.Default__PalUtility"));
    auto* fn = find<UFunction*>(STR("/Script/Pal.PalUtility:GetOptionWorldSettings"));
    Parameters params{fn};
    params.set_world(model);
    utility->ProcessEvent(fn, params.data.data());
    auto* options = CastField<FStructProperty>(fn->GetReturnProperty());
    require(options, "World hatching options unavailable");
    auto* hours_field = CastField<FFloatProperty>(options->GetStruct()->GetPropertyByName(STR("PalEggDefaultHatchingTime")));
    require(hours_field, "World egg incubation time unavailable");
    const auto hours = *hours_field->ContainerPtrToValuePtr<float>(options->ContainerPtrToValuePtr<void>(params.data.data()));
    require(std::isfinite(hours) && hours >= 0, "Invalid current world incubation time");
    // Audited v1.0.3 native 0x30294e0 uses 3600 seconds/hour, with a 0.1-work minimum
    // for a zero-time world. The processor's defined comfortable speed is 1.5.
    if (hours == 0) return 0.1 / 1.5;

    const auto character = read_field<NativeName>(egg, STR("CharacterID"), 0x70);
    auto* database = object_call(utility, STR("/Script/Pal.PalUtility:GetDatabaseCharacterParameter"), model);
    auto* rarity_fn = find<UFunction*>(STR("/Script/Pal.PalDatabaseCharacterParameter:GetRarity"));
    Parameters rarity_args{rarity_fn};
    auto* row = rarity_fn->GetPropertyByName(STR("RowName"));
    require(row && row->GetElementSize() == 8, "Current rarity query layout differs");
    std::memcpy(row->ContainerPtrToValuePtr<void>(rarity_args.data.data()), &character, 8);
    require(live(database), "Current character database unavailable");
    database->ProcessEvent(rarity_fn, rarity_args.data.data());
    auto* ret = CastField<FIntProperty>(rarity_fn->GetReturnProperty());
    require(ret, "Current rarity query return differs");
    const auto rarity = *ret->ContainerPtrToValuePtr<int32_t>(rarity_args.data.data());
    auto* setting = object_call(utility, STR("/Script/Pal.PalUtility:GetGameSetting"), model);
    require(live(setting), "Game settings unavailable");
    auto* array = CastField<FArrayProperty>(setting->GetPropertyByNameInChain(STR("PalEggRankInfoArray")));
    auto* inner = array ? CastField<FStructProperty>(array->GetInner()) : nullptr;
    require(inner, "Current egg rank table unavailable");
    auto* rank = CastField<FIntProperty>(inner->GetStruct()->GetPropertyByName(STR("PalRarity")));
    auto* rate = CastField<FFloatProperty>(inner->GetStruct()->GetPropertyByName(STR("HatchingSpeedDivisionRate")));
    require(rank && rate, "Current egg rank fields unavailable");
    FScriptArrayHelper rows{array, array->ContainerPtrToValuePtr<void>(setting)};
    for (int i = 0; i < rows.Num(); ++i) {
        void* data = rows.GetRawPtr(i);
        if (*rank->ContainerPtrToValuePtr<int32_t>(data) < rarity) continue;
        const auto divisor = *rate->ContainerPtrToValuePtr<float>(data);
        require(std::isfinite(divisor) && divisor > 0, "Current egg rank divisor is invalid");
        return static_cast<double>(hours) * 3600.0 / divisor / 1.5;
    }
    throw std::runtime_error("Current game has no matching egg rank; no guessed hatch time");
}

double disassembly_interval() {
    // Read the game's parameter CDO, not a hard-coded speed or a copied table.
    auto* defaults = find<UObject*>(STR("/Script/Pal.Default__PalMapObjectConvertCharacterToItemParameterComponent"));
    // This is deliberately a class default object. The settlement reader
    // rejects CDOs because its callers operate on live eggs and containers.
    require(defaults->HasAnyFlags(RF_ClassDefaultObject) &&
        !defaults->HasAnyFlags(static_cast<EObjectFlags>(RF_BeginDestroyed | RF_FinishDestroyed)) &&
        !defaults->IsUnreachable(), "Native disassembly defaults unavailable");
    auto* field = CastField<FFloatProperty>(defaults->GetPropertyByNameInChain(STR("RequiredConvertProcessTime")));
    require(field, "Native disassembly duration field unavailable");
    const auto seconds = *field->ContainerPtrToValuePtr<float>(defaults);
    require(std::isfinite(seconds) && seconds > 0, "Current native disassembly interval unavailable");
    return seconds;
}

int disassembly_capacity() {
    auto* defaults = find<UObject*>(STR("/Script/Pal.Default__PalMapObjectConvertCharacterToItemParameterComponent"));
    require(defaults->HasAnyFlags(RF_ClassDefaultObject) && !defaults->IsUnreachable(), "Native disassembly defaults unavailable");
    auto* field = CastField<FIntProperty>(defaults->GetPropertyByNameInChain(STR("ConvertQueueCapacity")));
    require(field, "Native conveyor capacity field unavailable");
    const auto capacity = *field->ContainerPtrToValuePtr<int32_t>(defaults);
    require(capacity > 0, "Invalid native conveyor capacity");
    return capacity;
}

bool processor_status(UObject* actor, int state, const std::array<int, 2>& activity) {
    if (!live(actor)) return false;
    bool projection_updated{};
    auto* fn = find<UFunction*>(STR("/Script/Engine.Actor:K2_GetComponentsByClass"));
    Parameters params{fn};
    auto* cls = fn->GetPropertyByName(STR("ComponentClass"));
    auto* result = CastField<FArrayProperty>(fn->GetReturnProperty());
    require(cls && cls->GetElementSize() == 8 && result && CastField<FObjectProperty>(result->GetInner()), "Status component query differs");
    auto* mesh_class = find<UClass*>(STR("/Script/Engine.PrimitiveComponent"));
    std::memcpy(cls->ContainerPtrToValuePtr<void>(params.data.data()), &mesh_class, 8);
    actor->ProcessEvent(fn, params.data.data());
    FScriptArrayHelper components{result, result->ContainerPtrToValuePtr<void>(params.data.data())};
    const std::array<std::array<double, 4>, 4> colors{{
        {0.01, 0.04, 0.05, 0}, {0.03, 1.34, 1.76, 0.6}, {1.8, 0.03, 0.016, 2}, {2.0, 0.6, 0.01, 0}}};
    for (int i = 0; i < components.Num(); ++i) {
        auto* component = *reinterpret_cast<UObject**>(components.GetRawPtr(i));
        if (!live(component)) continue;
        const bool projection = component->GetName().find(STR("PRF_Projection")) != StringType::npos;
        if (!projection && component->GetName().find(STR("PRF_Status")) == StringType::npos) continue;
        auto* set = find<UFunction*>(STR("/Script/Engine.PrimitiveComponent:SetCustomPrimitiveDataVector4"));
        Parameters arguments{set};
        auto* value = set->GetPropertyByName(STR("Value"));
        require(value && value->GetElementSize() == 32, "Status custom-data vector layout differs");
        const std::array<double, 4> projection_data{double(activity[0]), double(activity[1]), state == 3 ? 1.0 : 0.0, 0.0};
        auto* index = set->GetPropertyByName(STR("DataIndex"));
        require(index && index->GetElementSize() == 4, "Custom-data index layout differs");
        const int data_index = projection ? 4 : 0;
        std::memcpy(index->ContainerPtrToValuePtr<void>(arguments.data.data()), &data_index, 4);
        std::memcpy(value->ContainerPtrToValuePtr<void>(arguments.data.data()), projection ? projection_data.data() : colors.at(state).data(), 32);
        component->ProcessEvent(set, arguments.data.data());
        projection_updated |= projection;
    }
    return projection_updated;
}

void calculate_result(UObject* model, int slot_index, ManualSettlement& transaction) {
    transaction.validate_pending();
    const auto source = ManualSettlement::model_container(model);
    auto* egg = dynamic_egg(source.slots.at(static_cast<size_t>(slot_index)));
    auto* base = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    constexpr std::array<uint8_t, 16> prefix{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x54,0x24,0x10,0x55,0x56,0x57,0x41,0x54,0x41};
    require(std::memcmp(base + 0x3035620, prefix.data(), prefix.size()) == 0, "Native hatch entry differs; disabled");
    using GenerateHatchedSave = void* (*)(UObject*, void*, UObject*);
    auto generate = reinterpret_cast<GenerateHatchedSave>(base + 0x3035620);

    auto* structure = find<UScriptStruct*>(STR("/Script/Pal.PalIndividualCharacterSaveParameter"));
    require(structure->GetStructureSize() == 0x370, "Native hatch save layout differs");
    auto* egg_save = CastField<FStructProperty>(egg->GetPropertyByNameInChain(STR("SaveParameter")));
    require(egg_save && egg_save->GetOffset_Internal() == 0x78 && egg_save->GetStruct().Get() == structure,
        "Dynamic egg native save layout differs");
    // The native function initializes this buffer itself (v1.0.4: 0x2847780). Do not
    // double-initialize it or fill it with an invented character's defaults.
    alignas(16) std::array<std::byte, 0x370> save{};
    generate(model, save.data(), egg);
    struct ReleaseSave { UScriptStruct* type; void* data; ~ReleaseSave() { type->DestroyStruct(data); } } release{structure, save.data()};
    NativeName character{};
    std::memcpy(&character, save.data(), 8);
    require(character.index != 0, "Native hatch produced no character; egg retained and no reroll");

    RootedTemporary individual{find<UClass*>(STR("/Script/Pal.PalIndividualCharacterParameter")), reinterpret_cast<UObject*>(model->GetWorld())};
    auto* dest = CastField<FStructProperty>(individual.object->GetPropertyByNameInChain(STR("SaveParameter")));
    auto* actor = CastField<FObjectProperty>(individual.object->GetPropertyByNameInChain(STR("IndividualActor")));
    require(dest && dest->GetStruct().Get() == structure && dest->GetOffset_Internal() == 0x3d0 &&
        actor && actor->GetOffset_Internal() == 0x318 &&
        *actor->ContainerPtrToValuePtr<UObject*>(individual.object) == nullptr, "Transient individual layout differs");
    structure->CopyScriptStruct(dest->ContainerPtrToValuePtr<void>(individual.object), save.data());
    auto* utility = find<UObject*>(STR("/Script/Pal.Default__PalUtility"));
    auto* database = object_call(utility, STR("/Script/Pal.PalUtility:GetDatabaseCharacterParameter"), model);
    require(live(database), "Native butcher database unavailable");
    NativeDrops drops{};
    struct ReleaseDrops { NativeDrops& d; ~ReleaseDrops() { if (d.data) (*GMalloc)->Free(d.data); } } drop_release{drops};
    const NativeLocation location{};
    resolve_native()(database, &drops, individual.object, &location, nullptr);
    require(drops.num >= 0 && drops.capacity >= drops.num && (drops.num == 0 || drops.data), "Invalid native butcher output");
    Output::send<LogLevel::Normal>(STR("[PRFProcessor] NATIVE_RESULT slot={} character={} rows={} live-Pal-created=0\n"),
        slot_index, FName{character.index, character.number}.ToString(), drops.num);
    transaction.save_result(drops, true);
}
