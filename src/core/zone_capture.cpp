#include "core/zone_capture.hpp"

#include "world/world.hpp"

#include <algorithm>

namespace siege {
namespace {

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
    for (const auto& zone : world.zones()) {
        const Bounds& bounds = zone.bounds();
        if (position.x >= bounds.x && position.x < bounds.x + bounds.width &&
            position.y >= bounds.y && position.y < bounds.y + bounds.height) {
            return zone.index();
        }
    }
    return std::nullopt;
}

void update_zone_capture(World& world, const double fixed_delta_seconds,
                         CaptureRules rules,
                         ZoneSecurityRules security_rules) noexcept {
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
    for (auto& zone : world.zones()) {
        if (zone.type() != ZoneType::objective) {
            continue;
        }
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

std::optional<Bounds> deployment_bounds(
    const Zone& zone, const Team team, ZoneSecurityRules rules) noexcept {
    if (!is_zone_deployable(zone, team)) {
        return std::nullopt;
    }
    if (zone.type() == ZoneType::home) {
        return zone.bounds();
    }

    const Bounds& bounds = zone.bounds();
    const float rear_fraction =
        std::clamp(rules.deployment_rear_fraction, 0.0F, 1.0F);
    const float deployment_width = bounds.width * rear_fraction;
    const float deployment_x = team == Team::team_a
        ? bounds.x
        : bounds.x + bounds.width - deployment_width;
    return Bounds{deployment_x, bounds.y, deployment_width, bounds.height};
}

} // namespace siege
