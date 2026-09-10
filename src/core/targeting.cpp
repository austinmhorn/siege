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

} // namespace

std::optional<Unit::Id> select_target(const Unit& observer,
                                      const std::span<const Unit> units) noexcept {
    if (!observer.is_alive()) {
        return std::nullopt;
    }

    if (observer.target_id().has_value()) {
        const Unit* current = find_unit(units, *observer.target_id());
        if (current != nullptr && can_perceive(observer, *current)) {
            return current->id();
        }
    }

    std::optional<Unit::Id> nearest_id;
    float nearest_distance_squared = std::numeric_limits<float>::max();
    for (const auto& candidate : units) {
        if (!can_perceive(observer, candidate)) {
            continue;
        }

        const float candidate_distance_squared =
            length_squared(candidate.position() - observer.position());
        if (candidate_distance_squared < nearest_distance_squared ||
            (candidate_distance_squared == nearest_distance_squared &&
             (!nearest_id.has_value() || candidate.id() < *nearest_id))) {
            nearest_id = candidate.id();
            nearest_distance_squared = candidate_distance_squared;
        }
    }
    return nearest_id;
}

} // namespace siege
