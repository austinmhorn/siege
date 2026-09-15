#pragma once

#include "core/math.hpp"
#include "world/zone.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace siege {

struct ZoneDefinition {
    std::size_t id;
    Bounds bounds;
    ZoneType type;
    Team home_team;
};

struct TeamForwardDefinition {
    Team team;
    std::size_t home_zone_index;
    std::size_t opposing_home_zone_index;
    std::span<const std::size_t> objective_order;
    float x_direction;
};

struct MapDefinition {
    std::string_view id;
    float logical_width;
    float logical_height;
    std::span<const ZoneDefinition> zones;
    std::span<const std::size_t> objective_zone_indices;
    std::size_t center_objective_zone_index;
    TeamForwardDefinition team_a_forward;
    TeamForwardDefinition team_b_forward;
};

[[nodiscard]] const MapDefinition* map_definition(
    std::string_view id) noexcept;
[[nodiscard]] const MapDefinition& default_map_definition() noexcept;
[[nodiscard]] const TeamForwardDefinition* team_forward_definition(
    const MapDefinition& map, Team team) noexcept;
[[nodiscard]] const ZoneDefinition* zone_definition(
    const MapDefinition& map, std::size_t ordered_index) noexcept;

// Bounds use inclusive lower and exclusive upper edges. Shared boundaries
// therefore resolve to the later ordered zone exactly once.
[[nodiscard]] std::optional<std::size_t> map_zone_index_for_position(
    const MapDefinition& map, Vec2 position) noexcept;

} // namespace siege
