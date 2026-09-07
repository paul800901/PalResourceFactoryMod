#pragma once
// Explicit-key readback of an ORIGINAL ancient breeder actor and its components.
// No construction, assignment, item mutation, generation or native settlement.
inline UObject* ancient_read_object(UObject* object, const wchar_t* name) {
    if (!live(object)) return nullptr;
    auto* field = CastField<FObjectProperty>(object->GetPropertyByNameInChain(name));
    return field ? *field->ContainerPtrToValuePtr<UObject*>(object) : nullptr;
}

// Only loaded tables and rows belonging to the observed original building.
// No asset loading, row construction, import or modification.
inline void inspect_ancient_tables(UObject* wrapper) {
    auto* id_field = CastField<FNameProperty>(wrapper->GetPropertyByNameInChain(STR("MapObjectMasterDataId")));
    auto* build_field = CastField<FNameProperty>(wrapper->GetPropertyByNameInChain(STR("BuildObjectId")));
    require(id_field && build_field, "Ancient wrapper ID fields differ");
    const auto id = *id_field->ContainerPtrToValuePtr<FName>(wrapper);
    const auto build_id = *build_field->ContainerPtrToValuePtr<FName>(wrapper);
    require(!id.IsNone() && !build_id.IsNone(), "Ancient wrapper IDs are not ready");
    const auto assign_prefix = id.ToString() + STR("_");
    std::vector<UObject*> tables;
    UObjectGlobals::FindAllOf(STR("DataTable"), tables);
    int rows{};
    for (auto* object : tables) {
        if (!live(object)) continue;
        const auto name = object->GetName();
        const bool master = name == STR("DT_MapObjectMasterDataTable");
        const bool build = name == STR("DT_BuildObjectDataTable");
        const bool assign = name == STR("DT_MapObjectAssignData");
        if (!master && !build && !assign) continue;
        auto* table = static_cast<UDataTable*>(object);
        auto* type = table->GetRowStruct().Get();
        require(type != nullptr, "Ancient reference table has no row structure");
        for (const auto& pair : table->GetRowMap()) {
            const auto row_name = pair.Key.ToString();
            if (!((master && pair.Key == id) || (build && pair.Key == build_id) ||
                (assign && (pair.Key == id || row_name.starts_with(assign_prefix))))) continue;
            require(pair.Value != nullptr, "Ancient reference row is null");
            Output::send<LogLevel::Normal>(STR("[PRFAncientReadback] TABLE {} row={}\n"), table->GetFullName(), row_name);
            for (TFieldIterator<FProperty> it{type}; it; ++it) {
                FString value;
                (*it)->ExportTextItem_Direct(value, (*it)->ContainerPtrToValuePtr<void>(pair.Value), nullptr, table, 0);
                Output::send<LogLevel::Normal>(STR("[PRFAncientReadback] ROW_FIELD {}={}\n"), (*it)->GetName(), *value);
            }
            ++rows;
        }
    }
    Output::send<LogLevel::Normal>(STR("[PRFAncientReadback] TABLES rows={} writes=0\n"), rows);
}

