#pragma once

#include "core/math.hpp"
#include "world/unit.hpp"

#include <cstddef>

namespace siege {

class World;
class Zone;

inline constexpr double ai_objective_natural_occupancy_grace_seconds = 2.0;
inline constexpr float ai_objective_hold_inset = 12.0F;
inline constexpr float ai_objective_arrival_tolerance = 28.0F;

// The holder point is centered in the team's rear half of the objective. Its
// vertical position preserves the unit's lane while remaining safely inside.
[[nodiscard]] Vec2 ai_objective_hold_position(
    const World& world, const Zone& zone, Team team, float preferred_y,
    float unit_radius) noexcept;

[[nodiscard]] bool is_objective_holder(const World& world,
                                       Unit::Id unit_id) noexcept;

// Player-authored movement and explicit positional orders take precedence.
[[nodiscard]] bool ai_objective_movement_allowed(const Unit& unit) noexcept;

} // namespace siege
