#pragma once
#include "AncientBreederAdapter.hpp"
#include <algorithm>
#include <set>

namespace ancient_breeder {
// ProcessEvent guard. The runtime must attach the machine before starting any
// settlement and keep this guard active even if a transaction is suspended.
// Native/Blueprint bypass coverage still requires in-game verification.
class InputGuard {
    inline static UFunction* server_obtain{};
    inline static UnrealScriptFunction original_obtain{};
    static bool own_model(UObject* context) {
        if (!live(context) || !context->IsA(find<UClass*>(STR("/Script/Pal.PalMapObjectMultiHatchingEggWithBreedModel")))) return false;
        Parameters args{find<UFunction*>(STR("/Script/Pal.PalMapObjectConcreteModelBase:TryGetMapObjectId"))};
        auto* ret = args.function->GetReturnProperty();
        require(ret && ret->GetElementSize() == sizeof(NativeName), "Ancient identity ABI differs");
        context->ProcessEvent(args.function, args.data.data());
        NativeName id{};
        std::memcpy(&id, ret->ContainerPtrToValuePtr<void>(args.data.data()), sizeof(id));
        return FName{id.index, id.number}.ToString() == STR("PRF_ResourceBreedingFacility");
    }
    static void obtain_entry(UObject* context, FFrame& stack, void* result) {
        // This is the native UFunction entry, including calls made by Blueprint
        // and the server dispatcher without going through ProcessEvent.
        bool reject = true;
        try { reject = own_model(context); }
        catch (const std::exception& e) {
            Output::send<LogLevel::Error>(STR("[PRFAncient] OBTAIN_ID_FAILED {}\n"), to_wstring(e.what()));
        }
        if (!reject) { original_obtain(context, stack, result); return; }
        // Consume VM arguments even when skipping the native body. A bare return
        // would leave Blueprint's instruction pointer inside this call's args.
        Parameters discarded{server_obtain};
        for (TFieldIterator<FProperty> it{server_obtain}; it; ++it) {
            auto* field = *it;
            if (field->HasAnyPropertyFlags(CPF_Parm))
                stack.StepCompiledIn(field->ContainerPtrToValuePtr<void>(discarded.data.data()), field);
        }
        stack.Code() += !!stack.Code();
        Output::send<LogLevel::Warning>(STR("[PRFAncient] OBTAIN_BLOCKED native-entry=true live-Pal-created=0\n"));
    }
    enum class Request { Swap, Move, MoveContainer, Drop, Dispose, Filter, Sort };
    std::map<UFunction*, Request> requests;
    std::set<UFunction*> obtain;
    std::set<UFunction*> egg_interactions;
    UEnum* indicator_types{};
    mutable uint64_t indicator_events{};
    bool indicator_snapshot{};
    double indicator_wait{};
    struct Locked { FWeakObjectPtr model, module; st::Guid container; };
    std::vector<Locked> locked;
    template<class T> static T argument(UFunction* fn, void* params, const wchar_t* name) {
        auto* field = fn->GetPropertyByName(name);
        require(field && field->GetElementSize() == sizeof(T), "Ancient input argument layout differs");
        T result{};
        std::memcpy(&result, field->ContainerPtrToValuePtr<void>(params), sizeof(result));
        return result;
    }
public:
    ~InputGuard() {
        if (server_obtain && server_obtain->GetFuncPtr() == &obtain_entry) server_obtain->SetFuncPtr(original_obtain);
        server_obtain = nullptr;
        original_obtain = nullptr;
    }
    void initialize() {
        indicator_types = find<UEnum*>(STR("/Script/Pal.EPalInteractiveObjectIndicatorType"));
        for (const auto* name : {L"OnStartTriggerInteract", L"OnTriggerInteract", L"OnTriggeringInteract", L"OnEndTriggerInteract"}) {
            const auto path = std::wstring{L"/Script/Pal.PalMapObjectConcreteModelBase:"} + name;
            auto* fn = find<UFunction*>(path.c_str());
            auto* field = fn->GetPropertyByName(STR("IndicatorType"));
            require(field && field->GetElementSize() == sizeof(uint8_t), "Egg interaction type layout differs");
            egg_interactions.insert(fn);
        }
        for (const auto* name : {L"RequestObtainSingleHatchedCharacter", L"RequestObtainAllHatchedCharacter",
                                L"ObtainHatchedCharacter_ServerInternal"}) {
            const auto path = std::wstring{L"/Script/Pal.PalMapObjectHatchingEggModelBase:"} + name;
            obtain.insert(find<UFunction*>(path.c_str()));
        }
        auto add = [&](const wchar_t* name, Request kind) {
            const auto path = std::wstring{L"/Script/Pal.PalNetworkItemComponent:"} + name;
            requests.emplace(find<UFunction*>(path.c_str()), kind);
        };
        add(L"RequestSwap_ToServer", Request::Swap);
        add(L"RequestMove_ToServer", Request::Move);
        add(L"RequestMoveToContainer_ToServer", Request::MoveContainer);
        add(L"RequestDrop_ToServer", Request::Drop);
        add(L"RequestDispose_ToServer", Request::Dispose);
        add(L"RequestChangeFilter_ToServer", Request::Filter);
        add(L"RequestChangeAllFilterCheck_ToServer", Request::Filter);
        add(L"RequestChangeAllFilterUncheck_ToServer", Request::Filter);
        requests.emplace(find<UFunction*>(STR("/Script/Pal.PalMapObjectItemContainerModule:RequestSortContainer_ServerInternal")), Request::Sort);
        // Validate layouts before the runtime may attach/start a transaction.
        for (const auto& [fn, kind] : requests) {
            auto size = [&](const wchar_t* name, size_t bytes) {
                auto* field = fn->GetPropertyByName(name);
                require(field && field->GetElementSize() == bytes, "Ancient input ABI changed");
            };
            if (kind == Request::Swap) { size(L"SlotA", sizeof(st::SlotId)); size(L"SlotB", sizeof(st::SlotId)); }
            if (kind == Request::Move) size(L"To", sizeof(st::SlotId));
            if (kind == Request::Dispose) size(L"SlotInfo", sizeof(st::Consume));
            if (kind == Request::Filter) size(L"ContainerId", sizeof(st::Guid));
            if (kind == Request::Move || kind == Request::MoveContainer || kind == Request::Drop) {
                auto* field = CastField<FArrayProperty>(fn->GetPropertyByName(kind == Request::Drop ? L"DropSlotAndNumArray" : L"Froms"));
                require(field && field->GetInner()->GetElementSize() == sizeof(st::Consume), "Ancient input array ABI changed");
            }
        }
        // Locate the actual multi-incubator override by its verified native
        // entry, not the identically named function on HatchingEggModelBase.
        auto* base = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
        auto* cls = find<UClass*>(STR("/Script/Pal.PalMapObjectMultiHatchingEggWithBreedModel"));
        for (auto* fn : cls->ForEachFunctionInChain()) {
            if (fn->GetName() != STR("ObtainHatchedCharacter_ServerInternal")) continue;
            obtain.insert(fn);
            if (reinterpret_cast<void*>(fn->GetFuncPtr()) != base + 0x29d42e0) continue;
            require(!server_obtain, "Duplicate ancient obtain entry");
            server_obtain = fn;
        }
        require(server_obtain && !server_obtain->GetReturnProperty(), "Verified multi-incubator obtain entry unavailable");
        for (TFieldIterator<FProperty> it{server_obtain}; it; ++it)
            require(!(*it)->HasAnyPropertyFlags(CPF_ReturnParm) &&
                (!(*it)->HasAnyPropertyFlags(CPF_OutParm) || (*it)->HasAnyPropertyFlags(CPF_ConstParm)),
                "Obtain entry has unsupported output parameters");
        original_obtain = server_obtain->GetFuncPtr();
        server_obtain->SetFuncPtr(&obtain_entry);
        Output::send<LogLevel::Normal>(STR("[PRFAncient] OBTAIN_GUARD_READY entry={}\n"), server_obtain->GetFullName());
    }
    void attach(UObject* model) {
        require_target(model);
        require(!obtain.empty() && !requests.empty(), "Ancient input guard not initialized");
        auto* module = object_call(model, STR("/Script/Pal.PalMapObjectConcreteModelBase:GetItemContainerModule"));
        const auto source = ManualSettlement::model_container(model);
        auto* drop = CastField<FBoolProperty>(module->GetPropertyByNameInChain(STR("bDropItemAtDisposed")));
        require(drop, "Ancient egg demolition policy unavailable");
        drop->SetPropertyValueInContainer(module, false);
        std::erase_if(locked, [](const auto& value) { return value.model.Get() == nullptr; });
        for (const auto& value : locked) if (value.model.Get() == model) {
            require(value.container == source.id && value.module.Get() == module, "Ancient container changed after attach");
            return;
        }
        locked.push_back({FWeakObjectPtr{model}, FWeakObjectPtr{module}, source.id});
    }
    void clear() { locked.clear(); indicator_snapshot=false; indicator_wait=0; indicator_events=0; }
    void inspect_indicator(double step) {
        if (indicator_snapshot || (indicator_wait += step) < 3) return;
        indicator_wait=0;
        std::vector<UObject*> widgets;
        UObjectGlobals::FindAllOf(STR("WBP_PalInteractiveObjectIndicatorUI_C"), widgets);
        bool found_egg=false;
        for (auto* widget:widgets) {
            if (!live(widget)) continue;
            auto* info=CastField<FStructProperty>(widget->GetPropertyByNameInChain(STR("CachedActionInfo")));
            if (!info) continue;
            auto* type=info->GetStruct()->GetPropertyByName(STR("IndicatorType"));
            if (!type || type->GetElementSize()!=1) continue;
            auto name=indicator_types->GetNameByValue(*type->ContainerPtrToValuePtr<uint8_t>(info->ContainerPtrToValuePtr<void>(widget))).ToString();
            if (name==STR("SetEgg") || name==STR("EPalInteractiveObjectIndicatorType::SetEgg")) found_egg=true;
        }
        if (!found_egg) return;
        indicator_snapshot=true;
        Output::send<LogLevel::Normal>(STR("[PRFAncient] INDICATOR_SNAPSHOT widgets={} matching-post-events={} writes=0\n"),widgets.size(),indicator_events);
        std::vector<UObject*> canvases;
        UObjectGlobals::FindAllOf(STR("WBP_PalInteractiveObjectIndicatorCanvas_C"),canvases);
        widgets.insert(widgets.end(),canvases.begin(),canvases.end());
        for (auto* widget:widgets) {
            if (!live(widget)) continue;
            Output::send<LogLevel::Normal>(STR("[PRFAncient] INDICATOR_OBJECT {}\n"),widget->GetFullName());
            for (TFieldIterator<FProperty> it{widget->GetClassPrivate()};it;++it) {
                auto* p=*it;
                const auto name=p->GetName();
                if (!CastField<FInterfaceProperty>(p) && !CastField<FObjectProperty>(p) && name!=STR("CachedActionInfo") && name!=STR("Visibility")) continue;
                FString value;
                p->ExportTextItem_Direct(value,p->ContainerPtrToValuePtr<void>(widget),nullptr,widget,0);
                Output::send<LogLevel::Normal>(STR("[PRFAncient] INDICATOR_FIELD {}={}\n"),name,*value);
            }
        }
    }
    void hide_egg_indicator(UObject* context, UFunction* fn, void* params) const {
        static const FName widget_name{STR("WBP_PalInteractiveObjectIndicatorUI_C")};
        if (!live(context) || !context->GetClassPrivate()->GetFName().Equals(widget_name)) return;
        ++indicator_events;
        // Work at the actual display widget, after its normal visibility update.
        // SetVisibility itself is excluded to avoid recursion through this hook.
        const auto event = fn->GetName();
        if (event == STR("SetVisibility") || event == STR("GetParent")) return;
        auto* info = CastField<FStructProperty>(context->GetPropertyByNameInChain(STR("CachedActionInfo")));
        require(info, "Indicator cached action missing");
        auto* type = info->GetStruct()->GetPropertyByName(STR("IndicatorType"));
        require(type && type->GetElementSize() == 1, "Indicator cached type differs");
        const auto name = indicator_types->GetNameByValue(*type->ContainerPtrToValuePtr<uint8_t>(info->ContainerPtrToValuePtr<void>(context))).ToString();
        if (name != STR("SetEgg") && name != STR("EPalInteractiveObjectIndicatorType::SetEgg")) return;
        // Live readback: the child InteractiveObject is None. Its panel belongs
        // to the Canvas which owns the actual "Interactive Object" interface.
        auto* panel = object_call(context, STR("/Script/UMG.Widget:GetParent"));
        if (!live(panel)) return;
        auto* canvas = panel->GetTypedOuter(find<UClass*>(STR("/Game/Pal/Blueprint/UI/WBP_PalInteractiveObjectIndicatorCanvas.WBP_PalInteractiveObjectIndicatorCanvas_C")));
        if (!live(canvas)) return;
        {
            auto* field = CastField<FInterfaceProperty>(canvas->GetPropertyByNameInChain(STR("Interactive Object")));
            require(field, "Canvas interaction target missing");
            require(field->GetElementSize() == sizeof(void*) * 2, "Indicator interface layout differs");
            UObject* component{};
            std::memcpy(&component, field->ContainerPtrToValuePtr<void>(canvas), sizeof(component));
            if (!live(component) || !component->IsA(find<UClass*>(STR("/Script/Pal.PalInteractiveObjectBoxComponent")))) return;
            auto* actor = object_call(component, STR("/Script/Engine.ActorComponent:GetOwner"));
            if (!live(actor)) return;
            auto* id = CastField<FNameProperty>(actor->GetPropertyByNameInChain(STR("BuildObjectId")));
            if (!id || id->ContainerPtrToValuePtr<FName>(actor)->ToString() != STR("PRF_ResourceBreedingFacility")) return;
            Parameters hidden{find<UFunction*>(STR("/Script/UMG.Widget:SetVisibility"))};
            auto* value = hidden.function->GetPropertyByName(STR("InVisibility"));
            require(value && value->GetElementSize() == 1, "Indicator visibility layout differs");
            *value->ContainerPtrToValuePtr<uint8_t>(hidden.data.data()) = 1; // ESlateVisibility::Collapsed
            context->ProcessEvent(hidden.function, hidden.data.data());
            return;
        }
    }
    bool reject(UObject* context, UFunction* fn, void* params) const {
        // Disable only the egg-menu interaction, not parent assignment, cake
        // storage, or the external incubation HUD. Keep container locks below.
        if (egg_interactions.contains(fn)) {
            const auto type = argument<uint8_t>(fn, params, L"IndicatorType");
            const auto name = indicator_types->GetNameByValue(type).ToString();
            return (name == STR("SetEgg") || name == STR("EPalInteractiveObjectIndicatorType::SetEgg")) && own_model(context);
        }
        if (obtain.contains(fn)) {
            // Check exact building identity even before discovery/attachment;
            // original ancient machines remain untouched.
            if (!live(context)) return false;
            auto* cls = find<UClass*>(STR("/Script/Pal.PalMapObjectMultiHatchingEggWithBreedModel"));
            if (!context->IsA(cls)) return false;
            Parameters args{find<UFunction*>(STR("/Script/Pal.PalMapObjectConcreteModelBase:TryGetMapObjectId"))};
            auto* ret = args.function->GetReturnProperty();
            require(ret && ret->GetElementSize() == sizeof(NativeName), "Ancient building identity ABI differs");
            context->ProcessEvent(args.function, args.data.data());
            NativeName id{};
            std::memcpy(&id, ret->ContainerPtrToValuePtr<void>(args.data.data()), sizeof(id));
            return FName{id.index, id.number}.ToString() == STR("PRF_ResourceBreedingFacility");
        }
        const auto request = requests.find(fn);
        if (request == requests.end()) return false;
        auto ours = [&](const st::Guid& id) {
            return std::any_of(locked.begin(), locked.end(), [&](const auto& value) {
                return value.model.Get() != nullptr && value.container == id;
            });
        };
        auto array = [&](const wchar_t* name) {
            auto* field = CastField<FArrayProperty>(fn->GetPropertyByName(name));
            require(field && field->GetInner()->GetElementSize() == sizeof(st::Consume), "Ancient input array changed");
            FScriptArrayHelper values{field, field->ContainerPtrToValuePtr<void>(params)};
            for (int i = 0; i < values.Num(); ++i) {
                st::Consume value{};
                std::memcpy(&value, values.GetRawPtr(i), sizeof(value));
                if (ours(value.slot.container)) return true;
            }
            return false;
        };
        switch (request->second) {
        case Request::Swap: return ours(argument<st::SlotId>(fn, params, L"SlotA").container) || ours(argument<st::SlotId>(fn, params, L"SlotB").container);
        case Request::Move: return array(L"Froms") || ours(argument<st::SlotId>(fn, params, L"To").container);
        case Request::MoveContainer: return array(L"Froms");
        case Request::Drop: return array(L"DropSlotAndNumArray");
        case Request::Dispose: return ours(argument<st::Consume>(fn, params, L"SlotInfo").slot.container);
        case Request::Filter: return ours(argument<st::Guid>(fn, params, L"ContainerId"));
        case Request::Sort: return std::any_of(locked.begin(), locked.end(), [&](const auto& value) { return value.module.Get() == context; });
        }
        return false;
    }
};
} // namespace ancient_breeder
