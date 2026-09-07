#pragma once
#include <Unreal/Engine/UDataTable.hpp>
// Use the loaded game's building/technology data, not a guessed seconds value.
inline bool apply_build_work_balance() {
    std::vector<UObject*> objects;
    UObjectGlobals::FindAllOf(STR("DataTable"), objects);
    std::map<StringType, int> levels;
    for (auto* object : objects) {
        if (!live(object)) continue;
        auto* table = static_cast<UDataTable*>(object);
        auto* type = table->GetRowStruct().Get();
        if (!type) continue;
        auto* level = CastField<FIntProperty>(type->GetPropertyByNameInChain(STR("LevelCap")));
        auto* unlocks = CastField<FArrayProperty>(type->GetPropertyByName(STR("UnlockBuildObjects")));
        if (!level || !unlocks || !CastField<FNameProperty>(unlocks->GetInner())) continue;
        for (const auto& pair : table->GetRowMap()) {
            int n = *level->ContainerPtrToValuePtr<int32_t>(pair.Value);
            if (n != 26 && n != 36) continue;
            FScriptArrayHelper ids{unlocks, unlocks->ContainerPtrToValuePtr<void>(pair.Value)};
            for (int i=0;i<ids.Num();++i) levels[reinterpret_cast<FName*>(ids.GetRawPtr(i))->ToString()]=n;
        }
    }
    if (levels.empty()) return false;
    float maxima[2]{};
    StringType references[2];
    std::vector<std::pair<float*, int>> targets;
    for (auto* object : objects) {
        if (!live(object) || object->GetName() != STR("DT_BuildObjectDataTable")) continue;
        auto* table=static_cast<UDataTable*>(object);
        auto* type=table->GetRowStruct().Get();
        auto* work=type ? CastField<FFloatProperty>(type->GetPropertyByName(STR("RequiredBuildWorkAmount"))) : nullptr;
        if (!work) continue;
        for (const auto& pair : table->GetRowMap()) {
            auto name=pair.Key.ToString();
            auto* value=work->ContainerPtrToValuePtr<float>(pair.Value);
            if (name==STR("PRF_EggResourceProcessor")) targets.emplace_back(value,0);
            else if (name==STR("PRF_ResourceBreedingFacility")) targets.emplace_back(value,1);
            else if (!name.starts_with(STR("PRF_"))) {
                auto level=levels.find(name);
                if (level==levels.end()) continue;
                int i=level->second==26 ? 0 : 1;
                if (std::isfinite(*value) && *value>maxima[i]) { maxima[i]=*value; references[i]=name; }
            }
        }
    }
    if (targets.size()!=2 || maxima[0]<=0 || maxima[1]<=0) return false;
    for (auto [target,i] : targets) *target=maxima[i];
    Output::send<LogLevel::Normal>(STR("[PRFBalance] BUILD_WORK level26={} reference={} level36={} reference={} custom-rows=2\n"),maxima[0],references[0],maxima[1],references[1]);
    return true;
}
