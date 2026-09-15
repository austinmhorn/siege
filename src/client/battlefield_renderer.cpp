#include "client/battlefield_renderer.hpp"

#include "client/world_transform.hpp"
#include "core/frontline.hpp"
#include "world/world.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

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

bool render_terrain(SDL_Renderer* renderer, const World& world,
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

bool render_battlefield(SDL_Renderer* renderer, const World& world,
                        const WorldTransform& transform) noexcept {
    if (!SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND) ||
        !render_terrain(renderer, world, transform) ||
        !render_zone_boundaries(renderer, world, transform)) {
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
