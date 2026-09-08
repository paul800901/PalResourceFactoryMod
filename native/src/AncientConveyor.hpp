#pragma once
#include "ProcessorState.hpp"

namespace ancient_breeder {
// Only idle conveyor bookkeeping, never settlement receipts. Call only when
// no settlement is pending. Clear all stale slots before admitting any eggs
// so native slot moves cannot collide with an old copy in another slot.
inline void reconcile_conveyor(prf::processor::State& state,
    const std::array<std::string, 54>& current) {
    for (size_t i = 0; i < state.jobs.size(); ++i)
        if (state.jobs[i].egg != current[i]) state.empty(i);
}
}
