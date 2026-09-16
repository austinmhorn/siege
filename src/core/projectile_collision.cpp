#include "core/projectile_collision.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

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

std::optional<float> swept_bounds_hit_fraction(
    const Vec2 segment_start, const Vec2 segment_end,
    const Bounds bounds) noexcept {
    const Vec2 movement = segment_end - segment_start;
    float entry = 0.0F;
    float exit = 1.0F;
    const auto clip_axis = [&](const float origin, const float delta,
                               const float minimum, const float maximum) {
        if (std::abs(delta) <= 0.000001F) {
            return origin >= minimum && origin <= maximum;
        }
        const float first = (minimum - origin) / delta;
        const float second = (maximum - origin) / delta;
        entry = std::max(entry, std::min(first, second));
        exit = std::min(exit, std::max(first, second));
        return entry <= exit;
    };
    if (!clip_axis(segment_start.x, movement.x, bounds.x,
                   bounds.x + bounds.width) ||
        !clip_axis(segment_start.y, movement.y, bounds.y,
                   bounds.y + bounds.height) ||
        exit < 0.0F || entry > 1.0F) {
        return std::nullopt;
    }
    return std::clamp(entry, 0.0F, 1.0F);
}

std::optional<EnvironmentProjectileHit> nearest_environment_projectile_hit(
    const MapDefinition& map, const Vec2 segment_start,
    const Vec2 segment_end) noexcept {
    const EnvironmentObjectDefinition* nearest = nullptr;
    float nearest_fraction = std::numeric_limits<float>::infinity();
    constexpr float tie_epsilon = 0.000001F;
    for (const EnvironmentObjectDefinition& object : map.environment_objects) {
        if (!object.physical.blocks_projectiles) {
            continue;
        }
        const auto fraction = swept_bounds_hit_fraction(
            segment_start, segment_end, object.footprint);
        if (!fraction.has_value()) {
            continue;
        }
        if (nearest == nullptr ||
            *fraction < nearest_fraction - tie_epsilon ||
            (std::abs(*fraction - nearest_fraction) <= tie_epsilon &&
             object.id < nearest->id)) {
            nearest = &object;
            nearest_fraction = *fraction;
        }
    }
    if (nearest == nullptr) {
        return std::nullopt;
    }
    return EnvironmentProjectileHit{
        nearest, nearest_fraction,
        segment_start + (segment_end - segment_start) * nearest_fraction};
}

} // namespace siege
