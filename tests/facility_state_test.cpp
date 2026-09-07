#include "FacilityState.hpp"

#include <cstdlib>
#include <iostream>
#include <optional>

namespace {

void require(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

prf::EggJob make_job(const std::uint64_t id, const double incubation = 10.0, const double disassembly = 3.0) {
    return prf::EggJob{
        .id = id,
        .stage = prf::JobStage::Incubating,
        .remaining_incubation_seconds = incubation,
        .remaining_disassembly_seconds = disassembly,
        .opaque_game_payload = "opaque",
        .native_drops = std::nullopt,
    };
}

// Test fixture only. The game adapter must supply real native results instead.
prf::TickResult tick_with_result(prf::FacilityState& state, double elapsed, bool storage = true) {
    auto result = state.tick(elapsed, true, storage);
    if (result.drop_resolution_requested) {
        require(state.begin_drop_resolution(*result.drop_resolution_requested), "native calculation must be claimed once");
        require(state.record_native_drops(*result.drop_resolution_requested, {{"fixture-material", 2}}), "native result should be recorded");
        result.reward_commit_requested = state.tick(0.001, true, storage).reward_commit_requested;
    }
    return result;
}

void egg_processor_locks_and_freezes() {
    prf::FacilityState state(prf::egg_processor_config());
    require(state.current_power_per_second() == 200, "idle processor should draw 200/s");
    require(state.deposit_egg(make_job(1)), "first egg should be accepted");
    require(state.current_power_per_second() == 400, "incubating processor should draw 400/s");
    require(!state.deposit_egg(make_job(1)), "duplicate job id must be rejected");
    require(state.tick(20.0, false, true).became_ready_for_disassembly.empty(), "power loss must freeze incubation");
    require(state.job_count() == 1, "frozen egg must remain locked");
}

void vegetable_cake_requires_two_slots() {
    prf::FacilityState state(prf::breeding_factory_config());
    require(!state.consume_cake_and_enqueue(prf::CakeKind::Unsupported, make_job(1), std::nullopt), "unsupported cake must be rejected");
    require(!state.consume_cake_and_enqueue(prf::CakeKind::Vegetable, make_job(1), std::nullopt), "vegetable cake requires two eggs");
    require(state.consume_cake_and_enqueue(prf::CakeKind::Vegetable, make_job(1), make_job(2)), "vegetable cake should enqueue two eggs");
    require(state.job_count() == 2, "two internal incubation slots should be occupied");
    require(state.current_power_per_second() == 1000, "two concurrent eggs still draw a fixed 1000/s total");
    require(!state.consume_cake_and_enqueue(prf::CakeKind::Normal, make_job(3), std::nullopt), "full incubator must not consume another cake");
}

void zero_hatch_time_does_not_create_an_unbounded_cake_queue() {
    prf::FacilityState state(prf::breeding_factory_config());
    require(state.consume_cake_and_enqueue(prf::CakeKind::Vegetable, make_job(71, 0.0, 3.0), make_job(72, 0.0, 3.0)), "zero-time hatch batch should begin");
    auto result = state.tick(0.1, true, true);
    require(result.became_ready_for_disassembly.size() == 2, "both zero-time eggs should finish incubation");
    require(!state.consume_cake_and_enqueue(prf::CakeKind::Normal, make_job(73), std::nullopt), "hatched but unprocessed offspring must keep the two-result pipeline full");
    result = tick_with_result(state, 3.0);
    require(result.reward_commit_requested == 71, "first zero-time hatch should still require a disassembly interval");
    require(state.confirm_reward_committed(71), "first result should commit once");
    require(!state.consume_cake_and_enqueue(prf::CakeKind::Normal, make_job(73), std::nullopt), "a free position must not start the next cake before the entire batch finishes");
    prf::FacilityState restored(prf::breeding_factory_config());
    require(restored.restore(state.snapshot()), "partially completed batch should restore");
    require(!restored.consume_cake_and_enqueue(prf::CakeKind::Normal, make_job(73), std::nullopt), "reload must preserve the whole-batch cake boundary");
    result = tick_with_result(restored, 3.0);
    require(result.reward_commit_requested == 72, "second result needs its own interval");
    require(restored.confirm_reward_committed(72), "second result should commit");
    require(restored.consume_cake_and_enqueue(prf::CakeKind::Normal, make_job(73), std::nullopt), "next cake is allowed only after both results commit");
    require(!restored.consume_cake_and_enqueue(prf::CakeKind::Normal, make_job(74), std::nullopt), "normal cake also occupies a complete batch until finished");
}

void restore_rejects_breeder_pipeline_over_capacity() {
    prf::FacilityState state(prf::breeding_factory_config());
    prf::FacilitySnapshot snapshot;
    snapshot.jobs = {
        make_job(81, 0.0, 3.0),
        make_job(82, 0.0, 3.0),
        make_job(83, 0.0, 3.0),
    };
    for (auto& job : snapshot.jobs) {
        job.stage = prf::JobStage::AwaitingDisassembly;
    }
    require(!state.restore(snapshot), "restore must enforce the breeder's whole-pipeline capacity");
}

void rewards_commit_once_and_wait_for_base_storage() {
    prf::FacilityState state(prf::egg_processor_config());
    require(state.deposit_egg(make_job(10, 1.0, 1.0)), "egg should be accepted");
    auto result = state.tick(1.0, true, false);
    require(result.became_ready_for_disassembly.size() == 1, "egg should become ready");
    require(!result.reward_commit_requested.has_value(), "newly hatched pal must not be disassembled in the same time slice");
    result = tick_with_result(state, 1.0, false);
    require(!result.reward_commit_requested.has_value(), "unavailable base storage must defer reward commit");
    result = state.tick(1.0, true, true);
    require(result.reward_commit_requested == 10, "available base storage should request reward commit");
    require(state.confirm_reward_committed(10), "first reward confirmation should succeed");
    require(!state.confirm_reward_committed(10), "duplicate reward confirmation must fail");
    require(state.committed_reward_count() == 1, "one reward should be committed");
}

void disassembly_is_strictly_serial() {
    prf::FacilityState state(prf::breeding_factory_config());
    require(state.consume_cake_and_enqueue(prf::CakeKind::Vegetable, make_job(31, 1.0, 2.0), make_job(32, 1.0, 2.0)), "two eggs should be accepted");
    auto result = state.tick(1.0, true, true);
    require(result.became_ready_for_disassembly.size() == 2, "both eggs incubate concurrently");
    result = tick_with_result(state, 2.0);
    require(result.reward_commit_requested == 31, "only the first pal enters the grinder");
    require(state.confirm_reward_committed(31), "first reward should commit");
    result = state.tick(1.0, true, true);
    require(!result.reward_commit_requested.has_value(), "second pal still needs its own disassembly interval");
    result = tick_with_result(state, 1.0);
    require(result.reward_commit_requested == 32, "second pal completes after a separate interval");
}

void destruction_erases_all_contents() {
    prf::FacilityState state(prf::breeding_factory_config());
    require(state.consume_cake_and_enqueue(prf::CakeKind::Vegetable, make_job(21), make_job(22)), "two jobs should exist before destruction");
    state.destroy();
    require(state.destroyed(), "facility should be marked destroyed");
    require(state.job_count() == 0, "destruction must erase eggs and pending pals");
    require(state.committed_reward_count() == 0, "destruction must erase unclaimed output state");
    require(!state.consume_cake_and_enqueue(prf::CakeKind::Normal, make_job(23), std::nullopt), "destroyed facility cannot restart");
}

void save_restore_preserves_exactly_once_rewards() {
    prf::FacilityState original(prf::egg_processor_config());
    require(original.deposit_egg(make_job(41, 1.0, 1.0)), "egg should be accepted before save");
    original.tick(1.0, true, true);
    auto result = tick_with_result(original, 1.0);
    require(result.reward_commit_requested == 41, "reward commit should be requested before save");
    require(original.confirm_reward_committed(41), "reward should commit before save");

    prf::FacilityState restored(prf::egg_processor_config());
    require(restored.restore(original.snapshot()), "valid save should restore");
    require(restored.committed_reward_count() == 1, "committed reward IDs must survive reload");
    require(!restored.deposit_egg(make_job(41)), "a reloaded committed ID cannot be reused");
    require(!restored.tick(30.0, true, true).reward_commit_requested.has_value(), "reload must not duplicate a reward");
}

void invalid_save_is_rejected_without_mutation() {
    prf::FacilityState state(prf::breeding_factory_config());
    const prf::FacilitySnapshot invalid{
        .jobs = {make_job(51), make_job(51)},
        .committed_reward_ids = {},
        .stop_after_batch_requested = false,
        .destroyed = false,
    };
    require(!state.restore(invalid), "duplicate job IDs must invalidate a save");
    require(state.job_count() == 0, "failed restore must not partially mutate state");
}

void stop_after_batch_waits_for_both_vegetable_eggs() {
    prf::FacilityState state(prf::breeding_factory_config());
    require(state.consume_cake_and_enqueue(prf::CakeKind::Vegetable, make_job(61, 1.0, 1.0), make_job(62, 1.0, 1.0)), "vegetable batch should begin");
    require(state.request_stop_after_batch(), "breeding facility should accept stop-after-batch");
    require(!state.consume_cake_and_enqueue(prf::CakeKind::Normal, make_job(63), std::nullopt), "stop request must block the next cake");
    require(!state.parents_can_release(), "parents remain locked while either egg is pending");

    state.tick(1.0, true, true);
    auto result = tick_with_result(state, 1.0);
    require(state.confirm_reward_committed(*result.reward_commit_requested), "first vegetable egg reward should commit");
    require(!state.parents_can_release(), "first egg alone must not release parents");
    result = tick_with_result(state, 1.0);
    require(state.confirm_reward_committed(*result.reward_commit_requested), "second vegetable egg reward should commit");
    require(state.parents_can_release(), "parents release only after the whole two-egg batch");
    require(state.acknowledge_parent_release(), "release acknowledgement should reset the stop latch");
    require(!state.stop_after_batch_requested(), "stop latch should be clear for a new parent assignment");
}

void native_drops_are_immutable_and_survive_storage_wait_and_restore() {
    prf::FacilityState state(prf::egg_processor_config());
    require(state.deposit_egg(make_job(91, 0, 1)), "first egg accepted");
    require(state.deposit_egg(make_job(92, 100, 1)), "second egg accepted");
    state.tick(0.1, true, true);
    auto result = state.tick(1, true, false);
    require(result.drop_resolution_requested == 91, "completed disassembly requests native calculation independently of storage capacity");
    require(!result.reward_commit_requested, "no award before the native result exists");
    require(!state.confirm_reward_committed(91), "timer completion alone is not a reward");
    require(!state.record_native_drops(91, {{"fixture-material", 1}}), "cannot record without claiming calculation");
    require(state.begin_drop_resolution(91), "claim native call");
    require(!state.begin_drop_resolution(91), "duplicate native call must be blocked");
    require(!state.tick(20, true, true).drop_resolution_requested, "in-flight native call must not be retried");
    const std::vector<prf::MaterialDrop> actual{{"fixture-material-a", 4}, {"fixture-material-b", 3}};
    require(state.record_native_drops(91, actual), "record actual result");
    require(!state.record_native_drops(91, {{"different-result", 999}}), "cannot replace results with a reroll");
    const auto before = state.snapshot();
    for (int i = 0; i < 20; ++i) {
        result = state.tick(100, true, false);
        require(!result.drop_resolution_requested && !result.reward_commit_requested, "full storage must not reroll or commit");
    }
    require(state.snapshot().jobs[1].remaining_incubation_seconds == before.jobs[1].remaining_incubation_seconds,
        "storage backpressure freezes the next egg instead of accumulating completed results");
    prf::FacilityState restored(prf::egg_processor_config());
    require(restored.restore(state.snapshot()), "restore waiting result");
    require(restored.snapshot().jobs[0].native_drops == actual, "restore must preserve exact item IDs and counts");
    require(!restored.begin_drop_resolution(91), "restored result cannot invoke RNG again");
    result = restored.tick(1, true, true);
    require(result.reward_commit_requested == 91 && !result.drop_resolution_requested, "storage recovery retries the same result only");
    require(restored.confirm_reward_committed(91), "confirm successful external transaction");
    require(!restored.confirm_reward_committed(91), "transaction cannot be acknowledged twice");
}

void incomplete_native_call_is_not_rerolled_after_restore() {
    prf::FacilityState state(prf::egg_processor_config());
    state.deposit_egg(make_job(101, 0, 1));
    state.tick(1, true, true);
    state.tick(1, true, true);
    require(state.begin_drop_resolution(101), "native call starts");
    prf::FacilityState restored(prf::egg_processor_config());
    require(restored.restore(state.snapshot()), "in-flight state should restore without replay");
    const auto result = restored.tick(100, true, true);
    require(!result.drop_resolution_requested && !result.reward_commit_requested, "unknown native-call outcome must freeze, not reroll or award");
}

void zero_drops_are_a_valid_result_and_destruction_removes_pending_drops() {
    prf::FacilityState state(prf::egg_processor_config());
    state.deposit_egg(make_job(111, 0, 1));
    state.tick(1, true, true);
    state.tick(1, true, true);
    require(state.begin_drop_resolution(111), "claim zero-drop result");
    require(!state.record_native_drops(111, {{"invalid-count", -1}}), "negative count is not a valid native result");
    require(state.record_native_drops(111, {}), "zero drops differs from an unresolved result");
    auto snapshot = state.snapshot();
    require(snapshot.jobs[0].native_drops.has_value() && snapshot.jobs[0].native_drops->empty(), "empty result remains resolved");
    prf::FacilityState restored(prf::egg_processor_config());
    require(restored.restore(snapshot), "valid zero-drop result restores");
    require(restored.tick(1, true, true).reward_commit_requested == 111, "zero-drop result is eligible for completion");
    require(restored.confirm_reward_committed(111), "zero drops must not block the machine forever");
    snapshot.jobs[0].native_drops.reset();
    require(!restored.restore(snapshot), "pending reward cannot restore without its immutable native result");
    require(restored.committed_reward_count() == 1, "failed restore leaves the previous state intact");
    state.destroy();
    require(state.snapshot().jobs.empty(), "destruction erases pending materials, no recovery inventory");
    require(!state.confirm_reward_committed(111), "destroyed result cannot be awarded");
}

void processor_accepts_54_inputs_only() {
    prf::FacilityState state(prf::egg_processor_config());
    for (std::uint64_t i = 1; i <= 54; ++i) require(state.deposit_egg(make_job(i)), "all 54 slots accept eggs");
    require(!state.deposit_egg(make_job(55)), "55th egg refused");
    require(!state.consume_cake_and_enqueue(prf::CakeKind::Normal, make_job(56), std::nullopt), "processor never accepts cake");
    prf::FacilityState breeder(prf::breeding_factory_config());
    require(!breeder.deposit_egg(make_job(57)), "breeder never accepts external eggs");
}

} // namespace

int main() {
    egg_processor_locks_and_freezes();
    vegetable_cake_requires_two_slots();
    zero_hatch_time_does_not_create_an_unbounded_cake_queue();
    restore_rejects_breeder_pipeline_over_capacity();
    rewards_commit_once_and_wait_for_base_storage();
    disassembly_is_strictly_serial();
    destruction_erases_all_contents();
    save_restore_preserves_exactly_once_rewards();
    invalid_save_is_rejected_without_mutation();
    stop_after_batch_waits_for_both_vegetable_eggs();
    native_drops_are_immutable_and_survive_storage_wait_and_restore();
    incomplete_native_call_is_not_rerolled_after_restore();
    zero_drops_are_a_valid_result_and_destruction_removes_pending_drops();
    processor_accepts_54_inputs_only();
    std::cout << "facility_state_test: PASS\n";
    return 0;
}
