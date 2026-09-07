#pragma once
#include <chrono>
#include <future>
#include <optional>
#include <stdexcept>
#include <utility>

// One outstanding disk operation per owner. Poll never waits; destruction
// joins so the DLL and captured receipt cannot disappear beneath a writer.
class AsyncDisk {
    std::future<bool> pending;
public:
    bool busy() const { return pending.valid(); }
    template<class F> void start(F&& operation) {
        if (busy()) throw std::logic_error("Disk operation already pending");
        pending = std::async(std::launch::async, std::forward<F>(operation));
    }
    std::optional<bool> poll() {
        if (!busy() || pending.wait_for(std::chrono::seconds{0}) != std::future_status::ready) return std::nullopt;
        return pending.get(); // Propagate I/O errors before any dependent action.
    }
    void drain() { if (busy()) static_cast<void>(pending.get()); }
};
