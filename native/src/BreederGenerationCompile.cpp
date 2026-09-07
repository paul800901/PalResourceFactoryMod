#include "SettlementRuntime.hpp"
#include "BreederGeneration.hpp"
#include "BreederOffspringSave.hpp"
#include "BreederInternalEgg.hpp"

// Compile the real call body without adding a hotkey, hook or startup call.
// This object target is not a DLL and is not deployed to the game.
auto compile_breeder_generation(UObject* model, const std::optional<BreederEffect>& effect) {
    return breeder_native::generate_one(model, effect);
}

auto compile_breeder_save(const breeder_native::Offspring& offspring) {
    return breeder_native::save_offspring(offspring);
}

auto compile_breeder_hatch(UObject* model, const breeder_native::Offspring& offspring) {
    breeder_native::InternalEgg egg{model, offspring};
    const auto duration = egg.incubation(model);
    require(duration >= 0, "Invalid native incubation duration");
    return egg.hatch(model);
}
