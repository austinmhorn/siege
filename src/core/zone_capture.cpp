#include "core/zone_capture.hpp"

#include "world/world.hpp"

#include <algorithm>

namespace siege {

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
                         CaptureRules rules) noexcept {
    rules.capture_rate = std::max(0.0F, rules.capture_rate);
    rules.maximum_effective_pressure =
        std::max(0, rules.maximum_effective_pressure);

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
    }
}

} // namespace siege
