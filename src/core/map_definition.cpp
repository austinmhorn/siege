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
constexpr EnvironmentPhysicalProperties opaque_blocker{true, true, true};
constexpr EnvironmentPhysicalProperties low_blocker{true, true, false};
constexpr EnvironmentPhysicalProperties non_blocker{false, false, false};

// Regions are painted in order. A broad east/west supply road joins both home
// areas, while two quieter dirt lanes leave the objective space open and make
// the battlefield read as an intentionally used military position.
constexpr std::array battlefield_01_terrain{
    TerrainRegionDefinition{"grass_base", TerrainType::grass,
                            {0.0F, 0.0F, battlefield_01_width,
                             battlefield_01_height},
                            11},
    TerrainRegionDefinition{"north_dirt_lane", TerrainType::dirt,
                            {192.0F, 224.0F, 1'536.0F, 128.0F}, 23},
    TerrainRegionDefinition{"south_dirt_lane", TerrainType::dirt,
                            {192.0F, 704.0F, 1'536.0F, 128.0F}, 37},
    TerrainRegionDefinition{"home_supply_road", TerrainType::asphalt,
                            {0.0F, 480.0F, battlefield_01_width, 128.0F}, 41},
    TerrainRegionDefinition{"center_worn_crossing", TerrainType::dirt,
                            {384.0F, 480.0F, 1'152.0F, 128.0F}, 47},
    TerrainRegionDefinition{"south_drainage_bank", TerrainType::sand,
                            {832.0F, 896.0F, 256.0F, 184.0F}, 53},
    TerrainRegionDefinition{"south_drainage_pond", TerrainType::water,
                            {896.0F, 960.0F, 128.0F, 120.0F}, 67},
};

constexpr std::array<EnvironmentObjectDefinition, 24>
    battlefield_01_environment{{
    {"blue_home_house", EnvironmentObjectType::house,
     EnvironmentAsset::house_01, {48.0F, 72.0F},
     {62.0F, 152.0F, 104.0F, 44.0F},
     EnvironmentOrientation::team_a_forward, opaque_blocker},
    {"red_home_house", EnvironmentObjectType::house,
     EnvironmentAsset::house_01, {1'740.0F, 72.0F},
     {1'754.0F, 152.0F, 104.0F, 44.0F},
     EnvironmentOrientation::team_b_forward, opaque_blocker},
    {"blue_home_watchtower", EnvironmentObjectType::watchtower,
     EnvironmentAsset::watchtower_01, {192.0F, 912.0F},
     {214.0F, 934.0F, 28.0F, 28.0F},
     EnvironmentOrientation::team_a_forward, opaque_blocker},
    {"red_home_watchtower", EnvironmentObjectType::watchtower,
     EnvironmentAsset::watchtower_01, {1'656.0F, 912.0F},
     {1'678.0F, 934.0F, 28.0F, 28.0F},
     EnvironmentOrientation::team_b_forward, opaque_blocker},

    // Mirrored defensive lines sit behind the outer objectives and leave
    // generous north, center, and south routes around them.
    {"blue_north_sandbags", EnvironmentObjectType::sandbags,
     EnvironmentAsset::sandbags_01, {500.0F, 216.0F},
     {536.0F, 193.0F, 13.0F, 69.0F},
     EnvironmentOrientation::team_a_forward, low_blocker},
    {"blue_center_sandbags", EnvironmentObjectType::sandbags,
     EnvironmentAsset::sandbags_01, {500.0F, 518.0F},
     {536.0F, 495.0F, 13.0F, 69.0F},
     EnvironmentOrientation::team_a_forward, low_blocker},
    {"blue_south_sandbags", EnvironmentObjectType::sandbags,
     EnvironmentAsset::sandbags_01, {500.0F, 790.0F},
     {536.0F, 767.0F, 13.0F, 69.0F},
     EnvironmentOrientation::team_a_forward, low_blocker},
    {"red_north_sandbags", EnvironmentObjectType::sandbags,
     EnvironmentAsset::sandbags_01, {1'335.0F, 216.0F},
     {1'371.0F, 193.0F, 13.0F, 69.0F},
     EnvironmentOrientation::team_b_forward, low_blocker},
    {"red_center_sandbags", EnvironmentObjectType::sandbags,
     EnvironmentAsset::sandbags_01, {1'335.0F, 518.0F},
     {1'371.0F, 495.0F, 13.0F, 69.0F},
     EnvironmentOrientation::team_b_forward, low_blocker},
    {"red_south_sandbags", EnvironmentObjectType::sandbags,
     EnvironmentAsset::sandbags_01, {1'335.0F, 790.0F},
     {1'371.0F, 767.0F, 13.0F, 69.0F},
     EnvironmentOrientation::team_b_forward, low_blocker},

    {"blue_north_tree", EnvironmentObjectType::tree,
     EnvironmentAsset::tree_02, {296.0F, 48.0F},
     {326.0F, 120.0F, 45.0F, 24.0F},
     EnvironmentOrientation::neutral, opaque_blocker},
    {"blue_south_tree", EnvironmentObjectType::tree,
     EnvironmentAsset::tree_02, {304.0F, 856.0F},
     {334.0F, 928.0F, 45.0F, 24.0F},
     EnvironmentOrientation::neutral, opaque_blocker},
    {"red_north_tree", EnvironmentObjectType::tree,
     EnvironmentAsset::tree_02, {1'520.0F, 48.0F},
     {1'550.0F, 120.0F, 45.0F, 24.0F},
     EnvironmentOrientation::neutral, opaque_blocker},
    {"red_south_tree", EnvironmentObjectType::tree,
     EnvironmentAsset::tree_02, {1'512.0F, 856.0F},
     {1'542.0F, 928.0F, 45.0F, 24.0F},
     EnvironmentOrientation::neutral, opaque_blocker},

    {"blue_north_rock", EnvironmentObjectType::rock,
     EnvironmentAsset::rock_02, {680.0F, 112.0F},
     {692.0F, 154.0F, 30.0F, 18.0F},
     EnvironmentOrientation::neutral, opaque_blocker},
    {"blue_south_rock", EnvironmentObjectType::rock,
     EnvironmentAsset::rock_02, {680.0F, 872.0F},
     {692.0F, 914.0F, 30.0F, 18.0F},
     EnvironmentOrientation::neutral, opaque_blocker},
    {"red_north_rock", EnvironmentObjectType::rock,
     EnvironmentAsset::rock_02, {1'192.0F, 112.0F},
     {1'204.0F, 154.0F, 30.0F, 18.0F},
     EnvironmentOrientation::neutral, opaque_blocker},
    {"red_south_rock", EnvironmentObjectType::rock,
     EnvironmentAsset::rock_02, {1'192.0F, 872.0F},
     {1'204.0F, 914.0F, 30.0F, 18.0F},
     EnvironmentOrientation::neutral, opaque_blocker},

    {"blue_supply_crate", EnvironmentObjectType::crate,
     EnvironmentAsset::crate_02, {192.0F, 144.0F},
     {197.0F, 164.0F, 20.0F, 11.0F},
     EnvironmentOrientation::team_a_forward, low_blocker},
    {"blue_supply_barrel", EnvironmentObjectType::barrel,
     EnvironmentAsset::barrel_01, {230.0F, 154.0F},
     {234.0F, 166.0F, 12.0F, 8.0F},
     EnvironmentOrientation::neutral, low_blocker},
    {"red_supply_crate", EnvironmentObjectType::crate,
     EnvironmentAsset::crate_02, {1'698.0F, 144.0F},
     {1'703.0F, 164.0F, 20.0F, 11.0F},
     EnvironmentOrientation::team_b_forward, low_blocker},
    {"red_supply_barrel", EnvironmentObjectType::barrel,
     EnvironmentAsset::barrel_01, {1'670.0F, 154.0F},
     {1'674.0F, 166.0F, 12.0F, 8.0F},
     EnvironmentOrientation::neutral, low_blocker},

    // Bushes soften the lane transitions but deliberately remain passable.
    {"blue_lane_bush", EnvironmentObjectType::bush,
     EnvironmentAsset::bush_02, {632.0F, 382.0F},
     {642.0F, 398.0F, 46.0F, 12.0F},
     EnvironmentOrientation::neutral, non_blocker},
    {"red_lane_bush", EnvironmentObjectType::bush,
     EnvironmentAsset::bush_02, {1'222.0F, 670.0F},
     {1'232.0F, 686.0F, 46.0F, 12.0F},
     EnvironmentOrientation::neutral, non_blocker},
}};

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

float team_forward_facing_angle(const MapDefinition& map,
                                const Team team) noexcept {
    const TeamForwardDefinition* forward =
        team_forward_definition(map, team);
    if (forward == nullptr) {
        return 0.0F;
    }
    return facing_from_direction(Vec2{forward->x_direction, 0.0F});
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

bool validate_environment_objects(const MapDefinition& map) noexcept {
    for (std::size_t index = 0; index < map.environment_objects.size(); ++index) {
        const EnvironmentObjectDefinition& object =
            map.environment_objects[index];
        const Bounds& footprint = object.footprint;
        if (object.id.empty() || footprint.width <= 0.0F ||
            footprint.height <= 0.0F || footprint.x < 0.0F ||
            footprint.y < 0.0F ||
            footprint.x + footprint.width > map.logical_width ||
            footprint.y + footprint.height > map.logical_height) {
            return false;
        }
        for (std::size_t other = index + 1;
             other < map.environment_objects.size(); ++other) {
            if (object.id == map.environment_objects[other].id) {
                return false;
            }
        }
    }
    return true;
}

} // namespace siege
