#pragma once
#include "BreederGeneration.hpp"

namespace breeder_native {
// Engine text serialization preserves FNames and allocated arrays/strings,
// unlike a byte copy of the native save structure. Game-thread calls only.
// This is a payload for the batch checkpoint, not a registered inventory egg.
struct SavedOffspring {
    std::wstring parameter;
    bool mutation{};
};

inline FStructProperty* offspring_save_property() {
    auto* cls = find<UClass*>(STR("/Script/Pal.PalDynamicPalEggItemDataBase"));
    auto* property = CastField<FStructProperty>(cls->GetPropertyByName(STR("SaveParameter")));
    auto* type = find<UScriptStruct*>(STR("/Script/Pal.PalIndividualCharacterSaveParameter"));
    require(property && property->GetStruct().Get() == type && type->GetStructureSize() == 0x370,
        "Offspring serialization property differs");
    return property;
}

inline std::unique_ptr<Offspring> restore_offspring(const SavedOffspring& saved) {
    require(!saved.parameter.empty() && saved.parameter.find(L'\0') == std::wstring::npos,
        "Empty or embedded-null offspring checkpoint");
    auto* property = offspring_save_property();
    auto result = std::make_unique<Offspring>(property->GetStruct().Get());
    const auto* end = property->ImportText_Direct(saved.parameter.c_str(), result->save.data(), nullptr, 0, nullptr);
    require(end && *end == L'\0', "Native offspring checkpoint import failed or incomplete");
    require(result->character().index != 0, "Restored offspring has no character; no regeneration permitted");
    result->mutation = saved.mutation;
    return result;
}

inline SavedOffspring save_offspring(const Offspring& offspring) {
    auto* property = offspring_save_property();
    require(offspring.type == property->GetStruct().Get() && offspring.character().index != 0,
        "Cannot persist an invalid native offspring");
    FString text;
    property->ExportTextItem_Direct(text, offspring.save.data(), nullptr, nullptr, 0);
    SavedOffspring saved{std::wstring{*text}, offspring.mutation};
    // Do not accept an export merely because it is nonempty. Check native
    // equality before releasing the RNG result or allowing further processing.
    // If engine text omits a meaningful field, stop here rather than reroll.
    auto restored = restore_offspring(saved);
    require(property->Identical(offspring.save.data(), restored->save.data(), 0),
        "Native offspring text round trip changed data; retain original, do not reroll");
    return saved;
}
} // namespace breeder_native
