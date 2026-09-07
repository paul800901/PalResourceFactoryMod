#pragma once
#include "LocalizedButtons.hpp"
// Only the processor's open chest UI is repurposed. Inventory movement uses
// the game's server RPC; no item creation, deletion or client-side slot edits.
UObject* optional_object_field(UObject* object, const wchar_t* name) {
    if (!live(object)) return nullptr;
    auto* field = CastField<FObjectProperty>(object->GetPropertyByNameInChain(name));
    return field ? *field->ContainerPtrToValuePtr<UObject*>(object) : nullptr;
}

void set_text_value(UObject* text_widget, const wchar_t* text) {
    auto* convert = find<UFunction*>(STR("/Script/Engine.KismetTextLibrary:Conv_StringToText"));
    Parameters input{convert};
    auto* string = CastField<FStrProperty>(convert->GetPropertyByName(STR("InString")));
    require(string, "Deposit label string unavailable");
    *string->ContainerPtrToValuePtr<FString>(input.data.data()) = FString{text};
    find<UObject*>(STR("/Script/Engine.Default__KismetTextLibrary"))->ProcessEvent(convert, input.data.data());
    auto* set = find<UFunction*>(STR("/Script/UMG.TextBlock:SetText"));
    Parameters output{set};
    auto* value = set->GetPropertyByName(STR("InText"));
    auto* result = convert->GetReturnProperty();
    require(value && result && value->GetElementSize() == result->GetElementSize(), "Deposit label text layout differs");
    value->CopyCompleteValue(value->ContainerPtrToValuePtr<void>(output.data.data()), result->ContainerPtrToValuePtr<void>(input.data.data()));
    text_widget->ProcessEvent(set, output.data.data());
}

const wchar_t* deposit_label() {
    auto* get = find<UFunction*>(STR("/Script/Engine.KismetInternationalizationLibrary:GetCurrentLanguage"));
    Parameters args{get};
    find<UObject*>(STR("/Script/Engine.Default__KismetInternationalizationLibrary"))->ProcessEvent(get, args.data.data());
    auto* result = CastField<FStrProperty>(get->GetReturnProperty());
    require(result, "Current language unavailable");
    const std::wstring language{result->ContainerPtrToValuePtr<FString>(args.data.data())->GetCharArray().GetData()};
    std::wstring code=language;
    for (auto& c : code) { if (c==L'_') c=L'-'; if(c>=L'A' && c<=L'Z') c+=L'a'-L'A'; }
    if(code.starts_with(L"zh")) {
        code=(code.starts_with(L"zh-hant") || code.starts_with(L"zh-tw") || code.starts_with(L"zh-hk") || code.starts_with(L"zh-mo")) ? L"zh-Hant" : L"zh-Hans";
    } else if(code.starts_with(L"pt")) code=L"pt-BR";
    else if(code.starts_with(L"es-419") || code.starts_with(L"es-mx")) code=L"es-419";
    else code=code.substr(0,code.find(L'-'));
    for (const auto& row : prf_locale::buttons) if(code==row.language) return row.text;
    return prf_locale::buttons[0].text;
}

// Resolve reflection once per label update, not for every inventory widget.
struct DepositLabelSearch {
    UObject* quick_panel{};
    UClass* text = find<UClass*>(STR("/Script/UMG.TextBlock"));
    UClass* panel = find<UClass*>(STR("/Script/UMG.PanelWidget"));
    UClass* user = find<UClass*>(STR("/Script/UMG.UserWidget"));
    UFunction* count = find<UFunction*>(STR("/Script/UMG.PanelWidget:GetChildrenCount"));
    UFunction* child = find<UFunction*>(STR("/Script/UMG.PanelWidget:GetChildAt"));
    int visited{};
};

