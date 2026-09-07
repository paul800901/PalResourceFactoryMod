#include "SettlementRuntime.hpp"
#include "AncientBreederAdapter.hpp"
#include "AncientBreederSettlement.hpp"
#include "AncientBreederInputGuard.hpp"

// Compile-only: no DLL, hook, RNG or installed game mutation.
auto compile_ancient_completed(UObject* model, int slot) {
    return ancient_breeder::completed(model, slot);
}

auto compile_ancient_butcher(UObject* model, int slot, st::DynamicId claimed_egg) {
    return ancient_breeder::calculate_claimed(model, slot, claimed_egg);
}

auto compile_ancient_transaction(UObject* model, int slot, st::DynamicId egg) {
    return std::make_unique<ancient_breeder::PendingSettlement>(model, slot, egg);
}

bool compile_ancient_poll(ancient_breeder::PendingSettlement& pending) { return pending.poll(); }

bool compile_ancient_guard(ancient_breeder::InputGuard& guard, UObject* model, UFunction* fn, void* params) {
    guard.initialize();
    guard.attach(model);
    return guard.reject(model, fn, params);
}
