#pragma once

#include "core/math.hpp"
#include "world/unit.hpp"

#include <optional>
#include <vector>

namespace siege {

class World;

struct MovementPathRules {
    float unit_pick_radius;
    float sample_distance;
    float waypoint_reach_radius;
};

inline constexpr MovementPathRules default_movement_path_rules{
    .unit_pick_radius = 34.0F,
    .sample_distance = 18.0F,
    .waypoint_reach_radius = 14.0F,
};

[[nodiscard]] std::optional<Unit::Id> pick_path_unit(
    const World& world, Team team, Vec2 world_position,
    float pick_radius = default_movement_path_rules.unit_pick_radius) noexcept;

[[nodiscard]] bool append_path_sample(
    std::vector<Vec2>& points, Vec2 origin, Vec2 point, bool force_endpoint,
    float sample_distance = default_movement_path_rules.sample_distance);

} // namespace siege
