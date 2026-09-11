#pragma once
// Include after the native adapter. The lifecycle can also be tested against
// a fake game boundary without loading Unreal or touching a player's save.
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
    bool new_claim{}, rng_started{}, apply_started{}, failed{};
public:
    bool is_quarantined() const { return quarantined; }
    PendingSettlement(UObject* object, int index, st::DynamicId identity)
        : model(object), slot(index), egg(identity), transaction(object, index, true) {
        require(completed_matches(object, index, egg), "Cannot claim an unfinished or replaced offspring");
        disk.start([this] {
            const bool reused = transaction.load_or_claim();
            new_claim = !reused; // Read only after poll/drain joins this worker.
            return reused;
        });
    }
    ~PendingSettlement() {
        try {
            // Destruction already joined disk writes. Preserve only work whose
            // native call never started; do not touch the unloading world.
            disk.drain();
            if (failed || quarantined) return;
            if (phase == 0 && new_claim && !rng_started) {
                transaction.persist_rng_not_started();
                Output::send<LogLevel::Normal>(STR("[PRFAncient] SETTLEMENT_DEFERRED slot={} stage=before-rng game-writes=0\n"), slot);
            } else if (phase == 1 && !apply_started) {
                transaction.persist_apply_not_started();
                Output::send<LogLevel::Normal>(STR("[PRFAncient] SETTLEMENT_DEFERRED slot={} stage=before-apply fixed-result-retained=true game-writes=0\n"), slot);
            }
        } catch (const std::exception& e) {
            Output::send<LogLevel::Error>(STR("[PRFAncient] DEFER_FAILED {} no-replay=true\n"), to_wstring(e.what()));
        }
    }
    bool poll() try {
        const auto done = disk.poll();
        if (!done) return false;
        if (phase == 0) {
            require(model.Get() != nullptr, "Ancient breeder expired before settlement");
            if (!completed_matches(model.Get(), slot, egg)) {
                quarantined = true;
                Output::send<LogLevel::Warning>(STR("[PRFAncient] EGG_QUARANTINED slot={} source-changed=true other-eggs-continue=true\n"), slot);
                return true; // Retain receipt; never apply it to a different egg.
            }
            transaction.validate_pending();
            if (const auto prior = transaction.prior_ancient_phase()) {
                if (*prior == st::Phase::GroundRequested) {
                    apply_started = true;
                    transaction.attempt(true, true);
                }
                else {
                    quarantined = true;
                    Output::send<LogLevel::Error>(STR("[PRFAncient] EGG_QUARANTINED slot={} unresolved-receipt=true other-eggs-continue=true\n"), slot);
                }
                return true;
            }
            if (!*done) {
                rng_started = true;
                transaction.save_breeder_result(calculate_claimed(model.Get(), slot, egg));
            }
            phase = 1;
            disk.start([this, reused = *done] {
                if (!reused) transaction.persist_result();
                transaction.persist_applying();
                return true;
            });
        } else if (phase == 1) {
            require(model.Get() != nullptr, "Ancient breeder expired before source consumption");
            if (!completed_matches(model.Get(), slot, egg)) {
                quarantined = true;
                Output::send<LogLevel::Warning>(STR("[PRFAncient] EGG_QUARANTINED slot={} source-changed=true other-eggs-continue=true\n"), slot);
                return true;
            }
            apply_started = true;
            transaction.attempt(true);
            phase = 2;
            disk.start([this] { transaction.persist_ground_requested(); return true; });
        } else return true;
        return false;
    } catch (...) {
        failed = true; // A failed/partial native or disk call is not resumable.
        throw;
    }
};
} // namespace ancient_breeder
