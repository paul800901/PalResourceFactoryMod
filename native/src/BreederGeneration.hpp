#pragma once
#include "BreederEffects.hpp"
#include <memory>

// Game-thread adapter primitive, NOT an installed production loop.
// Caller must durably claim the batch and consume its cake before calling;
// it must not retry a failed/interrupted native RNG invocation.
namespace breeder_native {
struct Offspring {
    UScriptStruct* type;
    alignas(16) std::array<std::byte, 0x370> save{};
    bool mutation{};
    explicit Offspring(UScriptStruct* structure, void* (*initialize)(void*)) : type(structure) {
        initialize(save.data());
    }
    explicit Offspring(UScriptStruct* structure) : type(structure) {
        require(type && type->GetStructureSize() == save.size(), "Offspring restore layout differs");
        type->InitializeStruct(save.data());
    }
    ~Offspring() { type->DestroyStruct(save.data()); }
    Offspring(const Offspring&) = delete;
    Offspring& operator=(const Offspring&) = delete;
    NativeName character() const {
        NativeName result{};
        std::memcpy(&result, save.data(), sizeof(result));
        return result;
    }
};

inline void require_breeder_server(UObject* model) {
    require(live(model) && model->GetWorld() && !read_bool_field(model, STR("bDisposed")),
        "Breeder model is unavailable");
    // Never perform RNG against the original farm or the first machine.
    Parameters identity{find<UFunction*>(STR("/Script/Pal.PalMapObjectConcreteModelBase:TryGetMapObjectId"))};
    auto* identity_return = identity.function->GetReturnProperty();
    require(identity_return && identity_return->GetElementSize() == sizeof(NativeName), "Breeder identity layout differs");
    model->ProcessEvent(identity.function, identity.data.data());
    NativeName id{};
    std::memcpy(&id, identity_return->ContainerPtrToValuePtr<void>(identity.data.data()), sizeof(id));
    require(FName{id.index, id.number}.ToString() == STR("PRF_ResourceBreedingFacility"),
        "Native breeding generation is restricted to the second PRF building");
    auto* pal = find<UObject*>(STR("/Script/Pal.Default__PalUtility"));
    require(bool_call(pal, STR("/Script/Pal.PalUtility:IsServer"), model), "Client must not generate offspring");
}

inline std::unique_ptr<Offspring> generate_one(UObject* model, const std::optional<BreederEffect>& effect) {
    require_breeder_server(model);
    auto* workee = object_call(model, STR("/Script/Pal.PalMapObjectConcreteModelBase:GetWorkeeModule"));
    require(live(workee), "Breeder workee unavailable");
    auto* work = object_call(workee, STR("/Script/Pal.PalMapObjectWorkeeModule:GetWork"));
    require(live(work), "Breeder work unavailable");
    auto* utility = find<UObject*>(STR("/Script/Pal.Default__PalBreedingUtility"));
    Parameters pair{find<UFunction*>(STR("/Script/Pal.PalBreedingUtility:CanProceedBreeding"))};
    auto* work_arg = CastField<FObjectProperty>(pair.function->GetPropertyByName(STR("Work")));
    auto* eligible = CastField<FBoolProperty>(pair.function->GetReturnProperty());
    require(work_arg && eligible, "Breeder eligibility layout differs");
    *work_arg->ContainerPtrToValuePtr<UObject*>(pair.data.data()) = work;
    utility->ProcessEvent(pair.function, pair.data.data());
    require(eligible->GetPropertyValueInContainer(pair.data.data()), "Parents are no longer eligible");

    static_cast<void>(resolve_native()); // Current PE identity before unreflected calls.
    auto* base = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    constexpr std::array<uint8_t,16> init_prefix{0x48,0x89,0x5c,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x48,0x89,0x4c,0x24,0x08,0x57};
    constexpr std::array<uint8_t,16> mutation_prefix{0x48,0x83,0xec,0x48,0x0f,0x29,0x74,0x24,0x30,0x0f,0x29,0x7c,0x24,0x20,0x0f,0x28};
    constexpr std::array<uint8_t,16> generate_prefix{0x40,0x55,0x53,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xac};
    require(std::memcmp(base+0x2847780, init_prefix.data(), 16) == 0 &&
        std::memcmp(base+0x2dfcdb0, mutation_prefix.data(), 16) == 0 &&
        std::memcmp(base+0x2de98f0, generate_prefix.data(), 16) == 0, "Native breeding entry differs");
    auto* structure = find<UScriptStruct*>(STR("/Script/Pal.PalIndividualCharacterSaveParameter"));
    require(structure->GetStructureSize() == 0x370, "Native offspring save layout differs");
    float bonus{};
    if (effect) bonus = effect->field<float>(find<UScriptStruct*>(STR("/Script/Pal.PalBreedingItemEffectData")), STR("MutationRateBonusPercent"));
    require(std::isfinite(bonus), "Native cake mutation bonus is invalid");
    // The full farm wrapper applies this same percent-to-ratio conversion.
    float native_percent_scale{};
    std::memcpy(&native_percent_scale, base+0x674dd2c, sizeof(float));
    require(native_percent_scale == 0.01f, "Native breeding percent scale differs");
    using Initialize = void* (*)(void*);
    using Mutation = bool (*)(UObject*, float);
    using Generate = bool (*)(UObject*, UObject*, bool, const void* const*, void*);
    auto result = std::make_unique<Offspring>(structure, reinterpret_cast<Initialize>(base+0x2847780));
    result->mutation = reinterpret_cast<Mutation>(base+0x2dfcdb0)(model, bonus*native_percent_scale);
    const void* effect_ptr = effect ? effect->bytes.data() : nullptr;
    require(reinterpret_cast<Generate>(base+0x2de98f0)(work, model, result->mutation, &effect_ptr, result->save.data()),
        "Native offspring generation failed; do not retry this claimed batch");
    require(result->character().index != 0, "Native offspring is empty; do not reroll");
    // This is native save data, not a registered dynamic egg. Do not serialize
    // its raw bytes (they own Unreal allocations), or hand it to egg settlement.
    // Deliberately omit wrapper 0x3021510: it spawns a public world egg and
    // increments player statistics. Neither is part of this primitive.
    return result;
}
} // namespace breeder_native
