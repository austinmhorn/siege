#pragma once

#include "core/map_definition.hpp"

namespace siege {

// Environment-only visibility query. Unit bodies do not occlude one another.
// Touching an authored opaque footprint is considered blocked.
[[nodiscard]] bool environment_line_of_sight_clear(
    const MapDefinition& map, Vec2 observer, Vec2 target) noexcept;

} // namespace siege
