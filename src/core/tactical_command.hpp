#pragma once

#include "core/math.hpp"
#include "world/unit.hpp"

#include <cstddef>
#include <span>

namespace siege {

class World;

struct TacticalRules {
    float hold_leash_radius;
    float hold_return_start_fraction;
    float regroup_completion_radius;
};

inline constexpr TacticalRules default_tactical_rules{
    .hold_leash_radius = 120.0F,
    .hold_return_start_fraction = 0.70F,
    .regroup_completion_radius = 42.0F,
};

[[nodiscard]] std::size_t apply_tactical_order(
    World& world, std::span<const Unit::Id> unit_ids, TacticalOrder order,
    Team commanding_team = Team::team_a);

} // namespace siege
