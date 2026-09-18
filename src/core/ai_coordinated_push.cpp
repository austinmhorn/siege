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
    return unit.ai_push_staging_active() &&
        unit.mobility_mode() == MobilityMode::autonomous &&
        !unit.has_movement_path() &&
        unit.tactical_order() != TacticalOrder::hold &&
        unit.tactical_order() != TacticalOrder::regroup;
}

} // namespace siege
