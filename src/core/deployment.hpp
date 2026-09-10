#pragma once

#include "core/math.hpp"
#include "world/unit.hpp"

namespace siege {

class World;

enum class DeploymentResult {
    accepted,
    invalid_troop,
    invalid_location,
    insufficient_cash,
};

[[nodiscard]] bool is_valid_deployment_location(
    const World& world, Team team, Vec2 position) noexcept;

[[nodiscard]] DeploymentResult request_deployment(
    World& world, Team team, TroopType troop_type, Vec2 position);

void update_pending_deployments(World& world, double fixed_delta_seconds);

} // namespace siege
