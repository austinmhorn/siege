#include "core/weapon.hpp"

#include "core/math.hpp"
#include "core/map_definition.hpp"
#include "core/mortar_observation.hpp"
#include "world/unit.hpp"

#include <algorithm>
#include <cmath>

namespace siege {
namespace {

constexpr float angular_boundary_epsilon = 0.0001F;

} // namespace

float effective_weapon_range(const MapDefinition& map,
                             const WeaponDefinition& weapon) noexcept {
    if (weapon.maximum_range_mode == WeaponRangeMode::map_diagonal) {
        return std::hypot(map.logical_width, map.logical_height);
    }
    return std::max(weapon.range, 0.0F);
}

bool can_fire_at(const Unit& observer, const Unit& target) noexcept {
    if (!observer.is_alive() || !target.is_alive() ||
        !observer.target_id().has_value() || *observer.target_id() != target.id() ||
        observer.id() == target.id() || observer.team() == Team::none ||
        target.team() == Team::none || observer.team() == target.team() ||
        observer.weapon_cooldown_remaining() > 0.0F) {
        return false;
    }

    const Vec2 offset = target.position() - observer.position();
    const float range = std::max(observer.weapon().range, 0.0F);
    const float minimum_range =
        std::clamp(observer.weapon().minimum_range, 0.0F, range);
    if (length_squared(offset) > range * range ||
        length_squared(offset) < minimum_range * minimum_range ||
        length_squared(offset) <= 0.0001F) {
        return false;
    }

    if (observer.mobility_mode() == MobilityMode::player_path_only &&
        observer.is_relocating()) {
        return false;
    }

    const float target_bearing = facing_from_direction(offset);
    const float angular_difference =
        std::abs(shortest_angle_delta(observer.weapon_facing_angle(),
                                      target_bearing));
    const float half_arc =
        std::clamp(observer.weapon().firing_arc, 0.0F, 360.0F) * 0.5F;
    return angular_difference <= half_arc + angular_boundary_epsilon;
}

bool can_fire_at(const MapDefinition& map, const Unit& observer,
                 const Unit& target) noexcept {
    if (observer.troop_type() != TroopType::mortar) {
        return can_fire_at(observer, target);
    }
    if (!mortar_target_observable(map, observer, target) ||
        !observer.target_id().has_value() ||
        *observer.target_id() != target.id() ||
        observer.weapon_cooldown_remaining() > 0.0F ||
        observer.is_relocating()) {
        return false;
    }

    const Vec2 offset = target.position() - observer.position();
    const float target_bearing = facing_from_direction(offset);
    const float angular_difference = std::abs(shortest_angle_delta(
        observer.weapon_facing_angle(), target_bearing));
    const float half_arc =
        std::clamp(observer.weapon().firing_arc, 0.0F, 360.0F) * 0.5F;
    return angular_difference <= half_arc + angular_boundary_epsilon;
}

} // namespace siege
