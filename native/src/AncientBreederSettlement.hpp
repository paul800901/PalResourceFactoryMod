#pragma once
#include "AncientBreederAdapter.hpp"
#include "AsyncDisk.hpp"

namespace ancient_breeder {
// Same durable stages as the first machine. Only the input is different:
// use the native already-completed offspring, never hatch it again.
class PendingSettlement {
    FWeakObjectPtr model;
    int slot;
    st::DynamicId egg;
    ManualSettlement transaction;
    int phase{};
    AsyncDisk disk; // Joins before transaction destruction.
    bool quarantined{};
public:
    bool is_quarantined() const { return quarantined; }
    PendingSettlement(UObject* object, int index, st::DynamicId identity)
        : model(object), slot(index), egg(identity), transaction(object, index, true) {
        auto finished = completed(object, index);
        require(finished && finished->egg == egg, "Cannot claim an unfinished or replaced offspring");
        disk.start([this] { return transaction.load_or_claim(); });
    }
    bool poll() {
        const auto done = disk.poll();
        if (!done) return false;
        if (phase == 0) {
            require(model.Get() != nullptr, "Ancient breeder expired before settlement");
            auto finished = completed(model.Get(), slot);
            if (!finished || finished->egg != egg) {
                quarantined = true;
                Output::send<LogLevel::Warning>(STR("[PRFAncient] EGG_QUARANTINED slot={} source-changed=true other-eggs-continue=true\n"), slot);
                return true; // Retain receipt; never apply it to a different egg.
            }
            transaction.validate_pending();
            if (const auto prior = transaction.prior_ancient_phase()) {
                if (*prior == st::Phase::GroundRequested) transaction.attempt(true, true);
                else {
                    quarantined = true;
                    Output::send<LogLevel::Error>(STR("[PRFAncient] EGG_QUARANTINED slot={} unresolved-receipt=true other-eggs-continue=true\n"), slot);
                }
                return true;
            }
            if (!*done) transaction.save_breeder_result(calculate_claimed(model.Get(), slot, egg));
            phase = 1;
            disk.start([this, reused = *done] {
                if (!reused) transaction.persist_result();
                transaction.persist_applying();
                return true;
            });
        } else if (phase == 1) {
            require(model.Get() != nullptr, "Ancient breeder expired before source consumption");
            auto finished = completed(model.Get(), slot);
            if (!finished || finished->egg != egg) {
                quarantined = true;
                Output::send<LogLevel::Warning>(STR("[PRFAncient] EGG_QUARANTINED slot={} source-changed=true other-eggs-continue=true\n"), slot);
                return true;
            }
            transaction.attempt(true);
            phase = 2;
            disk.start([this] { transaction.persist_ground_requested(); return true; });
        } else return true;
        return false;
    }
};
} // namespace ancient_breeder
