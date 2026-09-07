#pragma once
// Layouts verified against the pinned executable, not UE4SS's FName wrappers.
// Pure value operations here are also exercised without loading the game.
#include <array>
#include <compare>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace prf::settlement {
inline void verify_value(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Name { uint32_t index{}, number{}; auto operator<=>(const Name&) const = default; };
struct Guid { std::array<uint32_t, 4> words{}; auto operator<=>(const Guid&) const = default; };
struct DynamicId { Guid world{}, local{}; auto operator<=>(const DynamicId&) const = default; };
struct ItemId { Name name{}; DynamicId dynamic{}; auto operator<=>(const ItemId&) const = default; };
struct ItemCount { ItemId item{}; int32_t count{}; };
struct ItemState { ItemId item{}; int32_t count{}; float corruption{}; bool operator==(const ItemState&) const = default; };
struct SlotId { Guid container{}; int32_t index{-1}; auto operator<=>(const SlotId&) const = default; };
struct Consume { SlotId slot{}; int32_t count{}; };
struct Change { ItemState state{}; SlotId target{}, source{}; };
template<class T> struct Array { T* data{}; int32_t num{}, capacity{}; };
static_assert(sizeof(Name) == 8 && sizeof(Guid) == 16 && sizeof(DynamicId) == 32);
static_assert(sizeof(ItemId) == 40 && sizeof(ItemCount) == 44 && sizeof(ItemState) == 48);
static_assert(sizeof(SlotId) == 20 && sizeof(Consume) == 24 && sizeof(Change) == 88);
static_assert(offsetof(Change, target) == 48 && offsetof(Change, source) == 68);
static_assert(sizeof(Array<Change>) == 16 && alignof(Change) == 4);

using Counts = std::map<Name, int64_t>;
inline void add_count(Counts& counts, Name name, int32_t count) {
    verify_value(name.index != 0 && count > 0, "Invalid native material row");
    auto& total = counts[name];
    verify_value(total <= std::numeric_limits<int32_t>::max() - int64_t{count}, "Native material total exceeds supported count");
    total += count;
}

// Native container filters can silently omit a row from their remainder array.
// Derive the true remainder from absolute planned slot values, never that array.
inline void account_addition(const ItemState& before, const ItemState& after, Counts& remaining) {
    verify_value(before.count >= 0 && after.count >= 0, "Negative planned stack");
    // Native packing may include visited-but-unchanged slots (including existing
    // dynamic equipment). They are neither newly created loot nor award targets.
    if (before == after) return;
    verify_value(after.item.dynamic == DynamicId{}, "Dynamic loot requires original item creation; left pending");
    if (before.count > 0) verify_value(before.item == after.item, "Plan would replace an existing item");
    verify_value(after.count >= before.count, "Material plan would remove an existing item");
    const int64_t added = int64_t{after.count} - before.count;
    verify_value(added > 0, "Material plan changes metadata without adding materials");
    auto found = remaining.find(after.item.name);
    verify_value(found != remaining.end() && found->second >= added, "Plan creates unrequested or excess materials");
    found->second -= added;
    if (found->second == 0) remaining.erase(found);
}

// The manual ground test places its outlet 2 m along the model's forward axis,
// 0.6 m above the model origin. Custom meshes will supply their own outlet later.
inline std::array<double, 3> front_outlet(const std::array<double, 3>& origin,
    const std::array<double, 4>& rotation) {
    for (const auto v : origin) verify_value(std::isfinite(v), "Invalid machine location");
    double length2{};
    for (const auto v : rotation) {
        verify_value(std::isfinite(v), "Invalid machine rotation");
        length2 += v * v;
    }
    verify_value(std::abs(length2 - 1.0) < 0.001, "Machine rotation is not normalized");
    const auto [x, y, z, w] = rotation;
    return {origin[0] + 200.0 * (1.0 - 2.0 * (y*y + z*z)),
        origin[1] + 200.0 * 2.0 * (x*y + w*z),
        origin[2] + 200.0 * 2.0 * (x*z - w*y) + 60.0};
}

enum class Phase { Resolving, Ready, Applying, Verified, GroundRequested };
struct SavedDrop { std::string item; int32_t count{}; bool operator==(const SavedDrop&) const = default; };
struct Receipt { std::string key; Phase phase{Phase::Resolving}; std::vector<SavedDrop> drops; };

inline std::string ready_record(const std::vector<SavedDrop>& drops) {
    std::ostringstream out;
    out << "DROPS " << drops.size() << '\n';
    for (const auto& row : drops) {
        verify_value(!row.item.empty() && row.count > 0, "Invalid saved material row");
        out << std::quoted(row.item) << ' ' << row.count << '\n';
    }
    out << "READY\n";
    return out.str();
}

// An interrupted RNG or apply is deliberately not replayable. That protects
// against the crashes/re-entry already observed; this is not crash recovery.
inline Receipt parse_receipt(const std::string& text, const std::string& expected_key) {
    verify_value(!text.empty() && text.back() == '\n', "Incomplete settlement receipt; no replay");
    std::istringstream in{text};
    std::string tag;
    Receipt result;
    verify_value(bool(in >> tag) && tag == "PRF_SETTLEMENT_1", "Unknown settlement receipt version");
    verify_value(bool(in >> std::quoted(result.key)) && result.key == expected_key, "Settlement receipt identity mismatch");
    verify_value(bool(in >> tag) && tag == "RESOLVING", "Missing settlement RNG claim");
    while (in >> tag) {
        if (tag == "DROPS" && result.phase == Phase::Resolving) {
            size_t count{};
            verify_value(bool(in >> count) && count <= text.size(), "Invalid saved drop count");
            for (size_t i = 0; i < count; ++i) {
                SavedDrop row;
                verify_value(bool(in >> std::quoted(row.item) >> row.count) && !row.item.empty() && row.count > 0,
                    "Incomplete native drop result; no reroll");
                result.drops.push_back(std::move(row));
            }
            verify_value(bool(in >> tag) && tag == "READY", "Native drop result was not fully saved; no reroll");
            result.phase = Phase::Ready;
        } else if (tag == "APPLYING" && result.phase == Phase::Ready) {
            result.phase = Phase::Applying;
        } else if (tag == "VERIFIED" && result.phase == Phase::Applying) {
            result.phase = Phase::Verified;
        } else if (tag == "GROUND_REQUESTED" && result.phase == Phase::Applying) {
            // Vanilla creates drop models asynchronously. Issued is terminal
            // for replay, but must not be misreported as observed/verified.
            result.phase = Phase::GroundRequested;
        } else throw std::runtime_error("Invalid settlement receipt sequence; no replay");
    }
    verify_value(in.eof(), "Unreadable settlement receipt");
    return result;
}
} // namespace prf::settlement
