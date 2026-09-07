#include "../native/src/ProcessorState.hpp"
#include <iostream>
#include <source_location>

using prf::processor::State;
void check(bool value, std::source_location where = std::source_location::current()) {
    if (!value) { std::cerr << "processor state test failed at line " << where.line() << '\n'; std::exit(1); }
}
template<class F> void refused(F f) {
    bool caught = false;
    try { f(); } catch (const std::exception&) { caught = true; }
    check(caught);
}
int main() {
    State s;
    for (size_t i = 0; i < 54; ++i) s.inserted(i, "egg-" + std::to_string(i), 3.0);
    refused([&]{ s.inserted(0, "replacement", 0); });
    refused([&]{ s.inserted(54, "overflow", 0); });
    check(!s.advance(2, 1));
    auto restored = State::parse(s.serialize());
    check(restored.jobs[53].remaining == 1 && restored.jobs[53].egg == "egg-53");
    check(!restored.advance(1, 1));
    check(restored.processing() && restored.jobs[0].disassembly == 0);
    restored = State::parse(restored.serialize());
    for (size_t i = 0; i < 54; ++i) {
        auto done = restored.advance(1, 1);
        check(done && *done == i);
        restored.empty(*done);
        check(!restored.advance(0, 1));
    }
    check(!restored.processing() && !restored.advance(100, 1));
    State counts;
    counts.inserted(0, "hatching", 10);
    counts.inserted(1, "queued", 0);
    check(counts.display_counts() == (std::array<int, 2>{1, 1}));
    counts.advance(1, 2);
    check(counts.display_counts() == (std::array<int, 2>{1, 1}));
    counts.empty(1);
    check(counts.display_counts() == (std::array<int, 2>{1, 0}));
    counts.empty(0);
    check(counts.display_counts() == (std::array<int, 2>{0, 0}));
    State immediate;
    immediate.inserted(0, "zero-time-world", 0);
    check(!immediate.advance(1, 0.5));
    check(!immediate.advance(0, 0.5)); // pause does not progress
    check(immediate.advance(0.5, 0.5) == 0);
    refused([&]{ State::parse(s.serialize().substr(0, 20)); });
    refused([&]{ State::parse(s.serialize() + "extra\n"); });
    refused([&]{ s.advance(-1, 1); });
    State pipeline;
    for (size_t i = 0; i < 54; ++i) pipeline.inserted(i, "pipeline-" + std::to_string(i), 0);
    pipeline.advance(0, 5, 3);
    double time = 0, last = 0;
    size_t completed = 0;
    while (completed < 54) {
        time += 0.1;
        auto due = pipeline.advance(0.1, 5, 3);
        if (due) {
            check(*due == completed);
            if (completed == 0) check(time >= 5 - 1e-9 && time < 5.2);
            else check(time - last >= 1.6 - 1e-9 && time - last < 1.9);
            last = time;
            // A due egg remains the same until commit, never a new roll.
            check(pipeline.advance(0, 5, 3) == due);
            pipeline.empty(*due);
            ++completed;
        }
        pipeline = State::parse(pipeline.serialize());
        check(time < 110);
    }
    check(!pipeline.processing());
    std::cout << "54 eggs: first completion 5s, steady spacing 1.7s, final " << last << "s\n";
    State disk_pending;
    for (size_t i = 0; i < 12; ++i) disk_pending.inserted(i, "disk-" + std::to_string(i), 0);
    disk_pending.advance(0, 5, 3);
    double previous = 0;
    size_t settled = 0;
    for (int tick = 1; tick < 400 && settled < 12; ++tick) {
        auto due = disk_pending.advance(0.1, 5, 3);
        if (!due) continue;
        check(*due == settled);
        if (settled) check(tick * 0.1 - previous < 2.0);
        previous = tick * 0.1;
        // Simulate 200ms receipt latency; other jobs continue advancing.
        check(disk_pending.advance(0.1, 5, 3) == due);
        check(disk_pending.advance(0.1, 5, 3) == due);
        tick += 2;
        disk_pending.empty(*due);
        ++settled;
    }
    check(settled == 12);
    State blocked;
    for (size_t i = 0; i < 5; ++i) blocked.inserted(i, "blocked-" + std::to_string(i), 0);
    for (int i = 0; i < 200; ++i) blocked.advance(0.1, 5, 3);
    check(blocked.jobs[2].disassembly == 1 && blocked.jobs[3].disassembly == -1);
    std::ostringstream legacy;
    legacy << "PRF_PROCESSOR_1\n0 2.5\n";
    for (int i = 0; i < 54; ++i) legacy << std::quoted(i == 0 ? "legacy-egg" : "") << " 0\n";
    auto migrated = State::parse(legacy.str(), 5);
    check(migrated.jobs[0].disassembly == 0.5);
    check(migrated.advance(2.5, 5, 3) == 0);
    refused([&]{ pipeline.advance(0.1, 5, 0); });
    std::cout << "54 slots, staggered pipeline, once-only commit, checkpoint migration and pause: PASS\n";
}
