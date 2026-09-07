#include "FacilityState.hpp"

#include <algorithm>
#include <utility>

namespace prf {

FacilityState::FacilityState(FacilityConfig config) : config_(config) {}

const FacilityConfig& FacilityState::config() const noexcept {
    return config_;
}

bool FacilityState::destroyed() const noexcept {
    return destroyed_;
}

std::size_t FacilityState::job_count() const noexcept {
    return jobs_.size();
}

std::size_t FacilityState::committed_reward_count() const noexcept {
    return committed_reward_ids_.size();
}

std::int32_t FacilityState::current_power_per_second() const noexcept {
    return incubating_count() > 0
        ? config_.incubation_power_per_second
        : config_.active_power_per_second;
}

FacilitySnapshot FacilityState::snapshot() const {
    return FacilitySnapshot{
        .jobs = {jobs_.begin(), jobs_.end()},
        .committed_reward_ids = {committed_reward_ids_.begin(), committed_reward_ids_.end()},
        .stop_after_batch_requested = stop_after_batch_requested_,
        .destroyed = destroyed_,
    };
}

bool FacilityState::stop_after_batch_requested() const noexcept {
    return stop_after_batch_requested_;
}

bool FacilityState::parents_can_release() const noexcept {
    return config_.kind == FacilityKind::BreedingFactory
        && stop_after_batch_requested_
        && jobs_.empty();
}

std::size_t FacilityState::incubating_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(jobs_.begin(), jobs_.end(), [](const EggJob& job) {
        return job.stage == JobStage::Incubating;
    }));
}

bool FacilityState::can_accept_job_count(const std::size_t count) const noexcept {
    if (destroyed_ || count == 0) {
        return false;
    }
    if (config_.kind == FacilityKind::EggProcessor) {
        return jobs_.size() + count <= config_.input_capacity;
    }
    // The breeding facility has one bounded two-result pipeline, not merely
    // two incubator slots. A zero-second world hatch setting must not allow
    // completed offspring to accumulate while the machine keeps eating cake.
    return jobs_.empty() && count <= config_.incubation_capacity;
}

bool FacilityState::deposit_egg(EggJob job) {
    if (config_.kind != FacilityKind::EggProcessor || !can_accept_job_count(1) || known_ids_.contains(job.id)) {
        return false;
    }
    job.stage = JobStage::Incubating;
    job.native_drops.reset();
    known_ids_.insert(job.id);
    jobs_.push_back(std::move(job));
    return true;
}

bool FacilityState::consume_cake_and_enqueue(
    const CakeKind cake,
    EggJob first,
    std::optional<EggJob> second) {
    if (config_.kind != FacilityKind::BreedingFactory
        || cake == CakeKind::Unsupported
        || stop_after_batch_requested_) {
        return false;
    }

    const std::size_t required = cake == CakeKind::Vegetable ? 2U : 1U;
    if (!can_accept_job_count(required) || known_ids_.contains(first.id)) {
        return false;
    }
    if (cake == CakeKind::Vegetable) {
        if (!second.has_value() || second->id == first.id || known_ids_.contains(second->id)) {
            return false;
        }
    } else if (second.has_value()) {
        return false;
    }

    first.stage = JobStage::Incubating;
    first.native_drops.reset();
    known_ids_.insert(first.id);
    jobs_.push_back(std::move(first));
    if (second.has_value()) {
        second->stage = JobStage::Incubating;
        second->native_drops.reset();
        known_ids_.insert(second->id);
        jobs_.push_back(std::move(*second));
    }
    return true;
}

TickResult FacilityState::tick(
    const double elapsed_seconds,
    const bool has_power,
    const bool destination_accepts_reward) {
    TickResult result;
    if (destroyed_ || elapsed_seconds <= 0.0 || !has_power) {
        return result;
    }

    // Do not start the next disassembly while the previous result is unresolved
    // or waiting for a complete storage transaction. Incubation freezes too.
    const auto pending = std::find_if(jobs_.begin(), jobs_.end(), [](const EggJob& job) {
        return job.stage == JobStage::AwaitingDropResolution
            || job.stage == JobStage::ResolvingDrops
            || job.stage == JobStage::AwaitingRewardCommit;
    });
    if (pending != jobs_.end()) {
        if (pending->stage == JobStage::AwaitingDropResolution) {
            result.drop_resolution_requested = pending->id;
        } else if (pending->stage == JobStage::AwaitingRewardCommit && destination_accepts_reward) {
            result.reward_commit_requested = pending->id;
        }
        return result;
    }

    const bool had_disassembly_candidate = std::any_of(jobs_.begin(), jobs_.end(), [](const EggJob& job) {
        return job.stage == JobStage::Disassembling || job.stage == JobStage::AwaitingDisassembly;
    });

    for (auto& job : jobs_) {
        if (job.stage != JobStage::Incubating) {
            continue;
        }
        job.remaining_incubation_seconds = std::max(0.0, job.remaining_incubation_seconds - elapsed_seconds);
        if (job.remaining_incubation_seconds == 0.0) {
            job.stage = JobStage::AwaitingDisassembly;
            result.became_ready_for_disassembly.push_back(job.id);
        }
    }

    auto active = std::find_if(jobs_.begin(), jobs_.end(), [](const EggJob& job) {
        return job.stage == JobStage::Disassembling;
    });
    if (active == jobs_.end() && had_disassembly_candidate) {
        active = std::find_if(jobs_.begin(), jobs_.end(), [](const EggJob& job) {
            return job.stage == JobStage::AwaitingDisassembly;
        });
        if (active != jobs_.end()) {
            active->stage = JobStage::Disassembling;
        }
    }

    if (active != jobs_.end() && active->stage == JobStage::Disassembling) {
        active->remaining_disassembly_seconds = std::max(0.0, active->remaining_disassembly_seconds - elapsed_seconds);
        if (active->remaining_disassembly_seconds == 0.0) {
            active->stage = JobStage::AwaitingDropResolution;
            result.drop_resolution_requested = active->id;
        }
    }

    return result;
}