inline void inspect_ancient_blueprint() {
    std::vector<UObject*> actors;
    UObjectGlobals::FindAllOf(STR("BP_BuildObject_MultiElectricHatchingPalEggWithBreed_C"), actors);
    int inspected{};
    auto print = [](UObject* object) {
        Output::send<LogLevel::Normal>(STR("[PRFAncientReadback] OBJECT {} class={}\n"), object->GetName(), object->GetClassPrivate()->GetName());
        for (const auto* name : {L"ConcreteModelClass", L"MapObjectId", L"Model", L"ConcreteModel", L"MapObjectModel",
                L"SlotNum", L"ChestSlotNum", L"TargetItemIds", L"BreedRequiredRealTime", L"AutoWorkAmountBySec",
                L"MenuUIWidgetClass", L"ConsumptionEnergySpeed", L"InteractType", L"OperationRestrictType",
                L"MapObjectMasterDataId", L"BuildObjectId", L"bDisposed", L"bWorkable",
                L"BreedProgressTime", L"BreedStoppedReason", L"TargetBreedItemIds", L"BreedItemContainer",
                L"DefaultConsumeEnergySpeed", L"DefaultAutoWorkAmountBySec", L"WorkeeModuleCache",
                L"RequiredEnergyType", L"ConsumeEnergySpeed", L"TargetWork", L"TargetContainer"}) {
            auto* field = object->GetPropertyByNameInChain(name);
            if (!field) continue;
            FString value;
            field->ExportTextItem_Direct(value, field->ContainerPtrToValuePtr<void>(object), nullptr, object, 0);
            Output::send<LogLevel::Normal>(STR("[PRFAncientReadback] FIELD {}={}\n"), name, *value);
        }
    };
    for (auto* actor : actors) {
        if (!live(actor) || !actor->GetWorld()) continue;
        print(actor);
        auto* wrapper = ancient_read_object(actor, STR("MapObjectModel"));
        auto* concrete = ancient_read_object(wrapper, STR("ConcreteModel"));
        Output::send<LogLevel::Normal>(STR("[PRFAncientReadback] LINK wrapper-valid={} concrete-valid={}\n"), live(wrapper), live(concrete));
        if (live(wrapper)) {
            print(wrapper);
            inspect_ancient_tables(wrapper);
        }
        if (live(concrete)) {
            print(concrete);
            auto* workee = ancient_read_object(concrete, STR("WorkeeModuleCache"));
            if (live(workee)) print(workee);
            auto* work = ancient_read_object(workee, STR("TargetWork"));
            Output::send<LogLevel::Normal>(STR("[PRFAncientReadback] WORK valid={}\n"), live(work));
            if (live(work)) print(work);
            // Count only: do not export offspring SaveParameters or individual IDs.
            auto* rep = CastField<FStructProperty>(concrete->GetPropertyByNameInChain(STR("RepInfoArray")));
            auto* items = rep ? CastField<FArrayProperty>(rep->GetStruct()->GetPropertyByName(STR("Items"))) : nullptr;
            if (items) {
                FScriptArrayHelper info{items, items->ContainerPtrToValuePtr<void>(rep->ContainerPtrToValuePtr<void>(concrete))};
                Output::send<LogLevel::Normal>(STR("[PRFAncientReadback] EGG_RECORDS count={}\n"), info.Num());
            }
            auto* cake = ancient_read_object(concrete, STR("BreedItemContainer"));
            Output::send<LogLevel::Normal>(STR("[PRFAncientReadback] CAKE_CONTAINER valid={}\n"), live(cake));
            auto* egg_module = object_call(concrete, STR("/Script/Pal.PalMapObjectConcreteModelBase:GetItemContainerModule"));
            auto* egg_container = ancient_read_object(egg_module, STR("TargetContainer"));
            Output::send<LogLevel::Normal>(STR("[PRFAncientReadback] EGG_CONTAINER module-valid={} container-valid={} separate-from-cake={}\n"),
                live(egg_module), live(egg_container), live(egg_container) && live(cake) && egg_container != cake);
            if (live(egg_module)) print(egg_module);
            for (auto* container : {cake, egg_container}) {
                if (!live(container)) continue;
                auto* slots = CastField<FArrayProperty>(container->GetPropertyByNameInChain(STR("ItemSlotArray")));
                if (!slots) continue;
                FScriptArrayHelper contents{slots, slots->ContainerPtrToValuePtr<void>(container)};
                Output::send<LogLevel::Normal>(STR("[PRFAncientReadback] CONTAINER kind={} slots={}\n"),
                    container == cake ? STR("cake") : STR("egg"), contents.Num());
            }
        }
        Parameters args{find<UFunction*>(STR("/Script/Engine.Actor:K2_GetComponentsByClass"))};
        auto* cls = CastField<FObjectProperty>(args.function->GetPropertyByName(STR("ComponentClass")));
        auto* result = CastField<FArrayProperty>(args.function->GetReturnProperty());
        require(cls && result && CastField<FObjectProperty>(result->GetInner()), "Ancient component query differs");
        *cls->ContainerPtrToValuePtr<UObject*>(args.data.data()) = find<UClass*>(STR("/Script/Engine.ActorComponent"));
        actor->ProcessEvent(args.function, args.data.data());
        FScriptArrayHelper components{result, result->ContainerPtrToValuePtr<void>(args.data.data())};
        for (int i = 0; i < components.Num(); ++i) {
            auto* component = *reinterpret_cast<UObject**>(components.GetRawPtr(i));
            if (live(component)) print(component);
        }
        ++inspected;
    }
    Output::send<LogLevel::Normal>(STR("[PRFAncientReadback] DONE original-actors={} writes=0 RNG=0\n"), inspected);
}
