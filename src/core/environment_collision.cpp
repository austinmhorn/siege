#include "core/environment_collision.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

namespace siege {
namespace {

constexpr float collision_epsilon = 0.001F;
constexpr int maximum_slide_iterations = 8;

Bounds expanded(const Bounds& bounds, const float radius) noexcept {
    return Bounds{bounds.x - radius, bounds.y - radius,
                  bounds.width + radius * 2.0F,
                  bounds.height + radius * 2.0F};
}

bool circle_intersects_bounds(const Vec2 center, const float radius,
                              const Bounds& bounds) noexcept {
    const float nearest_x = std::clamp(
        center.x, bounds.x, bounds.x + bounds.width);
    const float nearest_y = std::clamp(
        center.y, bounds.y, bounds.y + bounds.height);
    const Vec2 offset = center - Vec2{nearest_x, nearest_y};
    return length_squared(offset) <= radius * radius;
}

struct SweepHit {
    float fraction;
    Vec2 normal;
    std::string_view object_id;
};

std::optional<SweepHit> sweep_point_against_bounds(
    const Vec2 start, const Vec2 delta, const Bounds& bounds,
    const std::string_view object_id) noexcept {
    float entry = -std::numeric_limits<float>::infinity();
    float exit = std::numeric_limits<float>::infinity();
    Vec2 normal{};

    const auto update_axis = [&](const float origin, const float movement,
                                 const float minimum, const float maximum,
                                 const Vec2 negative_normal,
                                 const Vec2 positive_normal) {
        if (std::abs(movement) <= 0.000001F) {
            return origin >= minimum && origin <= maximum;
        }
        const float first = (minimum - origin) / movement;
        const float second = (maximum - origin) / movement;
        const float near_time = std::min(first, second);
        const float far_time = std::max(first, second);
        const Vec2 near_normal = movement > 0.0F ? negative_normal
                                                 : positive_normal;
        if (near_time > entry) {
            entry = near_time;
            normal = near_normal;
        }
        exit = std::min(exit, far_time);
        return entry <= exit;
    };

    if (!update_axis(start.x, delta.x, bounds.x,
                     bounds.x + bounds.width, {-1.0F, 0.0F},
                     {1.0F, 0.0F}) ||
        !update_axis(start.y, delta.y, bounds.y,
                     bounds.y + bounds.height, {0.0F, -1.0F},
                     {0.0F, 1.0F}) ||
        exit < 0.0F || entry > 1.0F || entry < 0.0F) {
        return std::nullopt;
    }

    // A center resting on a face and moving parallel to or away from it is
    // not a new collision. This keeps sliding stable on subsequent passes.
    if (entry <= collision_epsilon && dot(delta, normal) >= -0.000001F) {
        return std::nullopt;
    }
    return SweepHit{std::clamp(entry, 0.0F, 1.0F), normal, object_id};
}

std::optional<SweepHit> earliest_hit(const MapDefinition& map,
                                     const Vec2 start, const Vec2 delta,
                                     const float radius) noexcept {
    std::optional<SweepHit> nearest;
    for (const EnvironmentObjectDefinition& object : map.environment_objects) {
        if (!object.physical.blocks_unit_movement) {
            continue;
        }
        const auto candidate = sweep_point_against_bounds(
            start, delta, expanded(object.footprint, radius), object.id);
        if (!candidate.has_value()) {
            continue;
        }
        if (!nearest.has_value() ||
            candidate->fraction < nearest->fraction - 0.000001F ||
            (std::abs(candidate->fraction - nearest->fraction) <= 0.000001F &&
             candidate->object_id < nearest->object_id)) {
            nearest = candidate;
        }
    }
    return nearest;
}

} // namespace

bool unit_overlaps_blocking_environment(const MapDefinition& map,
                                        const Vec2 position,
                                        const float unit_radius) noexcept {
    const float radius = std::max(0.0F, unit_radius);
    for (const EnvironmentObjectDefinition& object : map.environment_objects) {
        if (object.physical.blocks_unit_movement &&
            circle_intersects_bounds(position, radius, object.footprint)) {
            return true;
        }
    }
    return false;
}

Vec2 resolve_unit_environment_movement(const MapDefinition& map,
                                       const Vec2 start, const Vec2 desired,
                                       const float unit_radius) noexcept {
    const float radius = std::max(0.0F, unit_radius);
    Vec2 position = start;
    Vec2 remaining = desired - start;
    for (int iteration = 0;
         iteration < maximum_slide_iterations &&
         length_squared(remaining) > 0.0000001F;
         ++iteration) {
        const auto hit = earliest_hit(map, position, remaining, radius);
        if (!hit.has_value()) {
            position = position + remaining;
            break;
        }

        position = position + remaining * hit->fraction +
                   hit->normal * collision_epsilon;
        remaining = remaining * (1.0F - hit->fraction);
        const float inward = dot(remaining, hit->normal);
        if (inward < 0.0F) {
            remaining = remaining - hit->normal * inward;
        }
    }
    return position;
}

} // namespace siege