// Never descend into nested inventory/slot UserWidgets while looking for the
// chest's own button. Descend into button children only after finding its panel.
void deposit_label_tree(UObject* widget, std::vector<UObject*>& labels, DepositLabelSearch& search, bool inside = false, int depth = 0) {
    if (!live(widget) || depth > 24) return;
    ++search.visited;
    if (widget->GetName() == STR("Canvas_QuickMoveButton")) search.quick_panel = widget;
    inside |= widget->GetName() == STR("Canvas_QuickMoveButton");
    if (inside && widget->IsA(search.text)) {
        labels.push_back(widget);
    }
    if (widget->IsA(search.panel)) {
        Parameters count_args{search.count};
        widget->ProcessEvent(search.count, count_args.data.data());
        auto* count_result = search.count->GetReturnProperty();
        require(count_result && count_result->GetElementSize() == sizeof(int), "Panel child count differs");
        int children{};
        std::memcpy(&children, count_result->ContainerPtrToValuePtr<void>(count_args.data.data()), sizeof(children));
        auto* get = search.child;
        for (int i = 0; i < children; ++i) {
            Parameters args{get};
            auto* index = get->GetPropertyByName(STR("Index"));
            require(index && index->GetElementSize() == 4, "Panel index differs");
            std::memcpy(index->ContainerPtrToValuePtr<void>(args.data.data()), &i, 4);
            widget->ProcessEvent(get, args.data.data());
            auto* result = CastField<FObjectProperty>(get->GetReturnProperty());
            require(result, "Panel child unavailable");
            deposit_label_tree(*result->ContainerPtrToValuePtr<UObject*>(args.data.data()), labels, search, inside, depth + 1);
        }
    } else if ((depth == 0 || inside) && widget->IsA(search.user)) {
        auto* tree = optional_object_field(widget, STR("WidgetTree"));
        deposit_label_tree(optional_object_field(tree, STR("RootWidget")), labels, search, inside, depth + 1);
    }
}

std::wstring text_value(UObject* widget) {
    auto* get = find<UFunction*>(STR("/Script/UMG.TextBlock:GetText"));
    Parameters value{get};
    widget->ProcessEvent(get, value.data.data());
    auto* convert = find<UFunction*>(STR("/Script/Engine.KismetTextLibrary:Conv_TextToString"));
    Parameters output{convert};
    auto* input = convert->GetPropertyByName(STR("InText"));
    require(input && input->GetElementSize() == get->GetReturnProperty()->GetElementSize(), "Label readback differs");
    input->CopyCompleteValue(input->ContainerPtrToValuePtr<void>(output.data.data()), get->GetReturnProperty()->ContainerPtrToValuePtr<void>(value.data.data()));
    find<UObject*>(STR("/Script/Engine.Default__KismetTextLibrary"))->ProcessEvent(convert, output.data.data());
    auto* result = CastField<FStrProperty>(convert->GetReturnProperty());
    require(result, "Label string readback unavailable");
    return std::wstring{result->ContainerPtrToValuePtr<FString>(output.data.data())->GetCharArray().GetData()};
}

