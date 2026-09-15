#include "core/movement_path.hpp"

#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace siege {

std::optional<Unit::Id> pick_path_unit(const World& world, const Team team,
                                       const Vec2 world_position,
                                       const float pick_radius) noexcept {
    const float radius = std::max(0.0F, pick_radius);
    const float radius_squared = radius * radius;
    const Unit* best = nullptr;
    float best_distance_squared = radius_squared;
    for (const auto& unit : world.units()) {
        if (!unit.is_alive() || unit.team() != team) {
            continue;
        }
        const float distance_squared =
            length_squared(unit.position() - world_position);
        if (distance_squared < best_distance_squared ||
            (distance_squared == best_distance_squared && best != nullptr &&
             unit.id() < best->id())) {
            best = &unit;
            best_distance_squared = distance_squared;
        } else if (distance_squared == best_distance_squared && best == nullptr) {
            best = &unit;
        }
    }
    return best == nullptr ? std::nullopt
                           : std::optional<Unit::Id>{best->id()};
}

bool append_path_sample(std::vector<Vec2>& points, const Vec2 origin,
                        const Vec2 point, const bool force_endpoint,
                        const float sample_distance) {
    const Vec2 previous = points.empty() ? origin : points.back();
    const float distance_squared = length_squared(point - previous);
    const float minimum = std::max(0.0F, sample_distance);
    if (distance_squared <= 0.0001F ||
        (!force_endpoint && distance_squared < minimum * minimum)) {
        return false;
    }
    points.push_back(point);
    return true;
}

bool assign_movement_path(World& world, const Unit::Id unit_id,
                          std::vector<Vec2> waypoints,
                          const Team commanding_team) {
    if (!world.match_state().active() || waypoints.empty()) {
        return false;
    }
    Unit* unit = world.find_unit(unit_id);
    if (unit == nullptr || !unit->is_alive() ||
        unit->team() != commanding_team) {
        return false;
    }
    unit->replace_movement_path(std::move(waypoints));
    return true;
}

} // namespace siege
