#include "core/deployment.hpp"

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
    const auto bounds = deployment_bounds(world.zones()[*zone_index], team);
    return bounds.has_value() && contains(*bounds, position);
}

DeploymentResult request_deployment(World& world, const Team team,
                                    const TroopType troop_type,
                                    const Vec2 position) {
    const TroopDefinition* definition = troop_definition_for(troop_type);
    if (definition == nullptr || team == Team::none) {
        return DeploymentResult::invalid_troop;
    }
    if (!is_valid_deployment_location(world, team, position)) {
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
    const double elapsed = std::max(0.0, fixed_delta_seconds);
    auto& pending = world.pending_deployments();
    for (auto deployment = pending.begin(); deployment != pending.end();) {
        if (deployment->remaining_seconds <= elapsed + 1.0e-9) {
            world.spawn_unit(deployment->troop_type, deployment->team,
                             deployment->position);
            deployment = pending.erase(deployment);
        } else {
            deployment->remaining_seconds -= elapsed;
            ++deployment;
        }
    }
}

} // namespace siege
