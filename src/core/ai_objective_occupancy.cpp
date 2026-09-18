#include "core/ai_objective_occupancy.hpp"

#include "core/map_definition.hpp"
#include "world/world.hpp"
#include "world/zone.hpp"

#include <algorithm>

namespace siege {

Vec2 ai_objective_hold_position(const World& world, const Zone& zone,
                                const Team team, const float preferred_y,
                                const float unit_radius) noexcept {
    const Bounds& bounds = zone.bounds();
    const TeamForwardDefinition* forward =
        team_forward_definition(world.map(), team);
    const float rear_fraction = forward != nullptr &&
            forward->x_direction < 0.0F
        ? 0.75F
        : 0.25F;
    const float inset = std::max(ai_objective_hold_inset, unit_radius + 2.0F);
    const float minimum_y = bounds.y + inset;
    const float maximum_y = bounds.y + bounds.height - inset;
    return {
        bounds.x + bounds.width * rear_fraction,
        minimum_y <= maximum_y
            ? std::clamp(preferred_y, minimum_y, maximum_y)
            : bounds.y + bounds.height * 0.5F,
    };
}

bool is_objective_holder(const World& world, const Unit::Id unit_id) noexcept {
    const Unit* unit = world.find_unit(unit_id);
    return unit != nullptr && unit->is_alive() &&
        unit->ai_objective_zone().has_value();
}

bool ai_objective_movement_allowed(const Unit& unit) noexcept {
    return unit.ai_objective_assignment_active() &&
        unit.mobility_mode() == MobilityMode::autonomous &&
        !unit.has_movement_path() &&
        (unit.tactical_order() == TacticalOrder::automatic ||
         unit.tactical_order() == TacticalOrder::advance);
}

} // namespace siege
