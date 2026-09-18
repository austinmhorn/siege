#include "core/ai_coordinated_push.hpp"

#include "core/frontline.hpp"
#include "core/map_definition.hpp"
#include "world/world.hpp"

#include <algorithm>

namespace siege {

std::optional<Vec2> ai_push_staging_point(
    const World& world, const Team team, const float preferred_y,
    AiCoordinatedPushRules rules) noexcept {
    const auto frontline = frontline_objective(world, team);
    const TeamForwardDefinition* forward =
        team_forward_definition(world.map(), team);
    if (!frontline.has_value() || forward == nullptr ||
        frontline->zone_index >= world.zones().size() ||
        world.zones()[frontline->zone_index].owner() == team) {
        return std::nullopt;
    }

    rules.staging_rear_offset_fraction = std::max(
        0.0F, rules.staging_rear_offset_fraction);
    const Bounds& bounds = world.zones()[frontline->zone_index].bounds();
    const float rear_boundary = forward->x_direction > 0.0F
        ? bounds.x
        : bounds.x + bounds.width;
    const float x = rear_boundary - forward->x_direction *
        (bounds.width * rules.staging_rear_offset_fraction);
    constexpr float world_inset = 32.0F;
    return Vec2{
        std::clamp(x, world_inset,
                   world.map().logical_width - world_inset),
        std::clamp(preferred_y, world_inset,
                   world.map().logical_height - world_inset),
    };
}

bool ai_push_staging_movement_allowed(const Unit& unit) noexcept {
    return unit.ai_push_id().has_value() && unit.ai_push_role().has_value() &&
        unit.mobility_mode() == MobilityMode::autonomous &&
        !unit.has_movement_path() &&
        unit.tactical_order() != TacticalOrder::hold &&
        unit.tactical_order() != TacticalOrder::regroup;
}

Vec2 ai_push_role_position(
    const Vec2 anchor_position, const float forward_x,
    const AiPushRole role, const std::size_t role_index,
    const std::size_t role_count, AiCoordinatedPushRules rules) noexcept {
    const auto distributed = [role_index, role_count](const float minimum,
                                                       const float maximum) {
        if (role_count <= 1) {
            return (minimum + maximum) * 0.5F;
        }
        return minimum + (maximum - minimum) *
            static_cast<float>(role_index) /
            static_cast<float>(role_count - 1);
    };
    float rear_offset = 0.0F;
    if (role == AiPushRole::frontline) {
        rear_offset = distributed(rules.frontline_min_rear_offset,
                                  rules.frontline_max_rear_offset);
    } else if (role == AiPushRole::support) {
        rear_offset = distributed(rules.support_min_rear_offset,
                                  rules.support_max_rear_offset);
    }
    const float lateral_index = static_cast<float>(role_index) -
        (static_cast<float>(role_count) - 1.0F) * 0.5F;
    return {
        anchor_position.x - forward_x * rear_offset,
        anchor_position.y + lateral_index * rules.lateral_spacing,
    };
}

} // namespace siege
