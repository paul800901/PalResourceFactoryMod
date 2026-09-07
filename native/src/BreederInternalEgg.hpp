#pragma once
#include "BreederOffspringSave.hpp"
#include "ProcessorNative.hpp" // Reuse timing queries only; no hooks or first-machine writes.

namespace breeder_native {
// Only an in-memory parameter object for the native hatch API. No dynamic
// subsystem registration, inventory slot, world actor, or pickup path exists.
class InternalEgg {
    RootedTemporary egg;
    UScriptStruct* type;
    bool mutation;
public:
    InternalEgg(UObject* model, const Offspring& offspring)
        : egg(checked_class(model), checked_outer(model)), type(offspring.type), mutation(offspring.mutation) {
        auto* save = CastField<FStructProperty>(egg.object->GetPropertyByNameInChain(STR("SaveParameter")));
        auto* character = egg.object->GetPropertyByNameInChain(STR("CharacterID"));
        require(save && save->GetStruct().Get() == type && save->GetOffset_Internal() == 0x78 &&
            character && character->GetOffset_Internal() == 0x70 && character->GetElementSize() == sizeof(NativeName),
            "Internal egg layout differs");
        require(offspring.character().index != 0, "Internal offspring has no native character");
        type->CopyScriptStruct(save->ContainerPtrToValuePtr<void>(egg.object), offspring.save.data());
        const auto id = offspring.character();
        std::memcpy(character->ContainerPtrToValuePtr<void>(egg.object), &id, sizeof(id));
    }
    static UObject* checked_outer(UObject* model) {
        require(live(model) && model->GetWorld(), "Internal egg world unavailable");
        return reinterpret_cast<UObject*>(model->GetWorld());
    }
    static UClass* checked_class(UObject* model) {
        require_breeder_server(model);
        static_cast<void>(resolve_native());
        return find<UClass*>(STR("/Script/Pal.PalDynamicPalEggItemDataBase"));
    }
    double incubation(UObject* model) const {
        require_breeder_server(model);
        return incubation_seconds(model, egg.object);
    }
    std::unique_ptr<Offspring> hatch(UObject* model) const {
        require_breeder_server(model);
        static_cast<void>(resolve_native());
        auto* base = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
        constexpr std::array<uint8_t,16> prefix{0x48,0x89,0x5c,0x24,0x08,0x48,0x89,0x54,0x24,0x10,0x55,0x56,0x57,0x41,0x54,0x41};
        require(std::memcmp(base+0x3035620, prefix.data(), prefix.size()) == 0, "Native hatch entry differs");
        // Audited v1.0.4 copies egg+0x78, then returns immediately when its
        // CharacterID is present. It does not read dynamic ID or item metadata
        // on this bred-egg branch and must not fall into wild-egg randomization.
        using Hatch = void* (*)(UObject*, void*, UObject*);
        alignas(16) std::array<std::byte,0x370> output{};
        reinterpret_cast<Hatch>(base+0x3035620)(model, output.data(), egg.object);
        struct Release { UScriptStruct* type; void* value; ~Release() { type->DestroyStruct(value); } } release{type, output.data()};
        auto result = std::make_unique<Offspring>(type);
        type->CopyScriptStruct(result->save.data(), output.data());
        require(result->character().index != 0, "Internal hatch returned no character; do not regenerate");
        auto* source = offspring_save_property()->ContainerPtrToValuePtr<void>(egg.object);
        require(offspring_save_property()->Identical(source, result->save.data(), 0),
            "Bred-egg hatch changed the fixed offspring; halt rather than reroll");
        result->mutation = mutation;
        return result;
    }
};
} // namespace breeder_native
