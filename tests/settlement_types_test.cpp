#include "../native/src/SettlementTypes.hpp"
#include <iostream>
#include <functional>
using namespace prf::settlement;

static void rejects(const std::function<void()>& fn) {
    bool failed = false;
    try { fn(); } catch (const std::runtime_error&) { failed = true; }
    verify_value(failed, "Expected refusal");
}
int main() {
    const Name wood{7, 0}, ore{8, 0};
    Counts left;
    add_count(left, wood, 6);
    add_count(left, ore, 2);
    ItemState existing{{wood, {}}, 97, 0.25F};
    ItemState stacked = existing;
    stacked.count = 99;
    account_addition(existing, stacked, left);
    verify_value(left.at(wood) == 4 && left.at(ore) == 2, "Stack delta or rejected-filter remainder lost");
    account_addition({}, {{wood, {}}, 4, 0}, left);
    account_addition({}, {{ore, {}}, 2, 0}, left);
    verify_value(left.empty(), "Split-container plan failed conservation");
    rejects([&] { account_addition({}, {{wood, {}}, 1, 0}, left); });
    Counts allowed{{wood, 5}, {ore, 5}};
    rejects([&] { account_addition(existing, {{ore, {}}, 99, 0}, allowed); });
    rejects([&] { account_addition(existing, {{wood, {}}, 96, 0}, allowed); });
    rejects([&] { account_addition({}, {{wood, {}}, 6, 0}, allowed); });
    auto dynamic = ItemState{{wood, {}}, 1, 0};
    dynamic.item.dynamic.local.words[0] = 1;
    const auto remaining_before_noop = allowed;
    account_addition(dynamic, dynamic, allowed);
    verify_value(allowed == remaining_before_noop, "Unchanged existing dynamic equipment must not block materials");
    rejects([&] { account_addition({}, dynamic, allowed); });
    rejects([&] { add_count(allowed, wood, std::numeric_limits<int32_t>::max()); });

    const std::string claim = "PRF_SETTLEMENT_1\n\"egg-key\"\nRESOLVING\n";
    const std::vector<SavedDrop> drops{{"Thermal_Core", 4}, {"PalUpgradeStone4", 3}};
    verify_value(parse_receipt(claim, "egg-key").phase == Phase::Resolving, "Interrupted RNG must stay claimed");
    const auto ready = claim + ready_record(drops);
    const auto loaded = parse_receipt(ready, "egg-key");
    verify_value(loaded.phase == Phase::Ready && loaded.drops == drops, "Saved native drop result changed");
    const auto unstarted = claim + "RNG_NOT_STARTED\n";
    verify_value(parse_receipt(unstarted, "egg-key").phase == Phase::RngNotStarted,
        "Normal unload before RNG must be distinguishable from interrupted RNG");
    verify_value(parse_receipt(unstarted + "RESOLVING\n" + ready_record(drops), "egg-key").drops == drops,
        "Unstarted claim cannot continue");
    const auto deferred = ready + "APPLYING\nAPPLY_NOT_STARTED\n";
    verify_value(parse_receipt(deferred, "egg-key").phase == Phase::Ready &&
        parse_receipt(deferred, "egg-key").drops == drops, "Normal unload lost the fixed native result");
    verify_value(parse_receipt(deferred + "APPLYING\nGROUND_REQUESTED\n", "egg-key").phase == Phase::GroundRequested,
        "Deferred fixed result cannot finish once");
    rejects([&] { parse_receipt(claim + "APPLY_NOT_STARTED\n", "egg-key"); });
    rejects([&] { parse_receipt(ready + "RNG_NOT_STARTED\n", "egg-key"); });
    rejects([&] { parse_receipt(deferred + "DROPS 0\nREADY\n", "egg-key"); });
    rejects([&] { parse_receipt(ready + "APPLYING\nGROUND_REQUESTED\nAPPLY_NOT_STARTED\n", "egg-key"); });
    verify_value(parse_receipt(ready + "APPLYING\n", "egg-key").phase == Phase::Applying, "Uncertain apply must not reset");
    verify_value(parse_receipt(ready + "APPLYING\nVERIFIED\n", "egg-key").phase == Phase::Verified, "Verified receipt lost");
    verify_value(parse_receipt(ready + "APPLYING\nGROUND_REQUESTED\n", "egg-key").phase == Phase::GroundRequested,
        "Issued ground output must remain non-replayable without claiming readback");
    verify_value(parse_receipt(claim + ready_record({}), "egg-key").drops.empty(), "Zero native drops must be valid");
    rejects([&] { parse_receipt(ready, "other-egg"); });
    rejects([&] { parse_receipt(ready + "DROPS 0\nREADY\n", "egg-key"); });
    rejects([&] { parse_receipt(ready + "VERIFIED\n", "egg-key"); });
    rejects([&] { parse_receipt(ready + "GROUND_REQUESTED\n", "egg-key"); });
    rejects([&] { parse_receipt(ready + "APPLYING\nGROUND_REQUESTED\nAPPLYING\n", "egg-key"); });
    rejects([&] { parse_receipt(ready + "APPLYING\nGROUND_REQUESTED\nVERIFIED\n", "egg-key"); });
    rejects([&] { parse_receipt(ready + "APPLYING\nGROUND_REQUES", "egg-key"); });
    rejects([&] { parse_receipt(ready + "APPLY", "egg-key"); });
    rejects([&] { parse_receipt(claim + "DROPS 2\n\"Thermal_Core\" 4\n", "egg-key"); });
    rejects([&] { parse_receipt(claim + "DROPS 1\n\"bad\" -1\nREADY\n", "egg-key"); });
    const auto straight = front_outlet({10.0, 20.0, 30.0}, {0.0, 0.0, 0.0, 1.0});
    verify_value(straight == std::array<double, 3>{210.0, 20.0, 90.0}, "Default outlet position differs");
    const double half = std::sqrt(0.5);
    const auto turned = front_outlet({10.0, 20.0, 30.0}, {0.0, 0.0, half, half});
    verify_value(std::abs(turned[0] - 10.0) < 1e-8 && std::abs(turned[1] - 220.0) < 1e-8 &&
        turned[2] == 90.0, "Outlet does not rotate with machine");
    rejects([&] { front_outlet({0, 0, 0}, {0, 0, 0, 0}); });
    rejects([&] { front_outlet({std::numeric_limits<double>::infinity(), 0, 0}, {0, 0, 0, 1}); });
    rejects([&] { front_outlet({0, 0, 0}, {0, 0, std::numeric_limits<double>::quiet_NaN(), 1}); });
    std::cout << "PASS native layouts, stack conservation, immutable receipt, ground request replay refusal and rotated outlet\n";
}
