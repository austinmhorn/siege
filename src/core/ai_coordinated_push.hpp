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
    float staging_member_spacing;
    float readiness_radius;
    float readiness_fraction;
};

inline constexpr AiCoordinatedPushRules default_ai_coordinated_push_rules{
    .medium_cooldown_seconds = 20.0,
    .hard_cooldown_seconds = 12.0,
    .staging_timeout_seconds = 8.0,
    .advance_timeout_seconds = 30.0,
    .staging_rear_offset_fraction = 0.15F,
    .staging_member_spacing = 36.0F,
    .readiness_radius = 120.0F,
    .readiness_fraction = 0.70F,
};

[[nodiscard]] std::optional<Vec2> ai_push_staging_point(
    const World& world, Team team, float preferred_y,
    AiCoordinatedPushRules rules = default_ai_coordinated_push_rules) noexcept;

[[nodiscard]] bool ai_push_staging_movement_allowed(
    const Unit& unit) noexcept;

} // namespace siege
