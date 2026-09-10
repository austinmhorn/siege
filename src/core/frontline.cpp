#include "core/frontline.hpp"

#include "world/world.hpp"

#include <algorithm>

namespace siege {

std::optional<FrontlineObjective> frontline_objective(
    const World& world, const Team team, FrontlineRules rules) noexcept {
    if (team == Team::none) {
        return std::nullopt;
    }

    rules.hold_depth_fraction =
        std::clamp(rules.hold_depth_fraction, 0.0F, 1.0F);
    if (team == Team::team_a) {
        for (std::size_t index = 1; index + 1 < world.zones().size(); ++index) {
            const Zone& zone = world.zones()[index];
            if (zone.owner() == team) {
                continue;
            }
            const Bounds& bounds = zone.bounds();
            return FrontlineObjective{
                index, bounds.x + bounds.width,
                bounds.x + bounds.width * rules.hold_depth_fraction};
        }
    } else {
        for (std::size_t index = world.zones().size() - 2; index > 0; --index) {
            const Zone& zone = world.zones()[index];
            if (zone.owner() == team) {
                continue;
            }
            const Bounds& bounds = zone.bounds();
            return FrontlineObjective{
                index, bounds.x,
                bounds.x + bounds.width *
                               (1.0F - rules.hold_depth_fraction)};
        }
    }
    return std::nullopt;
}

float autonomous_advance_x(const World& world, const Team team,
                           const Vec2 position,
                           FrontlineRules rules) noexcept {
    const float advance = team == Team::team_a
        ? 1.0F
        : team == Team::team_b ? -1.0F : 0.0F;
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
    if (team == Team::team_a) {
        const float limit = frontline->forward_boundary_x - inset;
        proposed_position.x =
            std::min(proposed_position.x, std::max(current_position.x, limit));
    } else if (team == Team::team_b) {
        const float limit = frontline->forward_boundary_x + inset;
        proposed_position.x =
            std::max(proposed_position.x, std::min(current_position.x, limit));
    }
    return proposed_position;
}

} // namespace siege
