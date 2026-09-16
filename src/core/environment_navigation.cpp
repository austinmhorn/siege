#include "core/environment_navigation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace siege {
namespace {

constexpr float navigation_clearance = 2.0F;
constexpr float visibility_epsilon = 0.001F;
constexpr float distance_epsilon = 0.0001F;

Bounds expanded_footprint(const EnvironmentObjectDefinition& object,
                          const float radius) noexcept {
    const float expansion = std::max(0.0F, radius) + navigation_clearance;
    return Bounds{object.footprint.x - expansion,
                  object.footprint.y - expansion,
                  object.footprint.width + expansion * 2.0F,
                  object.footprint.height + expansion * 2.0F};
}

bool inside_open(const Bounds& bounds, const Vec2 point) noexcept {
    return point.x > bounds.x + visibility_epsilon &&
           point.x < bounds.x + bounds.width - visibility_epsilon &&
           point.y > bounds.y + visibility_epsilon &&
           point.y < bounds.y + bounds.height - visibility_epsilon;
}

bool segment_intersects_open_bounds(const Vec2 from, const Vec2 to,
                                    Bounds bounds) noexcept {
    bounds.x += visibility_epsilon;
    bounds.y += visibility_epsilon;
    bounds.width -= visibility_epsilon * 2.0F;
    bounds.height -= visibility_epsilon * 2.0F;
    if (bounds.width <= 0.0F || bounds.height <= 0.0F) {
        return false;
    }

    const Vec2 delta = to - from;
    float entry = 0.0F;
    float exit = 1.0F;
    const auto clip_axis = [&](const float origin, const float movement,
                               const float minimum, const float maximum) {
        if (std::abs(movement) <= 0.000001F) {
            return origin >= minimum && origin <= maximum;
        }
        const float first = (minimum - origin) / movement;
        const float second = (maximum - origin) / movement;
        entry = std::max(entry, std::min(first, second));
        exit = std::min(exit, std::max(first, second));
        return entry <= exit;
    };
    return clip_axis(from.x, delta.x, bounds.x, bounds.x + bounds.width) &&
           clip_axis(from.y, delta.y, bounds.y, bounds.y + bounds.height) &&
           exit >= 0.0F && entry <= 1.0F;
}

std::vector<Bounds> blocking_bounds(const MapDefinition& map,
                                    const float radius) {
    std::vector<Bounds> result;
    result.reserve(map.environment_objects.size());
    for (const EnvironmentObjectDefinition& object : map.environment_objects) {
        if (object.physical.blocks_unit_movement) {
            result.push_back(expanded_footprint(object, radius));
        }
    }
    return result;
}

bool segment_clear(const std::vector<Bounds>& obstacles, const Vec2 from,
                   const Vec2 to) noexcept {
    for (const Bounds& obstacle : obstacles) {
        if (segment_intersects_open_bounds(from, to, obstacle)) {
            return false;
        }
    }
    return true;
}

Vec2 clamp_to_world(const MapDefinition& map, const float radius,
                    Vec2 point) noexcept {
    const float inset = std::max(0.0F, radius) + navigation_clearance;
    point.x = std::clamp(point.x, inset,
                         std::max(inset, map.logical_width - inset));
    point.y = std::clamp(point.y, inset,
                         std::max(inset, map.logical_height - inset));
    return point;
}

Vec2 resolve_destination(const MapDefinition& map,
                         const std::vector<Bounds>& obstacles,
                         const float radius, Vec2 destination) noexcept {
    destination = clamp_to_world(map, radius, destination);
    for (std::size_t pass = 0; pass <= obstacles.size(); ++pass) {
        bool adjusted = false;
        for (const Bounds& obstacle : obstacles) {
            if (!inside_open(obstacle, destination)) {
                continue;
            }
            const std::array<float, 4> distances{
                destination.x - obstacle.x,
                obstacle.x + obstacle.width - destination.x,
                destination.y - obstacle.y,
                obstacle.y + obstacle.height - destination.y,
            };
            std::size_t nearest_side = 0;
            for (std::size_t side = 1; side < distances.size(); ++side) {
                if (distances[side] < distances[nearest_side] -
                                          distance_epsilon) {
                    nearest_side = side;
                }
            }
            switch (nearest_side) {
            case 0:
                destination.x = obstacle.x;
                break;
            case 1:
                destination.x = obstacle.x + obstacle.width;
                break;
            case 2:
                destination.y = obstacle.y;
                break;
            case 3:
                destination.y = obstacle.y + obstacle.height;
                break;
            }
            destination = clamp_to_world(map, radius, destination);
            adjusted = true;
            break;
        }
        if (!adjusted) {
            break;
        }
    }
    return destination;
}

} // namespace

