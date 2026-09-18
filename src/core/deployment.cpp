#include "core/deployment.hpp"

#include "core/ai_objective_occupancy.hpp"
#include "core/environment_collision.hpp"
#include "core/troop_definition.hpp"
#include "core/zone_capture.hpp"
#include "world/world.hpp"

#include <algorithm>

namespace siege {
namespace {

bool contains(const Bounds& bounds, const Vec2 position) noexcept {
    return position.x >= bounds.x &&
           position.x < bounds.x + bounds.width &&
           position.y >= bounds.y &&
           position.y < bounds.y + bounds.height;
}

} // namespace

bool is_valid_deployment_location(const World& world, const Team team,
                                  const Vec2 position) noexcept {
    const auto zone_index = zone_index_for_position(world, position);
    if (!zone_index.has_value()) {
        return false;
    }
    const auto bounds =
        deployment_bounds(world, world.zones()[*zone_index], team);
    return bounds.has_value() && contains(*bounds, position);
}

bool is_valid_deployment_location(const World& world, const Team team,
                                  const TroopType troop_type,
                                  const Vec2 position) noexcept {
    const TroopDefinition* definition = troop_definition_for(troop_type);
    return definition != nullptr &&
           is_valid_deployment_location(world, team, position) &&
           !unit_overlaps_blocking_environment(
               world.map(), position, definition->hit_radius);
}

DeploymentResult request_deployment(World& world, const Team team,
                                    const TroopType troop_type,
                                    const Vec2 position) {
    if (!world.match_state().active()) {
        return DeploymentResult::match_finished;
    }
    const TroopDefinition* definition = troop_definition_for(troop_type);
    if (definition == nullptr || team == Team::none) {
        return DeploymentResult::invalid_troop;
    }
    if (!is_valid_deployment_location(world, team, troop_type, position)) {
        return DeploymentResult::invalid_location;
    }

    PlayerState* player = world.find_player(team);
    if (player == nullptr || !player->can_afford(definition->purchase_cost)) {
        return DeploymentResult::insufficient_cash;
    }

    world.queue_deployment(team, troop_type, position,
                           definition->deployment_seconds);
    if (!player->try_spend(definition->purchase_cost)) {
        world.pending_deployments().pop_back();
        return DeploymentResult::insufficient_cash;
    }
    return DeploymentResult::accepted;
}

void update_pending_deployments(World& world,
                                const double fixed_delta_seconds) {
    if (!world.match_state().active()) {
        return;
    }
    const double elapsed = std::max(0.0, fixed_delta_seconds);
    auto& pending = world.pending_deployments();
    for (auto deployment = pending.begin(); deployment != pending.end();) {
        if (deployment->remaining_seconds <= elapsed + 1.0e-9) {
            Unit& unit = world.spawn_unit(deployment->troop_type,
                                          deployment->team,
                                          deployment->position);
            if (deployment->ai_objective_zone.has_value() &&
                *deployment->ai_objective_zone < world.zones().size() &&
                world.zones()[*deployment->ai_objective_zone].owner() ==
                    deployment->team) {
                const std::size_t zone_index = *deployment->ai_objective_zone;
                unit.set_ai_objective_assignment(
                    zone_index,
                    ai_objective_hold_position(
                        world, world.zones()[zone_index], unit.team(),
                        unit.preferred_y(), unit.hit_radius()));
                unit.set_ai_objective_assignment_active(true);
            }
            deployment = pending.erase(deployment);
        } else {
            deployment->remaining_seconds -= elapsed;
            ++deployment;
        }
    }
}

} // namespace siege
