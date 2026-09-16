#include "client/battlefield_renderer.hpp"

#include "client/texture_cache.hpp"
#include "client/world_transform.hpp"
#include "core/frontline.hpp"
#include "world/world.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <string_view>
#include <system_error>
#include <vector>

namespace siege {
namespace {

struct Color {
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    Uint8 alpha;
};

struct BattlefieldTheme {
    std::array<Color, 5> grass;
    Color grass_detail;
    Color dirt;
    Color dirt_detail;
    Color zone_boundary;
    Color home_team_a;
    Color home_team_b;
    Color objective_team_a;
    Color objective_team_b;
    Color frontline_team_a;
    Color frontline_team_b;
    float terrain_tile_size;
    float home_edge_width;
    float objective_edge_width;
    float frontline_band_width;
    float frontline_dash_length;
    float frontline_dash_gap;
};

constexpr BattlefieldTheme battlefield_theme{
    .grass = {{{61, 82, 53, 255},
               {64, 85, 55, 255},
               {59, 80, 52, 255},
               {66, 86, 56, 255},
               {62, 84, 54, 255}}},
    .grass_detail = {39, 61, 39, 70},
    .dirt = {112, 91, 58, 34},
    .dirt_detail = {85, 70, 48, 54},
    .zone_boundary = {198, 211, 187, 78},
    .home_team_a = {74, 158, 224, 185},
    .home_team_b = {220, 91, 82, 185},
    .objective_team_a = {74, 158, 224, 128},
    .objective_team_b = {220, 91, 82, 128},
    .frontline_team_a = {102, 194, 255, 170},
    .frontline_team_b = {255, 121, 108, 170},
    .terrain_tile_size = 48.0F,
    .home_edge_width = 12.0F,
    .objective_edge_width = 4.0F,
    .frontline_band_width = 10.0F,
    .frontline_dash_length = 30.0F,
    .frontline_dash_gap = 18.0F,
};

void set_color(SDL_Renderer* renderer, const Color color) noexcept {
    SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue,
                           color.alpha);
}

std::uint32_t terrain_hash(const std::uint32_t x,
                           const std::uint32_t y) noexcept {
    std::uint32_t value = x * 0x9e3779b9U ^ y * 0x85ebca6bU ^ 0x51ed270bU;
    value ^= value >> 16U;
    value *= 0x7feb352dU;
    value ^= value >> 15U;
    return value;
}

std::filesystem::path terrain_asset_path(const TerrainType terrain) {
    std::string_view name;
    switch (terrain) {
    case TerrainType::grass:
        name = "grass.png";
        break;
    case TerrainType::dirt:
        name = "dirt.png";
        break;
    case TerrainType::asphalt:
        name = "asphalt.png";
        break;
    case TerrainType::sand:
        name = "sand.png";
        break;
    case TerrainType::water:
        name = "water.png";
        break;
    }
    return std::filesystem::path{"terrain/battlefield/tiles"} / name;
}

struct EnvironmentVisualAsset {
    EnvironmentAsset asset;
    std::string_view image;
    std::string_view shadow;
};

constexpr std::array environment_visual_assets{
    EnvironmentVisualAsset{EnvironmentAsset::house_01, "house_01.png",
                           "house_01.png"},
    EnvironmentVisualAsset{EnvironmentAsset::rock_02, "rock_02.png",
                           "rock_02.png"},
    EnvironmentVisualAsset{EnvironmentAsset::sandbags_01, "sandbags_01.png",
                           "sandbags_01.png"},
    EnvironmentVisualAsset{EnvironmentAsset::tree_02, "tree_02.png",
                           "tree_02.png"},
    EnvironmentVisualAsset{EnvironmentAsset::bush_02, "bush_02.png",
                           "bush_02.png"},
    EnvironmentVisualAsset{EnvironmentAsset::watchtower_01,
                           "watchtower_01.png", "watchtower_01.png"},
    EnvironmentVisualAsset{EnvironmentAsset::crate_02, "crate_02.png",
                           "crate_02.png"},
    EnvironmentVisualAsset{EnvironmentAsset::barrel_01, "barrel_01.png",
                           "barrel_01.png"},
};

const EnvironmentVisualAsset* environment_visual(
    const EnvironmentAsset asset) noexcept {
    for (const auto& visual : environment_visual_assets) {
        if (visual.asset == asset) {
            return &visual;
        }
    }
    return nullptr;
}

std::filesystem::path environment_asset_path(
    const EnvironmentAsset asset, const bool shadow) {
    const EnvironmentVisualAsset* visual = environment_visual(asset);
    if (visual == nullptr) {
        return {};
    }
    return std::filesystem::path{shadow ? "terrain/battlefield/shadows"
                                       : "terrain/battlefield/objects"} /
           (shadow ? visual->shadow : visual->image);
}

SDL_FRect drawable_rect(const WorldTransform& transform,
                        const Bounds bounds) noexcept {
    const Bounds transformed = transform.world_to_drawable(bounds);
    const float left = std::round(transformed.x);
    const float top = std::round(transformed.y);
    const float right = std::round(transformed.x + transformed.width);
    const float bottom = std::round(transformed.y + transformed.height);
    return SDL_FRect{left, top, std::max(1.0F, right - left),
                     std::max(1.0F, bottom - top)};
}

bool fill_world_rect(SDL_Renderer* renderer, const WorldTransform& transform,
                     const Bounds bounds, const Color color) noexcept {
    const SDL_FRect rectangle = drawable_rect(transform, bounds);
    set_color(renderer, color);
    return SDL_RenderFillRect(renderer, &rectangle);
}

bool render_procedural_terrain(SDL_Renderer* renderer, const World& world,
                               const WorldTransform& transform) noexcept {
    const MapDefinition& map = world.map();
    const float tile = battlefield_theme.terrain_tile_size;
    const std::uint32_t columns =
        static_cast<std::uint32_t>(std::ceil(map.logical_width / tile));
    const std::uint32_t rows =
        static_cast<std::uint32_t>(std::ceil(map.logical_height / tile));

    for (std::uint32_t row = 0; row < rows; ++row) {
        for (std::uint32_t column = 0; column < columns; ++column) {
            const std::uint32_t hash = terrain_hash(column, row);
            const float x = static_cast<float>(column) * tile;
            const float y = static_cast<float>(row) * tile;
            const Bounds tile_bounds{
                x, y, std::min(tile, map.logical_width - x),
                std::min(tile, map.logical_height - y)};
            const Color grass =
                battlefield_theme.grass[hash % battlefield_theme.grass.size()];
            if (!fill_world_rect(renderer, transform, tile_bounds, grass)) {
                return false;
            }

            // Sparse blocky wear breaks up the field without introducing
            // texture filtering or asset dependencies.
            if (((hash >> 8U) % 11U) == 0U) {
                const float patch_x = x + 7.0F + static_cast<float>((hash >> 16U) % 19U);
                const float patch_y = y + 8.0F + static_cast<float>((hash >> 21U) % 17U);
                if (!fill_world_rect(renderer, transform,
                                     Bounds{patch_x, patch_y, 22.0F, 12.0F},
                                     battlefield_theme.dirt)) {
                    return false;
                }
            } else if (((hash >> 10U) % 7U) == 0U) {
                const auto start = transform.world_to_drawable(
                    Point{x + 10.0F, y + 12.0F + static_cast<float>(hash % 18U)});
                const auto end = transform.world_to_drawable(
                    Point{x + 18.0F, y + 12.0F + static_cast<float>(hash % 18U)});
                set_color(renderer, battlefield_theme.grass_detail);
                if (!SDL_RenderLine(renderer, std::round(start.x),
                                    std::round(start.y), std::round(end.x),
                                    std::round(end.y))) {
                    return false;
                }
            }
        }
    }

    // Two irregular, broken lanes suggest long-term troop traffic while
    // remaining much quieter than units, paths, and projectiles.
    constexpr std::array<float, 2> worn_lanes{300.0F, 780.0F};
    for (std::uint32_t column = 0; column < columns; ++column) {
        const float x = static_cast<float>(column) * tile;
        for (std::uint32_t lane = 0; lane < worn_lanes.size(); ++lane) {
            const std::uint32_t hash = terrain_hash(column + 83U, lane + 41U);
            if ((hash % 5U) == 0U) {
                continue;
            }
            const float offset = static_cast<float>(static_cast<int>((hash >> 7U) % 25U) - 12);
            const float height = 18.0F + static_cast<float>((hash >> 15U) % 18U);
            if (!fill_world_rect(
                    renderer, transform,
                    Bounds{x, worn_lanes[lane] + offset - height * 0.5F,
                           std::min(tile, map.logical_width - x), height},
                    battlefield_theme.dirt)) {
                return false;
            }
            if ((hash % 3U) == 0U &&
                !fill_world_rect(renderer, transform,
                                 Bounds{x + 9.0F, worn_lanes[lane] + offset,
                                        15.0F, 3.0F},
                                 battlefield_theme.dirt_detail)) {
                return false;
            }
        }
    }
    return true;
}

bool render_authored_terrain(SDL_Renderer* renderer, TextureCache& textures,
                             const MapDefinition& map,
                             const WorldTransform& transform) {
    const float tile_size = std::max(1.0F, map.terrain_tile_size);
    for (const TerrainRegionDefinition& region : map.terrain_regions) {
        SDL_Texture* texture = textures.get(terrain_asset_path(region.terrain));
        if (texture == nullptr) {
            return false;
        }

        std::uint32_t row = 0;
        for (float y = region.bounds.y;
             y < region.bounds.y + region.bounds.height; y += tile_size, ++row) {
            std::uint32_t column = 0;
            for (float x = region.bounds.x;
                 x < region.bounds.x + region.bounds.width;
                 x += tile_size, ++column) {
                const float world_width = std::min(
                    tile_size, region.bounds.x + region.bounds.width - x);
                const float world_height = std::min(
                    tile_size, region.bounds.y + region.bounds.height - y);
                const Bounds drawable = transform.world_to_drawable(
                    Bounds{x, y, world_width, world_height});
                const SDL_FRect destination{drawable.x, drawable.y,
                                            drawable.width, drawable.height};
                const SDL_FRect source{
                    0.0F, 0.0F, 64.0F * world_width / tile_size,
                    64.0F * world_height / tile_size};

                const std::uint32_t hash = terrain_hash(
                    column + region.variant_seed, row + region.variant_seed * 3U);
                const bool complete_tile = world_width == tile_size &&
                                           world_height == tile_size;
                const double angle = complete_tile
                    ? static_cast<double>((hash & 3U) * 90U)
                    : 0.0;
                const SDL_FlipMode flip = complete_tile && (hash & 4U) != 0U
                    ? SDL_FLIP_HORIZONTAL
                    : SDL_FLIP_NONE;
                if (!SDL_RenderTextureRotated(renderer, texture, &source,
                                              &destination, angle, nullptr,
                                              flip)) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool render_environment_texture(SDL_Renderer* renderer, TextureCache& textures,
                                const EnvironmentObjectDefinition& object,
                                const MapDefinition& map,
                                const WorldTransform& transform,
                                const bool shadow) {
    const std::filesystem::path path =
        environment_asset_path(object.asset, shadow);
    SDL_Texture* texture = textures.get(path);
    if (texture == nullptr) {
        return false;
    }
    float width = 0.0F;
    float height = 0.0F;
    if (!SDL_GetTextureSize(texture, &width, &height)) {
        return false;
    }
    const Bounds drawable = transform.world_to_drawable(
        Bounds{object.position.x, object.position.y, width, height});
    const SDL_FRect destination{drawable.x, drawable.y, drawable.width,
                                drawable.height};
    float angle = 0.0F;
    switch (object.orientation) {
    case EnvironmentOrientation::neutral:
        break;
    case EnvironmentOrientation::team_a_forward:
        angle = team_forward_facing_angle(map, Team::team_a);
        break;
    case EnvironmentOrientation::team_b_forward:
        angle = team_forward_facing_angle(map, Team::team_b);
        break;
    }
    return SDL_RenderTextureRotated(renderer, texture, nullptr, &destination,
                                    angle, nullptr, SDL_FLIP_NONE);
}

bool render_environment(SDL_Renderer* renderer, TextureCache& textures,
                        const MapDefinition& map,
                        const WorldTransform& transform) {
    std::vector<const EnvironmentObjectDefinition*> ordered;
    ordered.reserve(map.environment_objects.size());
    for (const auto& object : map.environment_objects) {
        ordered.push_back(&object);
    }
    std::stable_sort(ordered.begin(), ordered.end(), [](const auto* left,
                                                        const auto* right) {
        const float left_y = left->footprint.y + left->footprint.height;
        const float right_y = right->footprint.y + right->footprint.height;
        return left_y < right_y ||
               (left_y == right_y && left->id < right->id);
    });

    for (const auto* object : ordered) {
        if (!render_environment_texture(renderer, textures, *object, map,
                                        transform, true)) {
            return false;
        }
    }
    for (const auto* object : ordered) {
        if (!render_environment_texture(renderer, textures, *object, map,
                                        transform, false)) {
            return false;
        }
    }
    return true;
}

bool render_zone_boundaries(SDL_Renderer* renderer, const World& world,
                            const WorldTransform& transform) noexcept {
    const float line_width = std::max(1.0F, std::round(transform.scale()));
    bool first_zone = true;
    for (const ZoneDefinition& zone : world.map().zones) {
        if (first_zone) {
            first_zone = false;
            continue;
        }
        const float boundary_x = zone.bounds.x;
        const auto top =
            transform.world_to_drawable(Point{boundary_x, 0.0F});
        const SDL_FRect line{std::round(top.x - line_width * 0.5F),
                             std::round(transform.viewport().y), line_width,
                             std::round(transform.viewport().height)};
        set_color(renderer, battlefield_theme.zone_boundary);
        if (!SDL_RenderFillRect(renderer, &line)) {
            return false;
        }
    }
    return true;
}

bool render_home_zone(SDL_Renderer* renderer, const MapDefinition& map,
                      const Zone& zone,
                      const WorldTransform& transform) noexcept {
    const Bounds bounds = zone.bounds();
    const bool team_a = zone.owner() == Team::team_a;
    const Color accent = team_a ? battlefield_theme.home_team_a
                                : battlefield_theme.home_team_b;
    const TeamForwardDefinition* forward =
        team_forward_definition(map, zone.owner());
    const float edge_x = forward != nullptr && forward->x_direction > 0.0F
        ? bounds.x + bounds.width - battlefield_theme.home_edge_width
        : bounds.x;
    if (!fill_world_rect(renderer, transform,
                         Bounds{edge_x, bounds.y,
                                battlefield_theme.home_edge_width,
                                bounds.height},
                         Color{accent.red, accent.green, accent.blue, 42})) {
        return false;
    }
    const SDL_FRect outline = drawable_rect(transform, bounds);
    set_color(renderer, Color{accent.red, accent.green, accent.blue, 118});
    return SDL_RenderRect(renderer, &outline);
}

bool render_objective_owner(SDL_Renderer* renderer, const Zone& zone,
                            const WorldTransform& transform) noexcept {
    if (zone.owner() == Team::none) {
        return true;
    }
    const Color accent = zone.owner() == Team::team_a
                             ? battlefield_theme.objective_team_a
                             : battlefield_theme.objective_team_b;
    const Bounds bounds = zone.bounds();
    const float edge = battlefield_theme.objective_edge_width;
    return fill_world_rect(renderer, transform,
                           Bounds{bounds.x, bounds.y, bounds.width, edge}, accent) &&
           fill_world_rect(renderer, transform,
                           Bounds{bounds.x, bounds.y + bounds.height - edge,
                                  bounds.width, edge}, accent);
}

bool render_frontline(SDL_Renderer* renderer, const World& world,
                      const WorldTransform& transform, const Team team) noexcept {
    const auto frontline = frontline_objective(world, team);
    if (!frontline.has_value()) {
        return true;
    }

    const Color accent = team == Team::team_a
                             ? battlefield_theme.frontline_team_a
                             : battlefield_theme.frontline_team_b;
    if (!fill_world_rect(
            renderer, transform,
            Bounds{frontline->forward_boundary_x -
                       battlefield_theme.frontline_band_width * 0.5F,
                   0.0F, battlefield_theme.frontline_band_width,
                   world.map().logical_height},
            Color{accent.red, accent.green, accent.blue, 22})) {
        return false;
    }

    const float stride = battlefield_theme.frontline_dash_length +
                         battlefield_theme.frontline_dash_gap;
    const float line_width = std::max(1.0F, std::round(2.0F * transform.scale()));
    for (float y = 0.0F; y < world.map().logical_height; y += stride) {
        const auto top = transform.world_to_drawable(
            Point{frontline->forward_boundary_x, y});
        const auto bottom = transform.world_to_drawable(Point{
            frontline->forward_boundary_x,
            std::min(world.map().logical_height,
                     y + battlefield_theme.frontline_dash_length)});
        const SDL_FRect dash{std::round(top.x - line_width * 0.5F),
                             std::round(top.y), line_width,
                             std::max(1.0F, std::round(bottom.y - top.y))};
        set_color(renderer, accent);
        if (!SDL_RenderFillRect(renderer, &dash)) {
            return false;
        }
    }
    return true;
}

} // namespace

bool BattlefieldRenderer::authored_assets_available(
    const TextureCache& textures, const World& world) noexcept {
    if (asset_check_complete_) {
        return authored_assets_available_;
    }
    asset_check_complete_ = true;
    authored_assets_available_ = !world.map().terrain_regions.empty();
    const auto exists = [&textures](const std::filesystem::path& relative) {
        std::error_code error;
        return !relative.empty() &&
               std::filesystem::is_regular_file(
                   textures.asset_root() / relative, error) &&
               !error;
    };
    for (const auto& region : world.map().terrain_regions) {
        authored_assets_available_ &= exists(terrain_asset_path(region.terrain));
    }
    for (const auto& object : world.map().environment_objects) {
        authored_assets_available_ &=
            exists(environment_asset_path(object.asset, false)) &&
            exists(environment_asset_path(object.asset, true));
    }
    if (!authored_assets_available_) {
        std::fprintf(stderr,
                     "Terrain assets for map '%.*s' are incomplete under '%s'; "
                     "using the procedural battlefield fallback. Run "
                     "scripts/sync_assets.sh to populate them.\n",
                     static_cast<int>(world.map().id.size()),
                     world.map().id.data(), textures.asset_root().string().c_str());
    }
    return authored_assets_available_;
}

bool BattlefieldRenderer::render(SDL_Renderer* renderer, TextureCache& textures,
                                 const World& world,
                                 const WorldTransform& transform) noexcept {
    if (!SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND)) {
        return false;
    }
    if (authored_assets_available(textures, world)) {
        if (!render_authored_terrain(renderer, textures, world.map(), transform) ||
            !render_environment(renderer, textures, world.map(), transform)) {
            return false;
        }
    } else if (!render_procedural_terrain(renderer, world, transform)) {
        return false;
    }
    if (!render_zone_boundaries(renderer, world, transform)) {
        return false;
    }

    for (const Zone& zone : world.zones()) {
        if (zone.type() == ZoneType::home) {
            if (!render_home_zone(renderer, world.map(), zone, transform)) {
                return false;
            }
        } else if (!render_objective_owner(renderer, zone, transform)) {
            return false;
        }
    }

    return render_frontline(renderer, world, transform, Team::team_a) &&
           render_frontline(renderer, world, transform, Team::team_b);
}

} // namespace siege
