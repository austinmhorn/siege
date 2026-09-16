#pragma once

#include "core/map_definition.hpp"
#include "core/math.hpp"

#include <optional>

namespace siege {

// Returns the earliest normalized time in [0, 1] where the segment intersects
// the circle. A segment starting inside the circle hits at time zero.
[[nodiscard]] std::optional<float>
swept_circle_hit_fraction(Vec2 segment_start, Vec2 segment_end,
                          Vec2 circle_center, float circle_radius) noexcept;

// Returns the earliest normalized time in [0, 1] where the segment intersects
// the closed axis-aligned bounds. A segment starting inside hits at zero.
[[nodiscard]] std::optional<float> swept_bounds_hit_fraction(
    Vec2 segment_start, Vec2 segment_end, Bounds bounds) noexcept;

struct EnvironmentProjectileHit {
    const EnvironmentObjectDefinition* object;
    float segment_fraction;
    Vec2 impact_position;
};

// Finds the first map-authored projectile blocker. Equal-time impacts are
// resolved by stable environment ID.
[[nodiscard]] std::optional<EnvironmentProjectileHit>
nearest_environment_projectile_hit(const MapDefinition& map,
                                   Vec2 segment_start,
                                   Vec2 segment_end) noexcept;

} // namespace siege