bool FacilityState::begin_drop_resolution(const std::uint64_t job_id) {
    if (destroyed_) return false;
    const auto job = std::find_if(jobs_.begin(), jobs_.end(), [job_id](const EggJob& value) {
        return value.id == job_id && value.stage == JobStage::AwaitingDropResolution;
    });
    if (job == jobs_.end()) return false;
    // Claim before invoking the game's RNG. A failed/incomplete call must not
    // silently reroll on the next tick or after restoring a snapshot.
    job->stage = JobStage::ResolvingDrops;
    return true;
}

bool FacilityState::record_native_drops(const std::uint64_t job_id, std::vector<MaterialDrop> drops) {
    if (destroyed_) return false;
    const auto job = std::find_if(jobs_.begin(), jobs_.end(), [job_id](const EggJob& value) {
        return value.id == job_id && value.stage == JobStage::ResolvingDrops;
    });
    if (job == jobs_.end() || job->native_drops.has_value()) return false;
    if (std::any_of(drops.begin(), drops.end(), [](const MaterialDrop& drop) {
        return drop.item_id.empty() || drop.count <= 0;
    })) return false;
    job->native_drops = std::move(drops);
    job->stage = JobStage::AwaitingRewardCommit;
    return true;
}

bool FacilityState::confirm_reward_committed(const std::uint64_t job_id) {
    if (destroyed_ || committed_reward_ids_.contains(job_id)) {
        return false;
    }
    const auto job = std::find_if(jobs_.begin(), jobs_.end(), [job_id](const EggJob& value) {
        return value.id == job_id && value.stage == JobStage::AwaitingRewardCommit
            && value.native_drops.has_value();
    });
    if (job == jobs_.end()) {
        return false;
    }
    committed_reward_ids_.insert(job_id);
    jobs_.erase(job);
    return true;
}

bool FacilityState::request_stop_after_batch() noexcept {
    if (destroyed_ || config_.kind != FacilityKind::BreedingFactory) {
        return false;
    }
    stop_after_batch_requested_ = true;
    return true;
}

bool FacilityState::acknowledge_parent_release() noexcept {
    if (!parents_can_release()) {
        return false;
    }
    stop_after_batch_requested_ = false;
    return true;
}

bool FacilityState::restore(const FacilitySnapshot& snapshot_value) {
    if (snapshot_value.destroyed
        && (!snapshot_value.jobs.empty()
            || !snapshot_value.committed_reward_ids.empty()
            || snapshot_value.stop_after_batch_requested)) {
        return false;
    }

    std::unordered_set<std::uint64_t> ids;
    for (const auto committed_id : snapshot_value.committed_reward_ids) {
        if (!ids.insert(committed_id).second) {
            return false;
        }
    }
    for (const auto& job : snapshot_value.jobs) {
        if (job.stage == JobStage::RewardCommitted || !ids.insert(job.id).second) {
            return false;
        }
        if ((job.stage == JobStage::AwaitingRewardCommit) != job.native_drops.has_value()) {
            return false;
        }
        if (job.native_drops && std::any_of(job.native_drops->begin(), job.native_drops->end(), [](const MaterialDrop& drop) {
            return drop.item_id.empty() || drop.count <= 0;
        })) return false;
    }

    if (config_.kind == FacilityKind::EggProcessor) {
        if (snapshot_value.jobs.size() > config_.input_capacity) {
            return false;
        }
    } else if (snapshot_value.jobs.size() > config_.incubation_capacity) {
        return false;
    }

    jobs_ = {snapshot_value.jobs.begin(), snapshot_value.jobs.end()};
    known_ids_ = std::move(ids);
    committed_reward_ids_ = {
        snapshot_value.committed_reward_ids.begin(), snapshot_value.committed_reward_ids.end()
    };
    stop_after_batch_requested_ = snapshot_value.stop_after_batch_requested;
    destroyed_ = snapshot_value.destroyed;
    return true;
}

void FacilityState::destroy() {
    jobs_.clear();
    known_ids_.clear();
    committed_reward_ids_.clear();
    stop_after_batch_requested_ = false;
    destroyed_ = true;
}

FacilityConfig egg_processor_config() {
    return FacilityConfig{
        .kind = FacilityKind::EggProcessor,
        .input_capacity = 54,
        .incubation_capacity = 54,
        .active_power_per_second = 200,
        .incubation_power_per_second = 400,
        .incubation_speed_multiplier = 1.5,
    };
}

FacilityConfig breeding_factory_config() {
    return FacilityConfig{
        .kind = FacilityKind::BreedingFactory,
        .input_capacity = 0,
        .incubation_capacity = 2,
        .active_power_per_second = 500,
        .incubation_power_per_second = 1000,
        .incubation_speed_multiplier = 2.0,
    };
}

} // namespace prf
