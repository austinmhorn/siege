#include "core/perception.hpp"

#include "core/math.hpp"
#include "world/unit.hpp"

#include <algorithm>
#include <cmath>

namespace siege {
namespace {

constexpr float angular_boundary_epsilon = 0.0001F;
constexpr float coincident_distance_epsilon = 0.0001F;

bool are_enemies(const Unit& observer, const Unit& target) noexcept {
    return observer.is_alive() && target.is_alive() &&
           observer.id() != target.id() && observer.team() != Team::none &&
           target.team() != Team::none && observer.team() != target.team();
}

bool within_distance(const Unit& observer, const Unit& target,
                     const float maximum_distance) noexcept {
    const Vec2 offset = target.position() - observer.position();
    return length_squared(offset) <= maximum_distance * maximum_distance;
}

} // namespace

bool inside_vision_cone(const Unit& observer, const Unit& target) noexcept {
    if (!are_enemies(observer, target) ||
        !within_distance(observer, target, observer.vision_range())) {
        return false;
    }

    const Vec2 offset = target.position() - observer.position();
    if (length_squared(offset) <= coincident_distance_epsilon) {
        return true;
    }

    const float target_bearing = facing_from_direction(offset);
    const float angular_difference =
        std::abs(shortest_angle_delta(observer.facing_angle(), target_bearing));
    const float half_angle = std::clamp(observer.vision_angle(), 0.0F, 360.0F) * 0.5F;
    return angular_difference <= half_angle + angular_boundary_epsilon;
}

bool inside_awareness_radius(const Unit& observer, const Unit& target) noexcept {
    return are_enemies(observer, target) &&
           within_distance(observer, target, observer.awareness_radius());
}

bool can_perceive(const Unit& observer, const Unit& target) noexcept {
    return inside_awareness_radius(observer, target) ||
           inside_vision_cone(observer, target);
}

} // namespace siege
