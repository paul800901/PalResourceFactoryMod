#include "../native/src/BreederState.hpp"
#include "../native/src/AncientConveyor.hpp"
#include <iostream>
#include <source_location>
using namespace prf::breeder;
void check(bool ok, std::source_location at = std::source_location::current()) {
    if (!ok) { std::cerr << "Breeder test failed: " << at.line() << '\n'; std::exit(1); }
}
template<class F> void refused(F f) {
    bool caught{}; try { f(); } catch (const std::exception&) { caught = true; } check(caught);
}
int main() {
    // Native incubation admits completed eggs independently of cake/breeding.
    // A full native 10-slot batch, or a final 5-egg batch with no new inputs,
    // must drain through the existing conveyor without needing another egg.
    for (int egg_count : {5, 10}) {
        prf::processor::State batch;
        for (int i = 0; i < egg_count; ++i) batch.inserted(i, "batch-" + std::to_string(i), 0);
        int processed{};
        for (int tick = 0; tick < 400 && processed < egg_count; ++tick) {
            if (const auto due = batch.advance(0.1, 5, 3)) {
                batch.empty(*due);
                ++processed;
            }
        }
        check(processed == egg_count && batch.display_counts() == std::array<int, 2>{0, 0});
    }
    prf::processor::State conveyor;
    conveyor.inserted(0, "old", 0);
    conveyor.inserted(1, "moved", 0);
    conveyor.advance(100, 1);
    std::array<std::string, 54> native{};
    native[0] = "moved";
    native[1] = "new";
    ancient_breeder::reconcile_conveyor(conveyor, native);
    conveyor.inserted(0, "moved", 0);
    conveyor.inserted(1, "new", 0);
    conveyor.advance(0.1, 1);
    const auto progress = conveyor.jobs[0].disassembly;
    ancient_breeder::reconcile_conveyor(conveyor, native);
    check(conveyor.jobs[0].disassembly == progress);
    native[0].clear(); // consumed while receipt persistence was delayed
    ancient_breeder::reconcile_conveyor(conveyor, native);
    check(conveyor.jobs[0].egg.empty());
    native[0] = "refilled";
    ancient_breeder::reconcile_conveyor(conveyor, native);
    conveyor.inserted(0, "refilled", 0);
    check(conveyor.jobs[0].disassembly == -1);
    State s;
    refused([&] { s.start("m", "m", Cake::Normal, 1, 10, true); });
    refused([&] { s.start("m", "f", Cake::Normal, 1, 10, false); });
    refused([&] { s.start("m", "f", Cake::Unsupported, 1, 10, true); });
    refused([&] { s.start("m", "f", Cake::Vegetable, 0, 10, true); });
    refused([&] { s.start("m", "f", Cake::Vegetable, 3, 10, true); });
    s.start("m", "f", Cake::Vegetable, 2, 10, true);
    check(!s.advance_breeding(100, false, true, "m", "f"));
    check(!s.advance_breeding(100, true, true, "different", "f"));
    check(!s.advance_breeding(100, true, false, "m", "f"));
    check(s.breeding_remaining == 10);
    check(!s.advance_breeding(4, true, true, "m", "f"));
    s = State::parse(s.serialize());
    check(s.advance_breeding(6, true, true, "m", "f"));
    s = State::parse(s.serialize());
    check(!s.advance_breeding(100, true, true, "m", "f")); // no RNG replay
    refused([&] { s.accept_native_eggs({{"one", 0}}); });
    refused([&] { s.accept_native_eggs({{"same", 0}, {"same", 0}}); });
    s.accept_native_eggs({{"one", 2}, {"two", 3}});
    s.stop_after_batch = true;
    check(!s.parents_can_release());
    check(!s.advance_processing(100, 5, 3, false));
    check(s.offspring.jobs[0].remaining == 2);
    int completed{};
    for (int tick = 0; tick < 150 && completed < 2; ++tick) {
        auto due = s.advance_processing(0.1, 5, 3, true);
        if (due) {
            const auto id = s.offspring.jobs[*due].egg;
            s.settled(*due, id);
            refused([&] { s.settled(*due, id); });
            ++completed;
            if (completed == 1) {
                check(!s.parents_can_release() && !s.can_start());
                refused([&] { s.start("m", "f", Cake::Normal, 1, 10, true); });
            }
        }
        s = State::parse(s.serialize());
    }
    check(completed == 2 && s.parents_can_release() && !s.can_start());
    s.stop_after_batch = false;
    s.start("m2", "f2", Cake::Normal, 1, 1, true);
    check(s.advance_breeding(1, true, true, "m2", "f2"));
    refused([&] { s.accept_native_eggs({{"three", 0}, {"four", 0}}); });
    s.accept_native_eggs({{"three", 0}});
    check(!s.advance_processing(0.1, 5, 3, true));
    check(s.advance_processing(5, 5, 3, true) == 0);
    s.settled(0, "three");
    check(s.can_start());
    State native_count;
    native_count.start("m", "f", Cake::Vegetable, 1, 10, true);
    check(native_count.offspring_count == 1); // Native data wins over the cake label.
    refused([&] { State::parse(s.serialize().substr(0, 20)); });
    std::cout << "Breeder: pair continuity, cake batch sizes, pause, claimed RNG, parallel offspring, restart and stop: PASS (core only)\n";
}
