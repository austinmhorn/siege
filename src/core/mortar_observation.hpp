#pragma once

#include <algorithm>

#include "core/map_definition.hpp"
#include "core/weapon.hpp"
#include "world/unit.hpp"

namespace siege {

inline constexpr float mortar_observation_zone_count = 2.0F;

// Mortar observation is intentionally separate from ballistic capability.
// The weapon can reach across the map, while the emplacement can only acquire
// targets within a map-derived local area. Keeping this policy outside ordinary
// cone/range/LOS perception prevents other troops from inheriting indirect
// awareness accidentally and leaves room for future team spotting.
[[nodiscard]] inline float mortar_observation_range(
    const MapDefinition& map) noexcept {
    if (map.zones.empty()) {
        return 0.0F;
    }
    return std::max(map.zones.front().bounds.width, 0.0F) *
           mortar_observation_zone_count;
}

[[nodiscard]] inline bool mortar_target_observable(
    const MapDefinition& map, const Unit& observer,
    const Unit& target) noexcept {
    if (observer.troop_type() != TroopType::mortar || !observer.is_alive() ||
        !target.is_alive() || observer.id() == target.id() ||
        observer.team() == Team::none || target.team() == Team::none ||
        observer.team() == target.team()) {
        return false;
    }

    const TeamForwardDefinition* forward =
        team_forward_definition(map, observer.team());
    const auto target_zone =
        map_zone_index_for_position(map, target.position());
    if (forward == nullptr || !target_zone.has_value() ||
        *target_zone == forward->opposing_home_zone_index) {
        return false;
    }

    const float distance_squared =
        length_squared(target.position() - observer.position());
    const float minimum_range =
        std::max(observer.weapon().minimum_range, 0.0F);
    const float maximum_range = mortar_observation_range(map);
    return distance_squared >= minimum_range * minimum_range &&
           distance_squared <= maximum_range * maximum_range;
}

} // namespace siege
