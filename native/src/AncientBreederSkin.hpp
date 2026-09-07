#pragma once
#include <Unreal/Property/FStrProperty.hpp>
#include <Unreal/Core/Containers/Map.hpp>
// Visual skin for the custom ID only. Keeps the native actor/model/modules.
namespace ancient_breeder {
class Skin {
    FWeakObjectPtr mesh;
    std::vector<FWeakObjectPtr> applied;
    std::map<UObject*, uint8_t> material_states;
    void apply_state(UObject* actor) {
        auto* state_field=actor->GetPropertyByNameInChain(STR("CurrentState"));
        require(state_field && state_field->GetElementSize()==1,"Build state missing");
        const auto state=*state_field->ContainerPtrToValuePtr<uint8_t>(actor);
        if (material_states.contains(actor) && material_states[actor]==state) return;
        if (state==0) return; // Initialization has no world/material set yet.
        auto* body=visual_object(actor,STR("SM_EggHatchingMachineFuture"));
        if (!live(body)) return;
        UObject* surface=nullptr;
        if (state==1 || state==2) {
            auto* manager=object_call(find<UObject*>(STR("/Script/Pal.Default__PalUtility")),STR("/Script/Pal.PalUtility:GetMapObjectManager"),actor);
            if (!live(manager)) return;
            auto* set=CastField<FStructProperty>(manager->GetPropertyByNameInChain(STR("BuildingSurfaceMaterialSet")));
            auto* material=set ? CastField<FObjectProperty>(set->GetStruct()->GetPropertyByName(state==2 ? STR("Building") : STR("Highlight"))) : nullptr;
            require(material,"Native building surface material missing");
            surface=*material->ContainerPtrToValuePtr<UObject*>(set->ContainerPtrToValuePtr<void>(manager));
            if (!live(surface)) return;
        }
        auto* custom=load();
        auto* slots=CastField<FArrayProperty>(custom->GetPropertyByNameInChain(STR("StaticMaterials")));
        require(slots,"Skin material slots missing");
        FScriptArrayHelper values{slots,slots->ContainerPtrToValuePtr<void>(custom)};
        for (int i=0;i<values.Num();++i) {
            Parameters p{find<UFunction*>(STR("/Script/Engine.PrimitiveComponent:SetMaterial"))};
            *p.function->GetPropertyByName(STR("ElementIndex"))->ContainerPtrToValuePtr<int32_t>(p.data.data())=i;
            *p.function->GetPropertyByName(STR("Material"))->ContainerPtrToValuePtr<UObject*>(p.data.data())=surface;
            body->ProcessEvent(p.function,p.data.data());
        }
        material_states[actor]=state;
        Output::send<LogLevel::Normal>(STR("[PRFAncient] SKIN_STATE state={} slots={} hologram={}\n"),state,values.Num(),surface!=nullptr);
    }
    static void vector_arg(UObject* object,const wchar_t* fn,const wchar_t* key,NativeLocation value) {
        Parameters p{find<UFunction*>(fn)};
        auto* field=p.function->GetPropertyByName(key);
        require(field && field->GetElementSize()==sizeof(value),"Skin vector layout differs");
        std::memcpy(field->ContainerPtrToValuePtr<void>(p.data.data()),&value,sizeof(value));
        object->ProcessEvent(p.function,p.data.data());
    }
    UObject* load() {
        if (auto* value=mesh.Get()) return value;
        auto* library=find<UObject*>(STR("/Script/Engine.Default__KismetSystemLibrary"));
        Parameters path{find<UFunction*>(STR("/Script/Engine.KismetSystemLibrary:MakeSoftObjectPath"))};
        auto* text=CastField<FStrProperty>(path.function->GetPropertyByName(STR("PathString")));
        require(text,"Soft asset string input missing");
        *text->ContainerPtrToValuePtr<FString>(path.data.data())=FString{STR("/Game/PalResourceFactory/BreederVisual/SM_PRF_Breeder.SM_PRF_Breeder")};
        library->ProcessEvent(path.function,path.data.data());
        Parameters reference{find<UFunction*>(STR("/Script/Engine.KismetSystemLibrary:Conv_SoftObjPathToSoftObjRef"))};
        auto* reference_input=reference.function->GetPropertyByName(STR("SoftObjectPath"));
        auto* path_output=path.function->GetReturnProperty();
        require(reference_input && path_output && reference_input->GetElementSize()==path_output->GetElementSize(),"Soft path layout differs");
        reference_input->CopyCompleteValue(reference_input->ContainerPtrToValuePtr<void>(reference.data.data()),path_output->ContainerPtrToValuePtr<void>(path.data.data()));
        library->ProcessEvent(reference.function,reference.data.data());
        Parameters asset{find<UFunction*>(STR("/Script/Engine.KismetSystemLibrary:LoadAsset_Blocking"))};
        auto* input=asset.function->GetPropertyByName(STR("Asset"));
        auto* output=reference.function->GetReturnProperty();
        require(input && output && input->GetElementSize()==output->GetElementSize(),"Soft asset input differs");
        input->CopyCompleteValue(input->ContainerPtrToValuePtr<void>(asset.data.data()),output->ContainerPtrToValuePtr<void>(reference.data.data()));
        library->ProcessEvent(asset.function,asset.data.data());
        auto* ret=CastField<FObjectProperty>(asset.function->GetReturnProperty());
        require(ret,"Skin asset return differs");
        auto* value=*ret->ContainerPtrToValuePtr<UObject*>(asset.data.data());
        require(live(value) && value->IsA(find<UClass*>(STR("/Script/Engine.StaticMesh"))),"Custom breeder mesh not loaded");
        mesh=FWeakObjectPtr{value};
        return value;
    }
public:
    void clear() { applied.clear(); material_states.clear(); mesh=FWeakObjectPtr{}; }
    void refresh_states() {
        std::erase_if(applied,[](const auto& a){return a.Get()==nullptr;});
        for (const auto& weak:applied) if (auto* actor=weak.Get()) apply_state(actor);
    }
    void refresh(UObject* model) {
        auto* actor=object_call(model,STR("/Script/Pal.PalMapObjectConcreteModelBase:GetActor"));
        refresh_actor(actor);
    }
    void on_event(UObject* actor,UFunction* fn) {
        static const FName simulation{STR("OnStartSimulation")}, replication{STR("OnRep_CurrentState")};
        const auto name=fn->GetFName();
        if (!name.Equals(simulation) && !name.Equals(replication)) return;
        refresh_actor(actor);
    }
    void after_event(UObject* actor,UFunction* fn) {
        static const FName simulation{STR("OnStartSimulation")}, replication{STR("OnRep_CurrentState")};
        const auto name=fn->GetFName();
        if (!name.Equals(simulation) && !name.Equals(replication)) return;
        if (!std::any_of(applied.begin(),applied.end(),[&](const auto& a){return a.Get()==actor;})) return;
        material_states.erase(actor);
        apply_state(actor); // After the native callback, not before it overwrites materials.
    }
    void refresh_actor(UObject* actor) {
        if (!live(actor)) return;
        auto* id=CastField<FNameProperty>(actor->GetPropertyByNameInChain(STR("BuildObjectId")));
        if (!id || id->ContainerPtrToValuePtr<FName>(actor)->ToString()!=STR("PRF_ResourceBreedingFacility")) return;
        std::erase_if(applied,[](const auto& a){return !live(a.Get());});
        if (std::any_of(applied.begin(),applied.end(),[&](const auto& a){return a.Get()==actor;})) return;
        auto* custom=load();
        auto* body=visual_object(actor,STR("SM_EggHatchingMachineFuture"));
        require(live(body),"Native breeder body component missing");
        auto* mobility=body->GetPropertyByNameInChain(STR("Mobility"));
        require(mobility && mobility->GetElementSize()==1,"Body mobility missing");
        const auto previous=*mobility->ContainerPtrToValuePtr<uint8_t>(body);
        auto set_mobility=[&](uint8_t value) {
            Parameters p{find<UFunction*>(STR("/Script/Engine.SceneComponent:SetMobility"))};
            auto* input=p.function->GetPropertyByName(STR("NewMobility"));
            require(input && input->GetElementSize()==1,"Mobility input differs");
            *input->ContainerPtrToValuePtr<uint8_t>(p.data.data())=value;
            body->ProcessEvent(p.function,p.data.data());
        };
        set_mobility(2);
        Parameters set{find<UFunction*>(STR("/Script/Engine.StaticMeshComponent:SetStaticMesh"))};
        auto* field=CastField<FObjectProperty>(set.function->GetPropertyByName(STR("NewMesh")));
        require(field,"Set mesh input missing");
        *field->ContainerPtrToValuePtr<UObject*>(set.data.data())=custom;
        body->ProcessEvent(set.function,set.data.data());
        require(visual_object(body,STR("StaticMesh"))==custom,"Custom mesh readback differs");
        // Native visual control caches normal materials separately. Replace that
        // cache before it applies preview/construction materials to this mesh.
        auto* ctrl=visual_object(actor,STR("VisualCtrl"));
        if (live(ctrl)) {
            auto* prop=CastField<FMapProperty>(ctrl->GetPropertyByNameInChain(STR("NormalMaterialMapCache")));
            auto* value=prop ? CastField<FStructProperty>(prop->GetValueProp()) : nullptr;
            auto* materials=value ? CastField<FArrayProperty>(value->GetStruct()->GetPropertyByName(STR("Materials"))) : nullptr;
            require(materials,"Native normal material cache unavailable");
            auto* map=prop->ContainerPtrToValuePtr<FScriptMap>(ctrl);
            for (int i=0;i<map->GetMaxIndex();++i) {
                if (!map->IsValidIndex(i)) continue;
                auto* pair=static_cast<uint8_t*>(map->GetData(i,prop->GetMapLayout()));
                if (*reinterpret_cast<UObject**>(pair)!=body) continue;
                FScriptArrayHelper cached{materials,materials->ContainerPtrToValuePtr<void>(pair+prop->GetMapLayout().ValueOffset)};
                // Material slots, not mesh sections: GetStaticMaterials is not
                // reflected, but the StaticMaterials property is.
                auto* slots=CastField<FArrayProperty>(custom->GetPropertyByNameInChain(STR("StaticMaterials")));
                auto* slot=slots ? CastField<FStructProperty>(slots->GetInner()) : nullptr;
                auto* material=slot ? CastField<FObjectProperty>(slot->GetStruct()->GetPropertyByName(STR("MaterialInterface"))) : nullptr;
                require(material,"Custom mesh material slots unavailable");
                FScriptArrayHelper source{slots,slots->ContainerPtrToValuePtr<void>(custom)};
                StringType text=STR("(");
                for (int j=0;j<source.Num();++j) {
                    if (j) text+=STR(",");
                    FString item;
                    material->ExportTextItem_Direct(item,material->ContainerPtrToValuePtr<void>(source.GetRawPtr(j)),nullptr,custom,0);
                    text+=*item;
                }
                text+=STR(")");
                require(materials->ImportText_Direct(text.c_str(),materials->ContainerPtrToValuePtr<void>(pair+prop->GetMapLayout().ValueOffset),ctrl,0,nullptr)!=nullptr,"Normal material cache update failed");
            }
        }
        auto* overrides=CastField<FArrayProperty>(body->GetPropertyByNameInChain(STR("OverrideMaterials")));
        require(overrides,"Material overrides missing");
        FScriptArrayHelper array{overrides,overrides->ContainerPtrToValuePtr<void>(body)};
        for (int i=0;i<array.Num();++i) {
            Parameters mat{find<UFunction*>(STR("/Script/Engine.PrimitiveComponent:SetMaterial"))};
            auto* index=CastField<FIntProperty>(mat.function->GetPropertyByName(STR("ElementIndex")));
            require(index,"Material index missing");
            *index->ContainerPtrToValuePtr<int32_t>(mat.data.data())=i;
            body->ProcessEvent(mat.function,mat.data.data());
        }
        vector_arg(body,STR("/Script/Engine.SceneComponent:K2_SetRelativeLocation"),STR("NewLocation"),{0,0,0});
        vector_arg(body,STR("/Script/Engine.SceneComponent:K2_SetRelativeRotation"),STR("NewRotation"),{0,-90,0});
        vector_arg(body,STR("/Script/Engine.SceneComponent:SetRelativeScale3D"),STR("NewScale3D"),{1,1,1});
        set_mobility(previous);
        for (const auto* name:{STR("PalSphereLight"),STR("PalSphereLight1"),STR("PalEggChildActor"),STR("NS_HatchingEggFinishGlow")}) {
            auto* component=visual_object(actor,name);
            if (!live(component)) continue;
            Parameters hidden{find<UFunction*>(STR("/Script/Engine.SceneComponent:SetHiddenInGame"))};
            for (const auto* key:{STR("NewHidden"),STR("bPropagateToChildren")}) {
                auto* b=CastField<FBoolProperty>(hidden.function->GetPropertyByName(key));
                require(b,"Skin visibility input missing");
                b->SetPropertyValueInContainer(hidden.data.data(),true);
            }
            component->ProcessEvent(hidden.function,hidden.data.data());
        }
        applied.emplace_back(actor);
        material_states.erase(actor);
        // The replacement has a different slot count from the original mesh.
        // Apply the game's own construction surface to every replacement slot;
        // OnRep alone does not rebuild the original visual component's caches.
        Output::send<LogLevel::Normal>(STR("[PRFAncient] CUSTOM_SKIN_APPLIED mesh={} native-model-unchanged=true\n"),custom->GetFullName());
    }
};
}
