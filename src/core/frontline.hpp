#pragma once

#include "core/math.hpp"
#include "world/zone.hpp"

#include <cstddef>
#include <optional>

namespace siege {

class World;

struct FrontlineRules {
    float hold_depth_fraction;
    float hold_return_distance;
    float boundary_inset;
};

struct FrontlineObjective {
    std::size_t zone_index;
    float forward_boundary_x;
    float hold_x;
};

inline constexpr FrontlineRules default_frontline_rules{
    .hold_depth_fraction = 0.70F,
    .hold_return_distance = 100.0F,
    .boundary_inset = 0.01F,
};

[[nodiscard]] std::optional<FrontlineObjective> frontline_objective(
    const World& world, Team team,
    FrontlineRules rules = default_frontline_rules) noexcept;

[[nodiscard]] float autonomous_advance_x(
    const World& world, Team team, Vec2 position,
    FrontlineRules rules = default_frontline_rules) noexcept;

[[nodiscard]] Vec2 constrain_to_frontline(
    const World& world, Team team, Vec2 current_position,
    Vec2 proposed_position,
    FrontlineRules rules = default_frontline_rules) noexcept;

} // namespace siege
