#pragma once
// Read the game's actual cake-effect row. No RNG or inventory operation.
// Included after SettlementRuntime.hpp, only by the independent breeder module.
struct BreederEffect {
    alignas(4) std::array<std::byte, 28> bytes{};
    template<class T> T field(UScriptStruct* type, const wchar_t* name) const {
        auto* property = type->GetPropertyByName(name);
        require(property && property->GetElementSize() == sizeof(T), "Breeding effect field differs");
        T value{};
        std::memcpy(&value, property->ContainerPtrToValuePtr<void>(const_cast<std::byte*>(bytes.data())), sizeof(T));
        return value;
    }
};

inline std::optional<BreederEffect> native_breeder_effect(UObject* model, NativeName item) {
    static_cast<void>(resolve_native()); // v1.0.4 PE identity, not an RNG call.
    auto* pal = find<UObject*>(STR("/Script/Pal.Default__PalUtility"));
    auto* settings = object_call(pal, STR("/Script/Pal.PalUtility:GetGameSetting"), model);
    auto* asset = read_field<UObject*>(settings, STR("BreedingItemEffectDataAsset"));
    require(live(asset), "Native breeding effects asset unavailable");
    auto* map = asset->GetPropertyByNameInChain(STR("ItemEffectMap"));
    require(map && map->GetOffset_Internal() == 0x30, "Native breeding effect map moved");
    auto* type = find<UScriptStruct*>(STR("/Script/Pal.PalBreedingItemEffectData"));
    require(type->GetStructureSize() == 28, "Native breeding effect structure differs");
    auto* base = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    constexpr std::array<uint8_t,16> prefix{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x74,0x24,0x10,0x57,0x48,0x83,0xec,0x20,0x8b};
    require(std::memcmp(base + 0x2de9110, prefix.data(), prefix.size()) == 0,
        "Native breeding effect lookup differs");
    // Audited full 155-byte function: FName hash + map lookup, returning
    // the borrowed 28-byte value or null. No allocating/consuming/RNG path.
    using Lookup = const void* (*)(UObject*, const NativeName*);
    const auto* value = reinterpret_cast<Lookup>(base + 0x2de9110)(asset, &item);
    if (!value) return std::nullopt;
    BreederEffect result;
    std::memcpy(result.bytes.data(), value, result.bytes.size());
    return result;
}

inline int native_breeder_count(const std::optional<BreederEffect>& effect) {
    // v1.0.4 farm tick uses one offspring when the native map returns null.
    // Normal Cake has no row; it must stay null when passed to native generation.
    if (!effect) return 1;
    return effect->field<int32_t>(find<UScriptStruct*>(STR("/Script/Pal.PalBreedingItemEffectData")), STR("BreedCount"));
}
