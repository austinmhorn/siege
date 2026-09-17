#include "core/tactical_escort.hpp"

#include "core/math.hpp"

namespace siege {

bool is_tank_escort_anchor(const TroopType troop_type) noexcept {
    return troop_type == TroopType::light_tank ||
           troop_type == TroopType::medium_tank ||
           troop_type == TroopType::heavy_tank;
}

std::optional<TacticalEscortTarget> anti_tank_escort_target(
    const Unit& unit, const std::span<const Unit> units,
    const TacticalEscortRules rules) noexcept {
    if (!unit.is_alive() || unit.troop_type() != TroopType::anti_tank ||
        unit.team() == Team::none || !unit.group_id().has_value()) {
        return std::nullopt;
    }

    const Unit* anchor = nullptr;
    std::size_t escort_count = 0;
    std::size_t slot_index = 0;
    for (const Unit& candidate : units) {
        if (!candidate.is_alive() || candidate.team() != unit.team() ||
            candidate.group_id() != unit.group_id()) {
            continue;
        }
        if (is_tank_escort_anchor(candidate.troop_type()) &&
            (anchor == nullptr || candidate.id() < anchor->id())) {
            anchor = &candidate;
        }
        if (candidate.troop_type() == TroopType::anti_tank) {
            ++escort_count;
            if (candidate.id() < unit.id()) {
                ++slot_index;
            }
        }
    }
    if (anchor == nullptr) {
        return std::nullopt;
    }

    const float centered_slot = static_cast<float>(slot_index) -
        (static_cast<float>(escort_count) - 1.0F) * 0.5F;
    const Vec2 forward = direction_from_facing(anchor->facing_angle());
    const Vec2 lateral{-forward.y, forward.x};
    const Vec2 desired = anchor->position() -
        forward * rules.trailing_distance +
        lateral * (centered_slot * rules.lateral_spacing);
    return TacticalEscortTarget{anchor->id(), desired, slot_index,
                                escort_count};
}

bool anti_tank_escort_movement_allowed(
    const Unit& unit, const bool has_combat_target,
    const CombatMovementState combat_state) noexcept {
    if (unit.troop_type() != TroopType::anti_tank ||
        unit.has_movement_path() ||
        unit.tactical_order() == TacticalOrder::hold ||
        unit.tactical_order() == TacticalOrder::regroup) {
        return false;
    }
    return !has_combat_target ||
           combat_state == CombatMovementState::inactive ||
           combat_state == CombatMovementState::advancing;
}

} // namespace siege
