#pragma once
#include "SettlementTypes.hpp"
#include <algorithm>
#include <optional>
#include <set>

namespace prf::processor {
// Only persisted value state; no UObject/actor pointers survive a frame.
struct Job {
    std::string egg;
    double remaining{};
    double disassembly{-1}; // normalized progress; -1 means not admitted
};
struct State {
    std::array<Job, 54> jobs{};
    bool processing() const {
        return std::any_of(jobs.begin(), jobs.end(), [](const auto& j) { return j.disassembly >= 0; });
    }
    std::array<int, 2> display_counts() const {
        std::array<int, 2> counts{};
        for (const auto& job : jobs)
            if (!job.egg.empty()) ++counts[job.remaining > 0 ? 0 : 1];
        return counts;
    }

    void inserted(size_t slot, const std::string& egg, double seconds) {
        settlement::verify_value(slot < jobs.size() && !egg.empty() && std::isfinite(seconds) && seconds >= 0,
            "Invalid processor input");
        if (jobs[slot].egg == egg) return;
        settlement::verify_value(jobs[slot].egg.empty(), "Locked processor input was replaced");
        for (const auto& job : jobs) settlement::verify_value(job.egg != egg, "Duplicate input egg identity");
        jobs[slot] = {egg, seconds};
    }
    void empty(size_t slot) {
        jobs.at(slot) = {};
    }
    std::optional<size_t> advance(double seconds, double native_interval, int capacity = 1) {
        settlement::verify_value(std::isfinite(seconds) && seconds >= 0 &&
            std::isfinite(native_interval) && native_interval > 0 && capacity > 0, "Invalid processor clock");
        // Complete parallel incubation first, but do not spend this frame twice
        // by also immediately advancing a newly selected disassembly job.
        for (auto& job : jobs) if (!job.egg.empty()) job.remaining = std::max(0.0, job.remaining - seconds);
        int active{};
        double youngest = 1;
        std::optional<size_t> due;
        for (size_t i = 0; i < jobs.size(); ++i) {
            auto& job = jobs[i];
            if (job.disassembly < 0) continue;
            job.disassembly = std::min(1.0, job.disassembly + seconds / native_interval);
            youngest = std::min(youngest, job.disassembly);
            ++active;
            if (job.disassembly >= 1 && !due) due = i;
        }
        // Native conveyor staggers admission by 1/capacity of a full cycle.
        // Completed eggs retain their slot until the existing settlement commits.
        if (active < capacity && (active == 0 || youngest >= 1.0 / capacity)) {
            for (auto& job : jobs) if (!job.egg.empty() && job.remaining == 0 && job.disassembly < 0) {
                job.disassembly = 0;
                break;
            }
        }
        return due;
    }
    std::string serialize() const {
        std::ostringstream out;
        out << "PRF_PROCESSOR_2\n" << std::setprecision(17);
        for (const auto& job : jobs) out << std::quoted(job.egg) << ' ' << job.remaining << ' ' << job.disassembly << '\n';
        return out.str();
    }
    static State parse(const std::string& text, double legacy_interval = 5.0) {
        settlement::verify_value(!text.empty() && text.back() == '\n', "Incomplete processor checkpoint");
        State result;
        std::istringstream in{text};
        std::string tag;
        settlement::verify_value(bool(in >> tag) && (tag == "PRF_PROCESSOR_1" || tag == "PRF_PROCESSOR_2"), "Invalid processor checkpoint");
        int legacy_slot = -1;
        double legacy_remaining{};
        if (tag == "PRF_PROCESSOR_1") {
            settlement::verify_value(bool(in >> legacy_slot >> legacy_remaining) && legacy_slot >= -1 && legacy_slot < 54 &&
                std::isfinite(legacy_remaining) && legacy_remaining >= 0 && std::isfinite(legacy_interval) && legacy_interval > 0,
                "Invalid legacy processor checkpoint");
        }
        std::set<std::string> seen;
        for (auto& job : result.jobs) {
            settlement::verify_value(bool(in >> std::quoted(job.egg) >> job.remaining) && std::isfinite(job.remaining) &&
                job.remaining >= 0 && (job.egg.empty() ? job.remaining == 0 : seen.insert(job.egg).second),
                "Invalid processor job checkpoint");
            if (tag == "PRF_PROCESSOR_2") settlement::verify_value(bool(in >> job.disassembly) &&
                std::isfinite(job.disassembly) && (job.disassembly == -1 || (job.disassembly >= 0 && job.disassembly <= 1 &&
                !job.egg.empty() && job.remaining == 0)), "Invalid pipeline checkpoint");
        }
        if (legacy_slot >= 0) {
            auto& job = result.jobs[legacy_slot];
            settlement::verify_value(!job.egg.empty() && job.remaining == 0, "Missing processing egg");
            job.disassembly = std::clamp(1.0 - legacy_remaining / legacy_interval, 0.0, 1.0);
        }
        in >> std::ws;
        settlement::verify_value(in.eof(), "Unexpected processor checkpoint content");
        return result;
    }
};
} // namespace prf::processor
