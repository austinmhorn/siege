#include "core/zone_capture.hpp"

#include "world/world.hpp"

#include <algorithm>

namespace siege {
namespace {

std::optional<std::size_t> deployment_front_index(
    const World& world, const Team team) noexcept {
    if (team == Team::none) {
        return std::nullopt;
    }
    const TeamForwardDefinition* forward =
        team_forward_definition(world.map(), team);
    if (forward == nullptr || forward->home_zone_index >= world.zones().size()) {
        return std::nullopt;
    }

    std::size_t front = forward->home_zone_index;
    if (world.match_state().phase() == MatchPhase::sudden_death) {
        return front;
    }
    for (const std::size_t index : forward->objective_order) {
        if (index >= world.zones().size()) {
            break;
        }
        if (!is_zone_deployable(world.zones()[index], team)) {
            break;
        }
        front = index;
    }
    return front;
}

void transition_ownership(World& world, Zone& zone) {
    const Team previous_owner = zone.owner();

    if (previous_owner == Team::team_a && zone.capture_value() <= 0.0F) {
        zone.set_owner(Team::none);
        world.emit_zone_ownership_event(zone.index(), Team::team_a, Team::none,
                                        ZoneTransitionType::neutralized);
    } else if (previous_owner == Team::team_b &&
               zone.capture_value() >= 0.0F) {
        zone.set_owner(Team::none);
        world.emit_zone_ownership_event(zone.index(), Team::team_b, Team::none,
                                        ZoneTransitionType::neutralized);
    }

    if (zone.owner() == Team::none && zone.capture_value() >= 100.0F) {
        zone.set_owner(Team::team_a);
        world.emit_zone_ownership_event(zone.index(), Team::none, Team::team_a,
                                        ZoneTransitionType::captured);
    } else if (zone.owner() == Team::none &&
               zone.capture_value() <= -100.0F) {
        zone.set_owner(Team::team_b);
        world.emit_zone_ownership_event(zone.index(), Team::none, Team::team_b,
                                        ZoneTransitionType::captured);
    }
}

} // namespace

std::optional<std::size_t> zone_index_for_position(
    const World& world, const Vec2 position) noexcept {
    return map_zone_index_for_position(world.map(), position);
}

void update_zone_capture(World& world, const double fixed_delta_seconds,
                         CaptureRules rules,
                         ZoneSecurityRules security_rules) noexcept {
    if (!world.match_state().active()) {
        return;
    }
    rules.capture_rate = std::max(0.0F, rules.capture_rate);
    rules.maximum_effective_pressure =
        std::max(0, rules.maximum_effective_pressure);
    security_rules.secure_duration_seconds =
        std::max(0.0, security_rules.secure_duration_seconds);

    for (auto& zone : world.zones()) {
        zone.clear_presence();
    }

    for (const auto& unit : world.units()) {
        if (!unit.is_alive() || unit.team() == Team::none) {
            continue;
        }
        const auto zone_index = zone_index_for_position(world, unit.position());
        if (zone_index.has_value() &&
            world.zones()[*zone_index].type() == ZoneType::objective) {
            world.zones()[*zone_index].add_presence(unit.team());
        }
    }

    const float delta_seconds =
        static_cast<float>(std::max(0.0, fixed_delta_seconds));
    for (const std::size_t index : world.map().objective_zone_indices) {
        if (index >= world.zones().size()) {
            continue;
        }
        Zone& zone = world.zones()[index];
        const int effective_pressure = std::clamp(
            zone.pressure(), -rules.maximum_effective_pressure,
            rules.maximum_effective_pressure);
        zone.advance_capture(static_cast<float>(effective_pressure) *
                             rules.capture_rate * delta_seconds);
        transition_ownership(world, zone);
        zone.update_security(delta_seconds,
                             security_rules.secure_duration_seconds);
    }
}

bool is_zone_deployable(const Zone& zone, const Team team) noexcept {
    if (team == Team::none || zone.owner() != team) {
        return false;
    }
    return zone.type() == ZoneType::home || zone.secured();
}

bool is_zone_deployable(const World& world, const Zone& zone,
                        const Team team) noexcept {
    if (!is_zone_deployable(zone, team)) {
        return false;
    }
    const auto front = deployment_front_index(world, team);
    if (!front.has_value()) {
        return false;
    }
    const TeamForwardDefinition* forward =
        team_forward_definition(world.map(), team);
    if (forward == nullptr) {
        return false;
    }
    if (zone.index() == forward->home_zone_index) {
        return true;
    }
    if (*front == forward->home_zone_index) {
        return false;
    }
    for (const std::size_t index : forward->objective_order) {
        if (index == zone.index()) {
            return true;
        }
        if (index == *front) {
            break;
        }
    }
    return false;
}

std::optional<Bounds> deployment_bounds(
    const Zone& zone, const Team team, ZoneSecurityRules rules) noexcept {
    if (!is_zone_deployable(zone, team)) {
        return std::nullopt;
    }
    const Bounds& bounds = zone.bounds();
    const float rear_fraction =
        std::clamp(rules.deployment_rear_fraction, 0.0F, 1.0F);
    const float deployment_width = bounds.width * rear_fraction;
    const TeamForwardDefinition* forward =
        team_forward_definition(default_map_definition(), team);
    const float deployment_x = forward != nullptr && forward->x_direction > 0.0F
        ? bounds.x
        : bounds.x + bounds.width - deployment_width;
    return Bounds{deployment_x, bounds.y, deployment_width, bounds.height};
}

std::optional<Bounds> deployment_bounds(
    const World& world, const Zone& zone, const Team team,
    const ZoneSecurityRules rules) noexcept {
    if (!is_zone_deployable(world, zone, team)) {
        return std::nullopt;
    }
    const auto front = deployment_front_index(world, team);
    if (!front.has_value()) {
        return std::nullopt;
    }
    if (zone.index() != *front) {
        return zone.bounds();
    }
    const Bounds& bounds = zone.bounds();
    const float rear_fraction =
        std::clamp(rules.deployment_rear_fraction, 0.0F, 1.0F);
    const float deployment_width = bounds.width * rear_fraction;
    const TeamForwardDefinition* forward =
        team_forward_definition(world.map(), team);
    if (forward == nullptr) {
        return std::nullopt;
    }
    const float deployment_x = forward->x_direction > 0.0F
        ? bounds.x
        : bounds.x + bounds.width - deployment_width;
    return Bounds{deployment_x, bounds.y, deployment_width, bounds.height};
}

} // namespace siege
