#include "core/support_positioning.hpp"

#include <algorithm>

namespace siege {

SupportPositioning support_positioning_for(
    const Unit& unit, const std::vector<Unit>& units) noexcept {
    if (!unit.is_alive() || unit.team() == Team::none ||
        unit.support_positioning_bias() <= 0.0F ||
        unit.support_search_radius() <= 0.0F) {
        return {};
    }

    const Unit* nearest_screen = nullptr;
    float nearest_distance_squared =
        unit.support_search_radius() * unit.support_search_radius();
    for (const auto& candidate : units) {
        if (!candidate.is_alive() || candidate.id() == unit.id() ||
            candidate.team() != unit.team() ||
            candidate.frontline_screen_weight() <= 0.0F) {
            continue;
        }

        const float distance_squared =
            length_squared(candidate.position() - unit.position());
        if (distance_squared > nearest_distance_squared) {
            continue;
        }
        if (nearest_screen == nullptr ||
            distance_squared < nearest_distance_squared ||
            (distance_squared == nearest_distance_squared &&
             candidate.id() < nearest_screen->id())) {
            nearest_screen = &candidate;
            nearest_distance_squared = distance_squared;
        }
    }

    if (nearest_screen == nullptr) {
        return {};
    }

    const float advance_x = unit.team() == Team::team_a ? 1.0F : -1.0F;
    const Vec2 desired_position{
        nearest_screen->position().x -
            advance_x * unit.support_rear_distance(),
        nearest_screen->position().y,
    };
    const Vec2 offset = desired_position - unit.position();
    const float distance = length(offset);
    if (distance <= 0.0001F) {
        return SupportPositioning{nearest_screen->id(), {}};
    }

    const float response_distance =
        std::max(unit.support_rear_distance(), 1.0F);
    const float strength =
        unit.support_positioning_bias() *
        nearest_screen->frontline_screen_weight() *
        std::clamp(distance / response_distance, 0.0F, 1.0F);
    return SupportPositioning{nearest_screen->id(), normalized(offset) * strength};
}

} // namespace siege
