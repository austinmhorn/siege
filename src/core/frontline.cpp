#include "core/frontline.hpp"

#include "world/world.hpp"

#include <algorithm>

namespace siege {

std::optional<FrontlineObjective> frontline_objective(
    const World& world, const Team team, FrontlineRules rules) noexcept {
    if (team == Team::none) {
        return std::nullopt;
    }
    const TeamForwardDefinition* forward =
        team_forward_definition(world.map(), team);
    if (forward == nullptr || forward->objective_order.empty()) {
        return std::nullopt;
    }

    rules.hold_depth_fraction =
        std::clamp(rules.hold_depth_fraction, 0.0F, 1.0F);
    std::size_t frontline_index = forward->objective_order.back();
    for (const std::size_t index : forward->objective_order) {
        if (index >= world.zones().size()) {
            continue;
        }
        if (world.zones()[index].owner() != team) {
            frontline_index = index;
            break;
        }
    }

    const Bounds& bounds = world.zones()[frontline_index].bounds();
    const bool advances_right = forward->x_direction > 0.0F;
    return FrontlineObjective{
        frontline_index,
        advances_right ? bounds.x + bounds.width : bounds.x,
        advances_right
            ? bounds.x + bounds.width * rules.hold_depth_fraction
            : bounds.x + bounds.width * (1.0F - rules.hold_depth_fraction)};
}

float autonomous_advance_x(const World& world, const Team team,
                           const Vec2 position,
                           FrontlineRules rules) noexcept {
    const TeamForwardDefinition* forward =
        team_forward_definition(world.map(), team);
    const float advance = forward == nullptr ? 0.0F : forward->x_direction;
    const auto frontline = frontline_objective(world, team, rules);
    if (!frontline.has_value() || advance == 0.0F) {
        return advance;
    }

    const float distance_to_hold =
        (frontline->hold_x - position.x) * advance;
    if (distance_to_hold > 0.0F) {
        return advance;
    }

    const float return_distance = std::max(1.0F, rules.hold_return_distance);
    return -advance *
           std::clamp(-distance_to_hold / return_distance, 0.0F, 0.5F);
}

Vec2 constrain_to_frontline(const World& world, const Team team,
                            const Vec2 current_position,
                            Vec2 proposed_position,
                            FrontlineRules rules) noexcept {
    const auto frontline = frontline_objective(world, team, rules);
    if (!frontline.has_value()) {
        return proposed_position;
    }

    const float inset = std::max(0.0F, rules.boundary_inset);
    const TeamForwardDefinition* forward =
        team_forward_definition(world.map(), team);
    if (forward == nullptr) {
        return proposed_position;
    }
    if (forward->x_direction > 0.0F) {
        const float limit = frontline->forward_boundary_x - inset;
        proposed_position.x =
            std::min(proposed_position.x, std::max(current_position.x, limit));
    } else if (forward->x_direction < 0.0F) {
        const float limit = frontline->forward_boundary_x + inset;
        proposed_position.x =
            std::max(proposed_position.x, std::min(current_position.x, limit));
    }
    return proposed_position;
}

} // namespace siege
