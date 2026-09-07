#pragma once
#include "ProcessorState.hpp"

namespace prf::breeder {
// Local batch controller. The game adapter must supply authoritative parent
// eligibility, cake effects, duration and real generated egg identities.
// This type neither consumes inventory nor invents offspring or drops.
enum class Cake { Normal, Vegetable, Unsupported };
enum class Phase { Idle, Breeding, GenerationClaimed, Processing };

struct State {
    Phase phase{Phase::Idle};
    std::string male, female;
    int offspring_count{};
    double breeding_remaining{};
    bool stop_after_batch{};
    processor::State offspring;

    bool can_start() const { return phase == Phase::Idle && !stop_after_batch; }
    // Called only after an adapter has durably consumed one approved cake.
    void start(const std::string& male_id, const std::string& female_id,
               Cake cake, int native_count, double native_seconds, bool native_pair_eligible) {
        settlement::verify_value(can_start() && native_pair_eligible && !male_id.empty() &&
            !female_id.empty() && male_id != female_id && cake != Cake::Unsupported &&
            native_count > 0 && native_count <= 2 &&
            std::isfinite(native_seconds) && native_seconds > 0, "Invalid breeding batch start");
        male = male_id;
        female = female_id;
        // The adapter supplies the current native effect, not a copied cake table.
        offspring_count = native_count;
        breeding_remaining = native_seconds;
        phase = Phase::Breeding;
    }
    // Return true only once. Persist GenerationClaimed BEFORE invoking native RNG.
    // An interrupted claim must not automatically generate a different offspring.
    bool advance_breeding(double seconds, bool has_power, bool native_pair_eligible,
                          const std::string& current_male, const std::string& current_female) {
        settlement::verify_value(std::isfinite(seconds) && seconds >= 0, "Invalid breeding clock");
        if (phase != Phase::Breeding || !has_power || !native_pair_eligible ||
            male != current_male || female != current_female) return false;
        breeding_remaining = std::max(0.0, breeding_remaining - seconds);
        if (breeding_remaining > 0) return false;
        phase = Phase::GenerationClaimed;
        return true;
    }
    void accept_native_eggs(const std::vector<processor::Job>& eggs) {
        settlement::verify_value(phase == Phase::GenerationClaimed &&
            eggs.size() == static_cast<size_t>(offspring_count), "Native breeding result count differs");
        processor::State next;
        for (size_t i = 0; i < eggs.size(); ++i) next.inserted(i, eggs[i].egg, eggs[i].remaining);
        offspring = std::move(next);
        phase = Phase::Processing;
    }
    std::optional<size_t> advance_processing(double seconds, double native_duration, int native_capacity, bool has_power) {
        if (phase != Phase::Processing || !has_power) return std::nullopt;
        return offspring.advance(seconds, native_duration, native_capacity);
    }
    // Only after the existing settlement has consumed this exact egg.
    void settled(size_t slot, const std::string& egg_id) {
        settlement::verify_value(phase == Phase::Processing && slot < static_cast<size_t>(offspring_count) &&
            !egg_id.empty() && offspring.jobs[slot].egg == egg_id && offspring.jobs[slot].disassembly == 1,
            "Invalid breeding settlement acknowledgement");
        offspring.empty(slot);
        if (std::none_of(offspring.jobs.begin(), offspring.jobs.end(), [](const auto& j) { return !j.egg.empty(); })) {
            phase = Phase::Idle;
            offspring_count = 0;
            male.clear(); female.clear();
        }
    }
    bool parents_can_release() const { return phase == Phase::Idle && stop_after_batch; }
    std::string serialize() const {
        std::ostringstream out;
        out << "PRF_BREEDER_1\n" << static_cast<int>(phase) << ' ' << offspring_count << ' '
            << std::setprecision(17) << breeding_remaining << ' ' << stop_after_batch << '\n'
            << std::quoted(male) << ' ' << std::quoted(female) << '\n' << offspring.serialize();
        return out.str();
    }
    static State parse(const std::string& text) {
        State result;
        std::istringstream in{text};
        std::string tag;
        int phase_value{};
        settlement::verify_value(bool(in >> tag >> phase_value >> result.offspring_count >> result.breeding_remaining >>
            result.stop_after_batch >> std::quoted(result.male) >> std::quoted(result.female)) && tag == "PRF_BREEDER_1" &&
            phase_value >= 0 && phase_value <= 3 && result.offspring_count >= 0 && result.offspring_count <= 2 &&
            std::isfinite(result.breeding_remaining) && result.breeding_remaining >= 0, "Invalid breeding checkpoint");
        result.phase = static_cast<Phase>(phase_value);
        in >> std::ws;
        const std::string tail{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
        result.offspring = processor::State::parse(tail);
        const bool idle = result.phase == Phase::Idle;
        settlement::verify_value(idle ? result.offspring_count == 0 && result.male.empty() && result.female.empty() :
            result.offspring_count > 0 && !result.male.empty() && !result.female.empty() && result.male != result.female,
            "Invalid breeding parents checkpoint");
        size_t occupied{};
        for (size_t i = 0; i < result.offspring.jobs.size(); ++i) if (!result.offspring.jobs[i].egg.empty()) {
            ++occupied;
            settlement::verify_value(result.phase == Phase::Processing && i < static_cast<size_t>(result.offspring_count),
                "Invalid internal offspring slot");
        }
        settlement::verify_value(result.phase != Phase::Processing || occupied > 0, "Empty processing batch");
        return result;
    }
};
} // namespace prf::breeder