void deposit_backpack_eggs(UObject* widget, UObject* model) {
    auto* utility = find<UObject*>(STR("/Script/Pal.Default__PalUtility"));
    require(bool_call(utility, STR("/Script/Pal.PalUtility:IsServer"), model) &&
        !bool_call(utility, STR("/Script/Pal.PalUtility:IsDedicatedServer"), model) &&
        !bool_call(utility, STR("/Script/Pal.PalUtility:IsOpenListenServer"), model), "Deposit test supports solo worlds only");
    const auto target = ManualSettlement::model_container(model);
    const auto capacity = std::count_if(target.slots.begin(), target.slots.end(), [](const auto& slot) { return slot.state.count == 0; });
    if (!capacity) return;
    auto* inventory = object_call(utility, STR("/Script/Pal.PalUtility:GetLocalInventoryData"), widget);
    require(live(inventory), "Local backpack unavailable");
    auto* get = find<UFunction*>(STR("/Script/Pal.PalPlayerInventoryData:TryGetContainerFromInventoryType"));
    Parameters args{get};
    auto* type = get->GetPropertyByName(STR("inventoryType"));
    auto* out = CastField<FObjectProperty>(get->GetPropertyByName(STR("OutContainer")));
    require(type && type->GetElementSize() == 1 && out, "Backpack lookup layout differs");
    const uint8_t common = 0; // EPalPlayerInventoryType::Common, not equipment.
    std::memcpy(type->ContainerPtrToValuePtr<void>(args.data.data()), &common, 1);
    inventory->ProcessEvent(get, args.data.data());
    const auto source = ManualSettlement::read_container(*out->ContainerPtrToValuePtr<UObject*>(args.data.data()));
    std::vector<st::Consume> eggs;
    for (const auto& slot : source.slots) {
        if (slot.state.count != 1) continue;
        // Static food eggs and other items are deliberately excluded.
        auto* lookup = find<UFunction*>(STR("/Script/Pal.PalItemSlot:TryGetDynamicItemData"));
        Parameters data{lookup};
        slot.object->ProcessEvent(lookup, data.data.data());
        auto* field = CastField<FObjectProperty>(lookup->GetPropertyByName(STR("OutDynamicItemData")));
        require(field, "Dynamic item output unavailable");
        auto* egg = *field->ContainerPtrToValuePtr<UObject*>(data.data.data());
        if (!live(egg) || !egg->IsA(find<UClass*>(STR("/Script/Pal.PalDynamicPalEggItemDataBase")))) continue;
        require(read_field<st::DynamicId>(egg, STR("ID")) == slot.state.item.dynamic, "Backpack egg identity differs");
        eggs.push_back(st::Consume{slot.id, 1});
        if (eggs.size() >= static_cast<size_t>(capacity)) break;
    }
    if (eggs.empty()) return;
    auto* transmitter = object_call(utility, STR("/Script/Pal.PalUtility:GetNetworkTransmitter"), widget);
    require(live(transmitter), "Local network transmitter unavailable");
    auto* network = object_call(transmitter, STR("/Script/Pal.PalNetworkTransmitter:GetItem"));
    auto* item_class = find<UClass*>(STR("/Script/Pal.PalNetworkItemComponent"));
    require(live(network) && network->IsA(item_class), "Network item component unavailable");
    auto* move = find<UFunction*>(STR("/Script/Pal.PalNetworkItemComponent:RequestMoveToContainer_ToServer"));
    Parameters request{move};
    auto* destination = move->GetPropertyByName(STR("ToContainerId"));
    auto* froms = CastField<FArrayProperty>(move->GetPropertyByName(STR("Froms")));
    auto* request_id = move->GetPropertyByName(STR("RequestID"));
    require(destination && destination->GetElementSize() == sizeof(st::Guid) && request_id && request_id->GetElementSize() == sizeof(st::Guid) &&
        froms && froms->GetInner()->GetElementSize() == sizeof(st::Consume), "Deposit RPC layout differs");
    const auto id = returned<st::Guid>(find<UObject*>(STR("/Script/Engine.Default__KismetGuidLibrary")), STR("/Script/Engine.KismetGuidLibrary:NewGuid"));
    std::memcpy(destination->ContainerPtrToValuePtr<void>(request.data.data()), &target.id, sizeof(target.id));
    std::memcpy(request_id->ContainerPtrToValuePtr<void>(request.data.data()), &id, sizeof(id));
    struct BorrowedArray { st::Consume* data; int32_t num; int32_t capacity; };
    const BorrowedArray borrowed{eggs.data(), static_cast<int32_t>(eggs.size()), static_cast<int32_t>(eggs.size())};
    require(froms->GetElementSize() == sizeof(borrowed), "Deposit RPC array header differs");
    // Reflection owns the deep copy and its destruction; never hand game code
    // a std::vector allocation as an owned Unreal array.
    froms->CopyCompleteValue(froms->ContainerPtrToValuePtr<void>(request.data.data()), &borrowed);
    network->ProcessEvent(move, request.data.data());
    Output::send<LogLevel::Normal>(STR("[PRFProcessor] DEPOSIT_ALL_REQUESTED eggs={} destination={} server-readback-pending=true\n"), eggs.size(), to_wstring(container_key(target.id)));
}
