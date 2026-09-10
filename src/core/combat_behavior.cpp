#include "core/combat_behavior.hpp"

#include <algorithm>

namespace siege {

CombatMovementState combat_movement_for(const Unit& observer,
                                        const Unit& target) noexcept {
    const float preferred_range = std::max(observer.preferred_combat_range(), 0.0F);
    const float tolerance = std::max(observer.range_tolerance(), 0.0F);
    const float lower_boundary = std::max(preferred_range - tolerance, 0.0F);
    const float upper_boundary = preferred_range + tolerance;
    const float distance_squared =
        length_squared(target.position() - observer.position());

    if (distance_squared > upper_boundary * upper_boundary) {
        return CombatMovementState::closing;
    }
    if (distance_squared < lower_boundary * lower_boundary) {
        return CombatMovementState::retreating;
    }
    return CombatMovementState::engaging;
}

} // namespace siege
