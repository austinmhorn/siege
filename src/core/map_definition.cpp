#include "core/map_definition.hpp"

#include <array>

namespace siege {
namespace {

constexpr std::string_view battlefield_01_id{"battlefield_01"};
constexpr float battlefield_01_width = 1'920.0F;
constexpr float battlefield_01_height = 1'080.0F;
constexpr float battlefield_01_zone_width = 384.0F;

constexpr std::array battlefield_01_zones{
    ZoneDefinition{0, {0.0F, 0.0F, battlefield_01_zone_width,
                       battlefield_01_height},
                   ZoneType::home, Team::team_a},
    ZoneDefinition{1, {384.0F, 0.0F, battlefield_01_zone_width,
                       battlefield_01_height},
                   ZoneType::objective, Team::none},
    ZoneDefinition{2, {768.0F, 0.0F, battlefield_01_zone_width,
                       battlefield_01_height},
                   ZoneType::objective, Team::none},
    ZoneDefinition{3, {1'152.0F, 0.0F, battlefield_01_zone_width,
                       battlefield_01_height},
                   ZoneType::objective, Team::none},
    ZoneDefinition{4, {1'536.0F, 0.0F, battlefield_01_zone_width,
                       battlefield_01_height},
                   ZoneType::home, Team::team_b},
};
constexpr std::array<std::size_t, 3> battlefield_01_objectives{1, 2, 3};
constexpr std::array<std::size_t, 3> battlefield_01_team_a_order{1, 2, 3};
constexpr std::array<std::size_t, 3> battlefield_01_team_b_order{3, 2, 1};

constexpr MapDefinition battlefield_01{
    .id = battlefield_01_id,
    .logical_width = battlefield_01_width,
    .logical_height = battlefield_01_height,
    .zones = battlefield_01_zones,
    .objective_zone_indices = battlefield_01_objectives,
    .center_objective_zone_index = 2,
    .team_a_forward = {
        Team::team_a, 0, 4, battlefield_01_team_a_order, 1.0F},
    .team_b_forward = {
        Team::team_b, 4, 0, battlefield_01_team_b_order, -1.0F},
};

} // namespace

const MapDefinition* map_definition(const std::string_view id) noexcept {
    return id == battlefield_01.id ? &battlefield_01 : nullptr;
}

const MapDefinition& default_map_definition() noexcept {
    return battlefield_01;
}

const TeamForwardDefinition* team_forward_definition(
    const MapDefinition& map, const Team team) noexcept {
    if (team == Team::team_a) {
        return &map.team_a_forward;
    }
    if (team == Team::team_b) {
        return &map.team_b_forward;
    }
    return nullptr;
}

const ZoneDefinition* zone_definition(const MapDefinition& map,
                                      const std::size_t ordered_index) noexcept {
    return ordered_index < map.zones.size() ? &map.zones[ordered_index]
                                            : nullptr;
}

std::optional<std::size_t> map_zone_index_for_position(
    const MapDefinition& map, const Vec2 position) noexcept {
    for (std::size_t index = 0; index < map.zones.size(); ++index) {
        const Bounds& bounds = map.zones[index].bounds;
        if (position.x >= bounds.x && position.x < bounds.x + bounds.width &&
            position.y >= bounds.y && position.y < bounds.y + bounds.height) {
            return index;
        }
    }
    return std::nullopt;
}

} // namespace siege
