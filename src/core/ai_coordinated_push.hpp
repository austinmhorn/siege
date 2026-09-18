#pragma once

#include "core/math.hpp"
#include "world/unit.hpp"

#include <optional>

namespace siege {

class World;

struct AiCoordinatedPushRules {
    double medium_cooldown_seconds;
    double hard_cooldown_seconds;
    double staging_timeout_seconds;
    double advance_timeout_seconds;
    float staging_rear_offset_fraction;
    float readiness_radius;
    float readiness_fraction;
    float frontline_min_rear_offset;
    float frontline_max_rear_offset;
    float support_min_rear_offset;
    float support_max_rear_offset;
    float lateral_spacing;
    float formation_dead_zone;
};

inline constexpr AiCoordinatedPushRules default_ai_coordinated_push_rules{
    .medium_cooldown_seconds = 20.0,
    .hard_cooldown_seconds = 12.0,
    .staging_timeout_seconds = 8.0,
    .advance_timeout_seconds = 30.0,
    .staging_rear_offset_fraction = 0.15F,
    .readiness_radius = 120.0F,
    .readiness_fraction = 0.70F,
    .frontline_min_rear_offset = 0.0F,
    .frontline_max_rear_offset = 80.0F,
    .support_min_rear_offset = 120.0F,
    .support_max_rear_offset = 200.0F,
    .lateral_spacing = 80.0F,
    .formation_dead_zone = 55.0F,
};

[[nodiscard]] std::optional<Vec2> ai_push_staging_point(
    const World& world, Team team, float preferred_y,
    AiCoordinatedPushRules rules = default_ai_coordinated_push_rules) noexcept;

[[nodiscard]] bool ai_push_staging_movement_allowed(
    const Unit& unit) noexcept;

[[nodiscard]] Vec2 ai_push_role_position(
    Vec2 anchor_position, float forward_x, AiPushRole role,
    std::size_t role_index, std::size_t role_count,
    AiCoordinatedPushRules rules = default_ai_coordinated_push_rules) noexcept;

} // namespace siege
