#pragma once

#include "core/math.hpp"

#include <cstddef>
#include <optional>

namespace siege {

class World;

struct CaptureRules {
    float capture_rate;
    int maximum_effective_pressure;
};

inline constexpr CaptureRules default_capture_rules{
    .capture_rate = 5.0F,
    .maximum_effective_pressure = 3,
};

// Zone rectangles are lower-bound inclusive and upper-bound exclusive.
// Consequently, a position on a shared boundary belongs to the zone on its
// right (or below it) and can never contribute to two zones.
[[nodiscard]] std::optional<std::size_t> zone_index_for_position(
    const World& world, Vec2 position) noexcept;

void update_zone_capture(
    World& world, double fixed_delta_seconds,
    CaptureRules rules = default_capture_rules) noexcept;

} // namespace siege
