#pragma once

#include "core/map_definition.hpp"

#include <vector>

namespace siege {

struct EnvironmentNavigationRoute {
    Vec2 requested_destination;
    Vec2 resolved_destination;
    std::vector<Vec2> waypoints;
};

// Returns an ordered route excluding start and including the resolved
// destination. Blocking footprints are expanded by unit radius plus a small
// clearance before visibility is evaluated.
[[nodiscard]] EnvironmentNavigationRoute environment_navigation_route(
    const MapDefinition& map, float unit_radius, Vec2 start,
    Vec2 destination);

[[nodiscard]] bool environment_navigation_segment_clear(
    const MapDefinition& map, float unit_radius, Vec2 from, Vec2 to) noexcept;

} // namespace siege
