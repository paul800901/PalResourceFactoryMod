#include "../native/src/AsyncDisk.hpp"
#include <iostream>
#include <thread>

void check(bool value) { if (!value) throw std::runtime_error("async disk test failed"); }
int main() {
    AsyncDisk disk;
    std::promise<void> release;
    auto gate = release.get_future();
    disk.start([&] { gate.get(); return false; });
    // Deliberately blocked disk: game-thread polling must still return.
    for (int i = 0; i < 10000; ++i) check(!disk.poll().has_value());
    bool refused{};
    try { disk.start([] { return true; }); } catch (const std::logic_error&) { refused = true; }
    check(refused);
    release.set_value();
    std::optional<bool> result;
    while (!(result = disk.poll()).has_value()) std::this_thread::yield();
    check(!*result && !disk.busy()); // false is a completed new RNG claim, not pending.
    disk.start([]() -> bool { throw std::runtime_error("write failed"); });
    bool failed{};
    try { disk.drain(); } catch (const std::runtime_error&) { failed = true; }
    check(failed && !disk.busy());
    int durable{};
    disk.start([&] { durable = 1; return true; });
    while (!disk.poll().has_value()) std::this_thread::yield();
    check(durable == 1); // Only now may the dependent game mutation run.
    disk.start([&] { durable = 2; return true; });
    disk.drain();
    check(durable == 2);
    std::cout << "async disk barriers passed\n";
}
