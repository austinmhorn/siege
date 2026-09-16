#pragma once

#include "core/map_definition.hpp"

namespace siege {

[[nodiscard]] bool unit_overlaps_blocking_environment(
    const MapDefinition& map, Vec2 position, float unit_radius) noexcept;

// Sweeps a unit center against map-authored blocking footprints expanded by
// the unit radius. Remaining motion is projected along the earliest obstacle
// face, producing deterministic local edge sliding without global pathfinding.
[[nodiscard]] Vec2 resolve_unit_environment_movement(
    const MapDefinition& map, Vec2 start, Vec2 desired,
    float unit_radius) noexcept;

} // namespace siege
