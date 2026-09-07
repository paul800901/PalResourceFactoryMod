#pragma once
// Visual-only treatment of the native menu; the server obtain guard remains
// authoritative. Never scan inventory children or touch ordinary incubators.
namespace ancient_breeder {
inline UObject* visual_object(UObject* object, const wchar_t* name) {
    if (!live(object)) return nullptr;
    auto* field = CastField<FObjectProperty>(object->GetPropertyByNameInChain(name));
    return field ? *field->ContainerPtrToValuePtr<UObject*>(object) : nullptr;
}
class MenuVisual {
    struct View { FWeakObjectPtr menu, button; bool owned{}, enabled{}; float opacity{1}; st::Guid container{}; bool set_all{}; };
    struct ImageState { FWeakObjectPtr image,button; std::unique_ptr<Parameters> brush,color; };
    std::vector<ImageState> originals;
    FWeakObjectPtr update_function;
    int32_t update_hook{-1};
    std::vector<View> views;
    std::mutex mutex;
    std::map<st::Guid, FWeakObjectPtr> containers;
    bool owns(UObject* menu) {
        // The menu keeps its inventory/model references; MapObjectConcreteModel
        // alone was not populated in the tested menu, so identify its container.
        for (TFieldIterator<FProperty> it{menu->GetClassPrivate()}; it; ++it) {
            auto* prop = CastField<FObjectProperty>(*it);
            if (!prop) continue;
            auto* value = *prop->ContainerPtrToValuePtr<UObject*>(menu);
            if (!live(value)) continue;
            if (value->IsA(find<UClass*>(STR("/Script/Pal.PalItemContainer")))) {
                auto found = containers.find(read_field<st::Guid>(value, STR("ID")));
                if (found != containers.end() && live(found->second.Get())) return true;
            }
            for (const auto& [id,model] : containers) if (value == model.Get()) return true;
        }
        return false;
    }
    void paint(UObject* button, bool enabled, float opacity, bool gray_style=true) {
        auto* fn = find<UFunction*>(STR("/Script/UMG.Widget:SetIsEnabled"));
        Parameters args{fn};
        auto* field = CastField<FBoolProperty>(fn->GetPropertyByName(STR("bInIsEnabled")));
        require(field, "Button enable field unavailable");
        field->SetPropertyValueInContainer(args.data.data(), enabled);
        button->ProcessEvent(fn, args.data.data());
        fn = find<UFunction*>(STR("/Script/UMG.Widget:SetRenderOpacity"));
        Parameters fade{fn};
        auto* value = CastField<FFloatProperty>(fn->GetPropertyByName(STR("InOpacity")));
        require(value, "Button opacity unavailable");
        *value->ContainerPtrToValuePtr<float>(fade.data.data()) = opacity;
        button->ProcessEvent(fn, fade.data.data());
        if (!enabled && gray_style) {
            Parameters stop{find<UFunction*>(STR("/Script/UMG.UserWidget:StopAllAnimations"))};
            button->ProcessEvent(stop.function,stop.data.data());
            // Keep texture alpha: removing it made decorative overlays opaque
            // and hid the caption. Tint the original brush, stopping its pulse.
            for (TFieldIterator<FProperty> it{button->GetClassPrivate()}; it; ++it) {
                auto* p = CastField<FObjectProperty>(*it);
                if (!p) continue;
                auto* image = *p->ContainerPtrToValuePtr<UObject*>(button);
                if (!live(image) || !image->IsA(find<UClass*>(STR("/Script/UMG.Image")))) continue;
                auto* brush = CastField<FStructProperty>(image->GetPropertyByNameInChain(STR("Brush")));
                require(brush, "Button image brush missing");
                Parameters b{find<UFunction*>(STR("/Script/UMG.Image:SetBrush"))};
                auto* input = CastField<FStructProperty>(b.function->GetPropertyByName(STR("InBrush")));
                require(input && input->GetStruct()==brush->GetStruct(), "Brush type differs");
                auto* data = input->ContainerPtrToValuePtr<void>(b.data.data());
                input->CopyCompleteValue(data, brush->ContainerPtrToValuePtr<void>(image));
                if (std::none_of(originals.begin(),originals.end(),[&](const auto& s){return s.image.Get()==image;})) {
                    ImageState saved{FWeakObjectPtr{image},FWeakObjectPtr{button},std::make_unique<Parameters>(b.function),
                        std::make_unique<Parameters>(find<UFunction*>(STR("/Script/UMG.Image:SetColorAndOpacity")))};
                    input->CopyCompleteValue(input->ContainerPtrToValuePtr<void>(saved.brush->data.data()),data);
                    auto* color=image->GetPropertyByNameInChain(STR("ColorAndOpacity"));
                    auto* dst=saved.color->function->GetPropertyByName(STR("InColorAndOpacity"));
                    require(color && dst && color->GetElementSize()==16 && dst->GetElementSize()==16,"Image color capture differs");
                    dst->CopyCompleteValue(dst->ContainerPtrToValuePtr<void>(saved.color->data.data()),color->ContainerPtrToValuePtr<void>(image));
                    originals.push_back(std::move(saved));
                }
                auto* resource = CastField<FObjectProperty>(brush->GetStruct()->GetPropertyByName(STR("ResourceObject")));
                require(resource, "Brush resource missing");
                auto* tint=CastField<FStructProperty>(brush->GetStruct()->GetPropertyByName(STR("TintColor")));
                require(tint,"Brush tint missing");
                auto* tint_data=tint->ContainerPtrToValuePtr<void>(data);
                auto* specified=tint->GetStruct()->GetPropertyByName(STR("SpecifiedColor"));
                auto* rule=tint->GetStruct()->GetPropertyByName(STR("ColorUseRule"));
                require(specified && specified->GetElementSize()==16 && rule && rule->GetElementSize()==1,"Slate color layout differs");
                const std::array<float,4> white{.22f,.22f,.22f,1};
                std::memcpy(specified->ContainerPtrToValuePtr<void>(tint_data),white.data(),16);
                *rule->ContainerPtrToValuePtr<uint8_t>(tint_data)=0;
                image->ProcessEvent(b.function,b.data.data());
                Parameters color{find<UFunction*>(STR("/Script/UMG.Image:SetColorAndOpacity"))};
                auto* c=color.function->GetPropertyByName(STR("InColorAndOpacity"));
                require(c && c->GetElementSize()==16,"Image color differs");
                const std::array<float,4> gray{1,1,1,1};
                std::memcpy(c->ContainerPtrToValuePtr<void>(color.data.data()),gray.data(),16);
                image->ProcessEvent(color.function,color.data.data());
            }
        }
    }
public:
    ~MenuVisual() { clear(); }
    void add(UObject* widget) {
        std::scoped_lock lock{mutex};
        views.push_back({FWeakObjectPtr{widget}});
        View left{FWeakObjectPtr{widget}}; left.set_all=true; views.push_back(left);
    }
    void attach(UObject* model, st::Guid id) { containers[id]=FWeakObjectPtr{model}; }
    void clear() {
        if (auto* fn=update_function.Get(); fn && update_hook>=0) static_cast<void>(static_cast<UFunction*>(fn)->UnregisterHook(update_hook));
        update_hook=-1; update_function=FWeakObjectPtr{};
        std::scoped_lock lock{mutex}; views.clear(); containers.clear(); originals.clear();
    }
    void refresh() {
        std::scoped_lock lock{mutex};
        std::erase_if(views, [](const auto& v) { return !live(v.menu.Get()); });
        for (auto& view : views) {
            auto* menu = view.menu.Get();
            if (update_hook<0) {
                auto* fn=menu->GetFunctionByNameInChain(STR("UpdateSlots"));
                require(fn,"Incubator UpdateSlots missing");
                update_function=FWeakObjectPtr{fn};
                update_hook=fn->RegisterPreHook([this,fn](UnrealScriptFunctionCallableContext& call,void*) {
                    for (TFieldIterator<FProperty> it{fn};it;++it) {
                        auto* p=*it;
                        if (!p->HasAnyPropertyFlags(CPF_Parm) || p->HasAnyPropertyFlags(CPF_ReturnParm)) continue;
                        if (p->GetElementSize()!=sizeof(st::Guid) || !CastField<FStructProperty>(p)) break;
                        st::Guid id{}; std::memcpy(&id,p->ContainerPtrToValuePtr<void>(call.TheStack.Locals()),sizeof(id));
                        std::scoped_lock lock{mutex};
                        for (auto& v:views) if(v.menu.Get()==call.Context) v.container=id;
                        break;
                    }
                });
                require(update_hook>=0,"Menu container tracking hook failed");
            }
            auto* model = visual_object(menu, STR("MapObjectConcreteModel"));
            bool owned=owns(menu);
            if (auto found=containers.find(view.container);found!=containers.end()) owned|=live(found->second.Get());
            if (live(model)) {
                Parameters id{find<UFunction*>(STR("/Script/Pal.PalMapObjectConcreteModelBase:TryGetMapObjectId"))};
                model->ProcessEvent(id.function, id.data.data());
                NativeName name{};
                auto* ret = id.function->GetReturnProperty();
                require(ret && ret->GetElementSize() == sizeof(name), "Menu model identity differs");
                std::memcpy(&name, ret->ContainerPtrToValuePtr<void>(id.data.data()), sizeof(name));
                owned |= FName{name.index, name.number}.ToString() == STR("PRF_ResourceBreedingFacility");
            }
            auto* button = visual_object(menu, view.set_all?STR("WBP_CommonButton_SetAll"):STR("WBP_CommonButton_OpenAll"));
            if (!live(button)) continue;
            if (owned) {
                if (!view.owned || view.button.Get() != button) {
                    view.enabled = read_bool_field(button, STR("bIsEnabled"));
                    view.opacity = read_field<float>(button, STR("RenderOpacity"));
                    view.button = FWeakObjectPtr{button};
                    Output::send<LogLevel::Normal>(STR("[PRFAncient] OBTAIN_BUTTON_DISABLED visual-only=true\n"));
                }
                paint(button, false, 1.f);
            } else if (view.owned && live(view.button.Get())) {
                for (auto& saved:originals) if(saved.button.Get()==view.button.Get() && live(saved.image.Get())) {
                    saved.image.Get()->ProcessEvent(saved.brush->function,saved.brush->data.data());
                    saved.image.Get()->ProcessEvent(saved.color->function,saved.color->data.data());
                }
                paint(view.button.Get(),view.enabled,view.opacity,false);
            }
            view.owned = owned;
        }
    }
};
} // namespace ancient_breeder
