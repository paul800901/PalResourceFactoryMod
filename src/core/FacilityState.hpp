#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace prf {

enum class FacilityKind {
    EggProcessor,
    BreedingFactory,
};

enum class CakeKind {
    Normal,
    Vegetable,
    Unsupported,
};

enum class JobStage {
    Incubating,
    AwaitingDisassembly,
    Disassembling,
    AwaitingRewardCommit,
    RewardCommitted,
    AwaitingDropResolution,
    ResolvingDrops,
};

struct FacilityConfig {
    FacilityKind kind{};
    std::size_t input_capacity{};
    std::size_t incubation_capacity{};
    std::int32_t active_power_per_second{};
    std::int32_t incubation_power_per_second{};
    double incubation_speed_multiplier{};
};

// Returned by the game's butcher calculation, not a copied drop table.
struct MaterialDrop {
    std::string item_id;
    std::int32_t count{};
    bool operator==(const MaterialDrop&) const = default;
};

struct EggJob {
    std::uint64_t id{};
    JobStage stage{JobStage::Incubating};
    double remaining_incubation_seconds{};
    double remaining_disassembly_seconds{};
    std::string opaque_game_payload;
    // nullopt = not calculated; an empty vector = a valid zero-drop result.
    std::optional<std::vector<MaterialDrop>> native_drops;
};

struct TickResult {
    std::vector<std::uint64_t> became_ready_for_disassembly;
    std::optional<std::uint64_t> reward_commit_requested;
    std::optional<std::uint64_t> drop_resolution_requested;
};

struct FacilitySnapshot {
    std::vector<EggJob> jobs;
    std::vector<std::uint64_t> committed_reward_ids;
    bool stop_after_batch_requested{false};
    bool destroyed{false};
};

class FacilityState {
public:
    explicit FacilityState(FacilityConfig config);

    [[nodiscard]] const FacilityConfig& config() const noexcept;
    [[nodiscard]] bool destroyed() const noexcept;
    [[nodiscard]] std::size_t job_count() const noexcept;
    [[nodiscard]] std::size_t committed_reward_count() const noexcept;
    [[nodiscard]] std::int32_t current_power_per_second() const noexcept;
    [[nodiscard]] FacilitySnapshot snapshot() const;
    [[nodiscard]] bool stop_after_batch_requested() const noexcept;
    [[nodiscard]] bool parents_can_release() const noexcept;

    bool deposit_egg(EggJob job);
    bool consume_cake_and_enqueue(CakeKind cake, EggJob first, std::optional<EggJob> second);
    TickResult tick(double elapsed_seconds, bool has_power, bool destination_accepts_reward);
    bool begin_drop_resolution(std::uint64_t job_id);
    bool record_native_drops(std::uint64_t job_id, std::vector<MaterialDrop> drops);
    bool confirm_reward_committed(std::uint64_t job_id);
    bool request_stop_after_batch() noexcept;
    bool acknowledge_parent_release() noexcept;
    bool restore(const FacilitySnapshot& snapshot);
    void destroy();

private:
    [[nodiscard]] std::size_t incubating_count() const noexcept;
    [[nodiscard]] bool can_accept_job_count(std::size_t count) const noexcept;

    FacilityConfig config_;
    std::deque<EggJob> jobs_;
    std::unordered_set<std::uint64_t> known_ids_;
    std::unordered_set<std::uint64_t> committed_reward_ids_;
    bool stop_after_batch_requested_{false};
    bool destroyed_{false};
};

FacilityConfig egg_processor_config();
FacilityConfig breeding_factory_config();

} // namespace prf
