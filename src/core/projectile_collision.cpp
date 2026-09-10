#include "core/projectile_collision.hpp"

#include <algorithm>
#include <cmath>

namespace siege {

std::optional<float> swept_circle_hit_fraction(
    const Vec2 segment_start, const Vec2 segment_end,
    const Vec2 circle_center, const float circle_radius) noexcept {
    const float radius = std::max(circle_radius, 0.0F);
    const Vec2 start_offset = segment_start - circle_center;
    const float constant = length_squared(start_offset) - radius * radius;
    if (constant <= 0.0F) {
        return 0.0F;
    }

    const Vec2 segment = segment_end - segment_start;
    const float quadratic = length_squared(segment);
    if (quadratic <= 0.000001F) {
        return std::nullopt;
    }

    const float linear = 2.0F * dot(start_offset, segment);
    const float discriminant = linear * linear - 4.0F * quadratic * constant;
    if (discriminant < 0.0F) {
        return std::nullopt;
    }

    const float root = std::sqrt(std::max(discriminant, 0.0F));
    const float first = (-linear - root) / (2.0F * quadratic);
    const float second = (-linear + root) / (2.0F * quadratic);
    if (first >= 0.0F && first <= 1.0F) {
        return first;
    }
    if (second >= 0.0F && second <= 1.0F) {
        return second;
    }
    return std::nullopt;
}

} // namespace siege
