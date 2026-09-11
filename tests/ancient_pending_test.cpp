// Exercise the production PendingSettlement lifecycle with a fake game/disk
// boundary. No Unreal objects, player saves or installed receipts are touched.
#include "../native/src/SettlementTypes.hpp"
#include <atomic>
#include <future>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>

namespace st = prf::settlement;
#define STR(value) L##value
enum class LogLevel { Normal, Warning, Error };
struct Output { template<LogLevel, class... T> static void send(T&&...) {} };
inline std::wstring to_wstring(const char*) { return {}; }
inline void require(bool ok, const char* why) { st::verify_value(ok, why); }

struct UObject {
    std::string receipt;
    std::vector<st::SavedDrop> fixed{{"NativeTestDrop", 2}};
    int rng{}, applied{}, output{};
    bool present{true}, fail_rng{}, fail_apply{}, fail_write{};
    std::shared_future<void> prepare_gate;
};
struct FWeakObjectPtr {
    UObject* value;
    FWeakObjectPtr(UObject* object) : value(object) {}
    UObject* Get() const { return value; }
};
class ManualSettlement {
    UObject& state;
    std::optional<st::Phase> prior;
public:
    struct Slot {};
    ManualSettlement(UObject* object, int, bool) : state(*object) {}
    bool load_or_claim() {
        if (state.receipt.empty()) {
            state.receipt = "PRF_SETTLEMENT_1\n\"egg\"\nRESOLVING\n";
            return false;
        }
        const auto old = st::parse_receipt(state.receipt, "egg");
        if (old.phase == st::Phase::RngNotStarted) {
            state.receipt += "RESOLVING\n";
            return false;
        }
        if (old.phase != st::Phase::Ready) prior = old.phase;
        else state.fixed = old.drops;
        return true;
    }
    auto prior_ancient_phase() const { return prior; }
    void validate_pending() { require(state.present, "source absent"); }
    void save_breeder_result(std::vector<st::SavedDrop> value) { state.fixed = std::move(value); }
    void persist_result() {
        if (state.prepare_gate.valid()) state.prepare_gate.get();
        if (state.fail_write) throw std::runtime_error("disk failed");
        state.receipt += st::ready_record(state.fixed);
    }
    void persist_applying() { state.receipt += "APPLYING\n"; }
    void persist_ground_requested() { state.receipt += "GROUND_REQUESTED\n"; }
    void persist_rng_not_started() { state.receipt += "RNG_NOT_STARTED\n"; }
    void persist_apply_not_started() { state.receipt += "APPLY_NOT_STARTED\n"; }
    void attempt(bool, bool source_only = false) {
        ++state.applied;
        if (state.fail_apply) throw std::runtime_error("native call failed");
        state.present = false;
        if (!source_only) ++state.output;
    }
};
namespace ancient_breeder {
bool completed_matches(UObject* object, int, const st::DynamicId&) { return object->present; }
std::vector<st::SavedDrop> calculate_claimed(UObject* object, int, st::DynamicId) {
    ++object->rng;
    if (object->fail_rng) throw std::runtime_error("native RNG failed");
    return object->fixed;
}
}
#include "../native/src/AncientBreederSettlement.hpp"

using ancient_breeder::PendingSettlement;
void check(bool value) { require(value, "pending settlement regression"); }
st::Phase phase(const UObject& object) { return st::parse_receipt(object.receipt, "egg").phase; }
void complete(PendingSettlement& pending) {
    while (!pending.poll()) std::this_thread::yield();
}
void calculated(PendingSettlement& pending, const UObject& object) {
    while (object.rng == 0) { check(!pending.poll()); std::this_thread::yield(); }
}
void expect_failure(PendingSettlement& pending) {
    bool threw{};
    try { complete(pending); } catch (const std::runtime_error&) { threw = true; }
    check(threw);
}
int main() {
    UObject unstarted;
    { PendingSettlement pending(&unstarted, 0, {}); }
    check(phase(unstarted) == st::Phase::RngNotStarted && unstarted.rng == 0 && unstarted.applied == 0);
    { PendingSettlement pending(&unstarted, 0, {}); complete(pending); }
    check(phase(unstarted) == st::Phase::GroundRequested && unstarted.rng == 1 && unstarted.output == 1);

    UObject prepared;
    std::promise<void> release;
    prepared.prepare_gate = release.get_future().share();
    {
        PendingSettlement pending(&prepared, 3, {});
        calculated(pending, prepared);
        // The worker is blocked. Poll must return without waiting for I/O.
        for (int i = 0; i < 1000; ++i) check(!pending.poll());
        check(prepared.applied == 0);
        release.set_value();
        // Exit before another game poll: retain the calculated result only.
    }
    check(phase(prepared) == st::Phase::Ready && prepared.rng == 1 && prepared.output == 0);
    const auto original = st::parse_receipt(prepared.receipt, "egg").drops;
    { PendingSettlement pending(&prepared, 3, {}); complete(pending); }
    check(phase(prepared) == st::Phase::GroundRequested && prepared.rng == 1 && prepared.output == 1);
    check(st::parse_receipt(prepared.receipt, "egg").drops == original);

    // Restoring the already-issued source never awards its drops again.
    prepared.present = true;
    { PendingSettlement pending(&prepared, 3, {}); complete(pending); }
    check(!prepared.present && prepared.rng == 1 && prepared.output == 1);

    for (bool applying : {false, true}) {
        UObject old;
        old.receipt = "PRF_SETTLEMENT_1\n\"egg\"\nRESOLVING\n";
        if (applying) old.receipt += st::ready_record(old.fixed) + "APPLYING\n";
        const auto before = old.receipt;
        { PendingSettlement pending(&old, 0, {}); complete(pending); check(pending.is_quarantined()); }
        check(old.receipt == before && old.rng == 0 && old.applied == 0);
    }
    for (bool after_rng : {false, true}) {
        UObject replaced;
        {
            PendingSettlement pending(&replaced, 0, {});
            if (after_rng) calculated(pending, replaced);
            replaced.present = false;
            complete(pending);
            check(pending.is_quarantined());
        }
        check(phase(replaced) == (after_rng ? st::Phase::Applying : st::Phase::Resolving));
        check(replaced.applied == 0 && replaced.output == 0);
    }
    UObject rng_failure;
    rng_failure.fail_rng = true;
    { PendingSettlement pending(&rng_failure, 0, {}); expect_failure(pending); }
    check(phase(rng_failure) == st::Phase::Resolving && rng_failure.rng == 1);
    UObject apply_failure;
    apply_failure.fail_apply = true;
    { PendingSettlement pending(&apply_failure, 0, {}); expect_failure(pending); }
    check(phase(apply_failure) == st::Phase::Applying && apply_failure.applied == 1 && apply_failure.output == 0);
    UObject disk_failure;
    disk_failure.fail_write = true;
    { PendingSettlement pending(&disk_failure, 0, {}); expect_failure(pending); }
    check(phase(disk_failure) == st::Phase::Resolving && disk_failure.applied == 0);
    std::cout << "PASS unload continuation, no reroll, no duplicate output, nonblocking poll, failed-call isolation (fake game boundary)\n";
}
