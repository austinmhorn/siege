#include "core/targeting.hpp"

#include "core/perception.hpp"
#include "core/mortar_observation.hpp"

#include <limits>
#include <cmath>

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

std::optional<Unit::Id> select_mortar_target(
    const MapDefinition& map, const Unit& observer,
    const std::span<const Unit> units) noexcept {
    std::optional<Unit::Id> best_id;
    int best_category = std::numeric_limits<int>::max();
    int best_cluster = -1;
    float best_distance_squared = std::numeric_limits<float>::max();
    const float splash_squared =
        observer.weapon().splash_radius * observer.weapon().splash_radius;

    for (const auto& candidate : units) {
        if (!mortar_target_observable(map, observer, candidate)) {
            continue;
        }
        const float distance_squared =
            length_squared(candidate.position() - observer.position());

        int cluster = 0;
        for (const auto& nearby : units) {
            if (nearby.is_alive() && nearby.team() == candidate.team() &&
                mortar_target_observable(map, observer, nearby) &&
                length_squared(nearby.position() - candidate.position()) <=
                    splash_squared) {
                ++cluster;
            }
        }
        const int category = candidate.target_category() ==
                                     TargetCategory::infantry
            ? 0
            : 1;
        const bool better = category < best_category ||
            (category == best_category && cluster > best_cluster) ||
            (category == best_category && cluster == best_cluster &&
             (distance_squared < best_distance_squared ||
              (distance_squared == best_distance_squared &&
               (!best_id.has_value() || candidate.id() < *best_id))));
        if (better) {
            best_id = candidate.id();
            best_category = category;
            best_cluster = cluster;
            best_distance_squared = distance_squared;
        }
    }
    return best_id;
}

} // namespace

std::optional<Unit::Id> select_target(
    const MapDefinition& map, const Unit& observer,
    const std::span<const Unit> units) noexcept {
    if (!observer.is_alive()) {
        return std::nullopt;
    }
    if (observer.troop_type() == TroopType::mortar) {
        return select_mortar_target(map, observer, units);
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
