#pragma once

#include "core/math.hpp"
#include "world/zone.hpp"

#include <cstddef>
#include <optional>

namespace siege {

class World;

struct FrontlineRules {
    float hold_depth_fraction;
    float hold_return_distance;
    float boundary_inset;
};

struct FrontlineObjective {
    std::size_t zone_index;
    float forward_boundary_x;
    float hold_x;
};

struct TerminalStandoffRules {
    float minimum_clearance;
    float hit_radius_padding;
    float arrival_tolerance;
};

struct TerminalFrontlineStandoff {
    float boundary_x;
    float position_x;
    float clearance;
};

inline constexpr FrontlineRules default_frontline_rules{
    .hold_depth_fraction = 0.70F,
    .hold_return_distance = 100.0F,
    .boundary_inset = 0.01F,
};

inline constexpr TerminalStandoffRules default_terminal_standoff_rules{
    .minimum_clearance = 48.0F,
    .hit_radius_padding = 24.0F,
    .arrival_tolerance = 0.01F,
};

[[nodiscard]] std::optional<FrontlineObjective> frontline_objective(
    const World& world, Team team,
    FrontlineRules rules = default_frontline_rules) noexcept;

[[nodiscard]] float autonomous_advance_x(
    const World& world, Team team, Vec2 position,
    FrontlineRules rules = default_frontline_rules) noexcept;

[[nodiscard]] Vec2 constrain_to_frontline(
    const World& world, Team team, Vec2 current_position,
    Vec2 proposed_position,
    FrontlineRules rules = default_frontline_rules) noexcept;

[[nodiscard]] std::optional<TerminalFrontlineStandoff>
terminal_frontline_standoff(
    const World& world, Team team, float unit_hit_radius,
    TerminalStandoffRules rules = default_terminal_standoff_rules) noexcept;

[[nodiscard]] float terminal_standoff_advance_x(
    Vec2 position, const TerminalFrontlineStandoff& standoff,
    TerminalStandoffRules rules = default_terminal_standoff_rules) noexcept;

[[nodiscard]] Vec2 constrain_to_terminal_standoff(
    const World& world, Team team, float unit_hit_radius,
    Vec2 current_position, Vec2 proposed_position,
    TerminalStandoffRules rules = default_terminal_standoff_rules) noexcept;

} // namespace siege
