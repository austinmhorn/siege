#include "client/renderer.hpp"

#include "client/world_transform.hpp"
#include "core/math.hpp"
#include "world/unit.hpp"
#include "world/world.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <utility>

namespace siege {
namespace {

struct Color {
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    Uint8 alpha;
};

constexpr Color letterbox{12, 15, 20, 255};
constexpr Color neutral{66, 72, 78, 255};
constexpr Color team_a{46, 91, 132, 255};
constexpr Color team_b{132, 55, 50, 255};
constexpr Color divider{196, 203, 207, 255};
constexpr Color team_a_projectile{126, 218, 255, 255};
constexpr Color team_b_projectile{255, 174, 102, 255};
constexpr float projectile_tracer_length = 18.0F;

struct SoldierVisualLayout {
    float source_pixel_world_size;
    float render_scale;
    float legs_canvas_size;
    float upper_canvas_size;
    std::size_t non_firing_rifle_frame;
};

constexpr SoldierVisualLayout soldier_layout{
    .source_pixel_world_size = 2.0F,
    .render_scale = 0.5F,
    .legs_canvas_size = 32.0F,
    .upper_canvas_size = 64.0F,
    .non_firing_rifle_frame = 1,
};

void set_color(SDL_Renderer* renderer, const Color color) {
    SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, color.alpha);
}

Color color_for(const Zone& zone) {
    switch (zone.owner()) {
    case Team::team_a:
        return team_a;
    case Team::team_b:
        return team_b;
    case Team::none:
        return neutral;
    }
    return neutral;
}

std::filesystem::path frame_path(const std::string& layer, const std::string& prefix,
                                 const std::size_t frame) {
    return std::filesystem::path{"soldiers/color1/soldier1"} / layer /
           (prefix + std::to_string(frame) + ".png");
}

} // namespace

Renderer::Renderer(SDL_Renderer* renderer, std::filesystem::path asset_root)
    : renderer_(renderer), textures_(renderer, std::move(asset_root)),
      debug_renderer_(renderer) {}

void Renderer::update(const World& world, const double fixed_delta_seconds) {
    for (const auto& unit : world.units()) {
        auto [entry, inserted] = leg_animations_.try_emplace(
            unit.id(), std::vector<std::size_t>{1, 2, 3, 4, 5, 6, 7}, 0.10, true);
        static_cast<void>(inserted);
        if (unit.movement_state() == MovementState::moving) {
            entry->second.update(fixed_delta_seconds);
        } else {
            entry->second.reset();
        }
    }
}

void Renderer::toggle_debug_overlay() noexcept {
    debug_overlay_enabled_ = !debug_overlay_enabled_;
}

bool Renderer::render(const World& world, const double interpolation_alpha,
                      const double render_fps) const {
    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetRenderOutputSize(renderer_, &output_width, &output_height)) {
        return false;
    }

    const WorldTransform transform{World::width, World::height, output_width, output_height};

    set_color(renderer_, letterbox);
    if (!SDL_RenderClear(renderer_)) {
        return false;
    }

    for (const auto& zone : world.zones()) {
        const auto bounds = transform.world_to_drawable(zone.bounds());
        const SDL_FRect rectangle{bounds.x, bounds.y, bounds.width, bounds.height};
        set_color(renderer_, color_for(zone));
        if (!SDL_RenderFillRect(renderer_, &rectangle)) {
            return false;
        }
    }

    set_color(renderer_, divider);
    const float line_width = std::max(1.0F, 3.0F * transform.scale());
    for (std::size_t index = 1; index < world.zones().size(); ++index) {
        const auto& bounds = world.zones()[index].bounds();
        const auto top = transform.world_to_drawable(Point{bounds.x, 0.0F});
        const SDL_FRect divider_rectangle{top.x - line_width * 0.5F, top.y, line_width,
                                          transform.viewport().height};
        if (!SDL_RenderFillRect(renderer_, &divider_rectangle)) {
            return false;
        }
    }

    if (!render_projectiles(world, transform, interpolation_alpha) ||
        !render_units(world, transform, interpolation_alpha)) {
        return false;
    }

    if (debug_overlay_enabled_ &&
        !debug_renderer_.render(world, transform, render_fps, 60.0)) {
        return false;
    }

    return SDL_RenderPresent(renderer_);
}

bool Renderer::render_projectiles(const World& world,
                                  const WorldTransform& transform,
                                  const double interpolation_alpha) const {
    const float alpha = static_cast<float>(interpolation_alpha);
    for (const auto& projectile : world.projectiles()) {
        const Vec2 position =
            lerp(projectile.previous_position(), projectile.position(), alpha);
        const Vec2 direction = normalized(projectile.velocity());
        const Vec2 trail = position - direction * projectile_tracer_length;
        const auto draw_position =
            transform.world_to_drawable(Point{position.x, position.y});
        const auto draw_trail = transform.world_to_drawable(Point{trail.x, trail.y});

        set_color(renderer_, projectile.team() == Team::team_a
                                 ? team_a_projectile
                                 : team_b_projectile);
        if (!SDL_RenderLine(renderer_, draw_trail.x, draw_trail.y,
                            draw_position.x, draw_position.y) ||
            !SDL_RenderPoint(renderer_, draw_position.x, draw_position.y)) {
            return false;
        }
    }
    return true;
}

bool Renderer::render_units(const World& world, const WorldTransform& transform,
                            const double interpolation_alpha) const {
    for (const auto& unit : world.units()) {
        if (unit.troop_type() != TroopType::rifle) {
            continue;
        }

        const float alpha = static_cast<float>(interpolation_alpha);
        const Vec2 position = lerp(unit.previous_position(), unit.position(), alpha);
        const float facing =
            lerp_angle(unit.previous_facing_angle(), unit.facing_angle(), alpha);
        std::size_t leg_frame = 1;
        if (const auto animation = leg_animations_.find(unit.id());
            animation != leg_animations_.end()) {
            leg_frame = animation->second.current_frame();
        }

        const auto render_layer = [this, &transform, position, facing](
                                      const std::filesystem::path& path,
                                      const float canvas_size) {
            SDL_Texture* texture = textures_.get(path);
            if (texture == nullptr) {
                return false;
            }

            const float world_size = canvas_size * soldier_layout.source_pixel_world_size *
                                     soldier_layout.render_scale;
            const Bounds world_bounds{
                position.x - world_size * 0.5F,
                position.y - world_size * 0.5F,
                world_size,
                world_size,
            };
            const auto bounds = transform.world_to_drawable(world_bounds);
            const SDL_FRect destination{bounds.x, bounds.y, bounds.width, bounds.height};
            const SDL_FPoint pivot{destination.w * 0.5F, destination.h * 0.5F};
            return SDL_RenderTextureRotated(renderer_, texture, nullptr, &destination,
                                            facing, &pivot, SDL_FLIP_NONE);
        };

        const std::array layers{
            std::pair{frame_path("shadows/legs", "legs", leg_frame),
                      soldier_layout.legs_canvas_size},
            std::pair{frame_path("shadows/rifle", "rifle",
                                 soldier_layout.non_firing_rifle_frame),
                      soldier_layout.upper_canvas_size},
            std::pair{frame_path("legs", "legs", leg_frame),
                      soldier_layout.legs_canvas_size},
            std::pair{frame_path("rifle", "rifle",
                                 soldier_layout.non_firing_rifle_frame),
                      soldier_layout.upper_canvas_size},
        };

        for (const auto& [path, canvas_size] : layers) {
            if (!render_layer(path, canvas_size)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace siege