EnvironmentNavigationRoute environment_navigation_route(
    const MapDefinition& map, const float unit_radius, const Vec2 start,
    const Vec2 destination) {
    const std::vector<Bounds> obstacles = blocking_bounds(map, unit_radius);
    const Vec2 routing_start =
        resolve_destination(map, obstacles, unit_radius, start);
    const Vec2 resolved =
        resolve_destination(map, obstacles, unit_radius, destination);
    EnvironmentNavigationRoute route{destination, resolved, {}};
    if (length_squared(routing_start - start) >
        distance_epsilon * distance_epsilon) {
        route.waypoints.push_back(routing_start);
    }
    if (segment_clear(obstacles, routing_start, resolved)) {
        route.waypoints.push_back(resolved);
        return route;
    }

    std::vector<Vec2> nodes{routing_start, resolved};
    nodes.reserve(2 + obstacles.size() * 4);
    for (const Bounds& obstacle : obstacles) {
        const std::array<Vec2, 4> corners{
            Vec2{obstacle.x, obstacle.y},
            Vec2{obstacle.x + obstacle.width, obstacle.y},
            Vec2{obstacle.x + obstacle.width,
                 obstacle.y + obstacle.height},
            Vec2{obstacle.x, obstacle.y + obstacle.height},
        };
        for (const Vec2 corner : corners) {
            const bool inside_another = std::ranges::any_of(
                obstacles, [&](const Bounds& candidate) {
                    return inside_open(candidate, corner);
                });
            if (!inside_another) {
                nodes.push_back(corner);
            }
        }
    }

    const float infinity = std::numeric_limits<float>::infinity();
    const std::size_t invalid = nodes.size();
    std::vector<float> distances(nodes.size(), infinity);
    std::vector<std::size_t> previous(nodes.size(), invalid);
    std::vector<bool> visited(nodes.size(), false);
    distances[0] = 0.0F;

    for (std::size_t iteration = 0; iteration < nodes.size(); ++iteration) {
        std::size_t current = invalid;
        for (std::size_t index = 0; index < nodes.size(); ++index) {
            if (visited[index] || !std::isfinite(distances[index])) {
                continue;
            }
            if (current == invalid ||
                distances[index] < distances[current] - distance_epsilon ||
                (std::abs(distances[index] - distances[current]) <=
                     distance_epsilon &&
                 index < current)) {
                current = index;
            }
        }
        if (current == invalid || current == 1) {
            break;
        }
        visited[current] = true;
        for (std::size_t candidate = 1; candidate < nodes.size(); ++candidate) {
            if (candidate == current || visited[candidate] ||
                !segment_clear(obstacles, nodes[current], nodes[candidate])) {
                continue;
            }
            const float candidate_distance =
                distances[current] + length(nodes[candidate] - nodes[current]);
            if (candidate_distance < distances[candidate] - distance_epsilon ||
                (std::abs(candidate_distance - distances[candidate]) <=
                     distance_epsilon &&
                 current < previous[candidate])) {
                distances[candidate] = candidate_distance;
                previous[candidate] = current;
            }
        }
    }

    if (!std::isfinite(distances[1])) {
        return route;
    }
    std::vector<Vec2> reversed;
    for (std::size_t node = 1; node != 0 && node != invalid;
         node = previous[node]) {
        reversed.push_back(nodes[node]);
    }
    if (reversed.empty() || previous[1] == invalid) {
        return route;
    }
    route.waypoints.insert(route.waypoints.end(), reversed.rbegin(),
                           reversed.rend());
    return route;
}

bool environment_navigation_segment_clear(const MapDefinition& map,
                                          const float unit_radius,
                                          const Vec2 from,
                                          const Vec2 to) noexcept {
    return segment_clear(blocking_bounds(map, unit_radius), from, to);
}

} // namespace siege
