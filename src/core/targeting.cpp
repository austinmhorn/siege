#include "core/targeting.hpp"

#include "core/perception.hpp"

#include <limits>

namespace siege {
namespace {

const Unit* find_unit(const std::span<const Unit> units,
                      const Unit::Id id) noexcept {
    for (const auto& unit : units) {
        if (unit.id() == id) {
            return &unit;
        }
    }
    return nullptr;
}

int target_priority(const Unit& observer, const Unit& candidate) noexcept {
    return observer.prefers_vehicle_targets() &&
                   candidate.target_category() == TargetCategory::vehicle
        ? 0
        : 1;
}

} // namespace

std::optional<Unit::Id> select_target(
    const MapDefinition& map, const Unit& observer,
    const std::span<const Unit> units) noexcept {
    if (!observer.is_alive()) {
        return std::nullopt;
    }

    const Unit* current = observer.target_id().has_value()
        ? find_unit(units, *observer.target_id())
        : nullptr;
    if (current != nullptr && !can_perceive(map, observer, *current)) {
        current = nullptr;
    }
    const int current_priority = current == nullptr
        ? std::numeric_limits<int>::max()
        : target_priority(observer, *current);

    // Preserve the existing target within a priority tier. A higher-priority
    // perceivable category may preempt it (Anti-Tank infantry switching from
    // infantry to a visible vehicle), but distance changes alone never flicker.
    if (current != nullptr && current_priority == 0) {
        return current->id();
    }

    std::optional<Unit::Id> nearest_id;
    float nearest_distance_squared = std::numeric_limits<float>::max();
    int nearest_priority = current_priority;
    for (const auto& candidate : units) {
        if (!can_perceive(map, observer, candidate)) {
            continue;
        }

        const int candidate_priority = target_priority(observer, candidate);
        if (candidate_priority >= current_priority) {
            continue;
        }

        const float candidate_distance_squared =
            length_squared(candidate.position() - observer.position());
        const bool nearer_within_priority =
            candidate_distance_squared < nearest_distance_squared ||
            (candidate_distance_squared == nearest_distance_squared &&
             (!nearest_id.has_value() || candidate.id() < *nearest_id));
        if (candidate_priority < nearest_priority ||
            (candidate_priority == nearest_priority &&
             nearer_within_priority)) {
            nearest_id = candidate.id();
            nearest_distance_squared = candidate_distance_squared;
            nearest_priority = candidate_priority;
        }
    }
    return nearest_id.has_value()
        ? nearest_id
        : current == nullptr ? std::nullopt
                             : std::optional<Unit::Id>{current->id()};
}

} // namespace siege
