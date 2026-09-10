#pragma once

#include "core/math.hpp"

#include <optional>

namespace siege {

// Returns the earliest normalized time in [0, 1] where the segment intersects
// the circle. A segment starting inside the circle hits at time zero.
[[nodiscard]] std::optional<float>
swept_circle_hit_fraction(Vec2 segment_start, Vec2 segment_end,
                          Vec2 circle_center, float circle_radius) noexcept;

} // namespace siege
