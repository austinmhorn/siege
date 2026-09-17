#pragma once

#include "world/unit.hpp"

#include <cstddef>
#include <optional>
#include <span>

namespace siege {

struct TacticalEscortRules {
    float trailing_distance{140.0F};
    float lateral_spacing{64.0F};
    float arrival_tolerance{36.0F};
};

inline constexpr TacticalEscortRules default_tactical_escort_rules{};

struct TacticalEscortTarget {
    Unit::Id anchor_id{};
    Vec2 desired_position{};
    std::size_t slot_index{};
    std::size_t slot_count{};
};

[[nodiscard]] bool is_tank_escort_anchor(TroopType troop_type) noexcept;

[[nodiscard]] std::optional<TacticalEscortTarget> anti_tank_escort_target(
    const Unit& unit, std::span<const Unit> units,
    TacticalEscortRules rules = default_tactical_escort_rules) noexcept;

[[nodiscard]] bool anti_tank_escort_movement_allowed(
    const Unit& unit, bool has_combat_target,
    CombatMovementState combat_state) noexcept;

} // namespace siege
