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
constexpr float battlefield_01_tile_size = 64.0F;

// Regions are painted in order. The grass base covers the complete map; later
// regions add authored lanes and ground features without affecting gameplay.
constexpr std::array battlefield_01_terrain{
    TerrainRegionDefinition{"grass_base", TerrainType::grass,
                            {0.0F, 0.0F, battlefield_01_width,
                             battlefield_01_height},
                            11},
    TerrainRegionDefinition{"north_dirt_lane", TerrainType::dirt,
                            {256.0F, 256.0F, 1'408.0F, 64.0F}, 23},
    TerrainRegionDefinition{"south_dirt_lane", TerrainType::dirt,
                            {256.0F, 768.0F, 1'408.0F, 64.0F}, 37},
    TerrainRegionDefinition{"center_road", TerrainType::asphalt,
                            {896.0F, 0.0F, 128.0F,
                             battlefield_01_height},
                            41},
    TerrainRegionDefinition{"southeast_sand", TerrainType::sand,
                            {1'280.0F, 896.0F, 256.0F, 184.0F}, 53},
    TerrainRegionDefinition{"southeast_water", TerrainType::water,
                            {1'344.0F, 960.0F, 128.0F, 120.0F}, 67},
};

constexpr std::array battlefield_01_environment{
    EnvironmentObjectDefinition{"blue_home_house",
                                EnvironmentObjectType::house,
                                EnvironmentAsset::house_01,
                                {96.0F, 104.0F},
                                {112.0F, 208.0F, 100.0F, 24.0F}},
    EnvironmentObjectDefinition{"red_home_watchtower",
                                EnvironmentObjectType::watchtower,
                                EnvironmentAsset::watchtower_01,
                                {1'736.0F, 112.0F},
                                {1'750.0F, 160.0F, 44.0F, 22.0F}},
    EnvironmentObjectDefinition{"west_tree", EnvironmentObjectType::tree,
                                EnvironmentAsset::tree_02,
                                {320.0F, 856.0F},
                                {350.0F, 928.0F, 45.0F, 24.0F}},
    EnvironmentObjectDefinition{"east_tree", EnvironmentObjectType::tree,
                                EnvironmentAsset::tree_02,
                                {1'488.0F, 104.0F},
                                {1'518.0F, 176.0F, 45.0F, 24.0F}},
    EnvironmentObjectDefinition{"center_rock", EnvironmentObjectType::rock,
                                EnvironmentAsset::rock_02,
                                {824.0F, 520.0F},
                                {836.0F, 562.0F, 30.0F, 18.0F}},
    EnvironmentObjectDefinition{"east_rock", EnvironmentObjectType::rock,
                                EnvironmentAsset::rock_02,
                                {1'392.0F, 840.0F},
                                {1'404.0F, 882.0F, 30.0F, 18.0F}},
    EnvironmentObjectDefinition{"west_sandbags",
                                EnvironmentObjectType::sandbags,
                                EnvironmentAsset::sandbags_01,
                                {584.0F, 432.0F},
                                {592.0F, 442.0F, 69.0F, 13.0F}},
    EnvironmentObjectDefinition{"east_sandbags",
                                EnvironmentObjectType::sandbags,
                                EnvironmentAsset::sandbags_01,
                                {1'240.0F, 616.0F},
                                {1'248.0F, 626.0F, 69.0F, 13.0F}},
    EnvironmentObjectDefinition{"center_crate",
                                EnvironmentObjectType::crate,
                                EnvironmentAsset::crate_02,
                                {928.0F, 648.0F},
                                {933.0F, 668.0F, 20.0F, 11.0F}},
    EnvironmentObjectDefinition{"center_barrel",
                                EnvironmentObjectType::barrel,
                                EnvironmentAsset::barrel_01,
                                {968.0F, 656.0F},
                                {972.0F, 668.0F, 12.0F, 8.0F}},
    EnvironmentObjectDefinition{"west_bush", EnvironmentObjectType::bush,
                                EnvironmentAsset::bush_02,
                                {432.0F, 168.0F},
                                {442.0F, 184.0F, 46.0F, 12.0F}},
    EnvironmentObjectDefinition{"east_bush", EnvironmentObjectType::bush,
                                EnvironmentAsset::bush_02,
                                {1'424.0F, 872.0F},
                                {1'434.0F, 888.0F, 46.0F, 12.0F}},
};

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
    .terrain_tile_size = battlefield_01_tile_size,
    .terrain_regions = battlefield_01_terrain,
    .environment_objects = battlefield_01_environment,
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
