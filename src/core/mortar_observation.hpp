#pragma once

#include <algorithm>

#include "core/map_definition.hpp"
#include "core/weapon.hpp"
#include "world/unit.hpp"

namespace siege {

// Mortars are explicit map-wide indirect-fire observers. This policy is kept
// separate from ordinary cone/range/LOS perception so no other troop inherits
// artillery awareness accidentally.
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
    const float maximum_range = effective_weapon_range(map, observer.weapon());
    return distance_squared >= minimum_range * minimum_range &&
           distance_squared <= maximum_range * maximum_range;
}

} // namespace siege
