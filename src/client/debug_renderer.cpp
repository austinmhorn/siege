#include "client/debug_renderer.hpp"

#include "client/world_transform.hpp"
#include "core/math.hpp"
#include "world/unit.hpp"
#include "world/world.hpp"

#include <SDL3/SDL.h>

#include <cmath>

namespace siege {
namespace {

constexpr float direction_length = 55.0F;
constexpr float preferred_half_width = 65.0F;

void set_color(SDL_Renderer* renderer, const Uint8 red, const Uint8 green,
               const Uint8 blue, const Uint8 alpha = 255) {
    SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
}

Vec2 direction_from_facing(const float degrees) noexcept {
    constexpr float degrees_to_radians = 0.017453292519943295F;
    const float radians = degrees * degrees_to_radians;
    return Vec2{-std::sin(radians), std::cos(radians)};
}

bool draw_world_line(SDL_Renderer* renderer, const WorldTransform& transform,
                     const Vec2 from, const Vec2 to) {
    const auto draw_from = transform.world_to_drawable(Point{from.x, from.y});
    const auto draw_to = transform.world_to_drawable(Point{to.x, to.y});
    return SDL_RenderLine(renderer, draw_from.x, draw_from.y, draw_to.x, draw_to.y);
}

} // namespace

DebugRenderer::DebugRenderer(SDL_Renderer* renderer) noexcept : renderer_(renderer) {}

bool DebugRenderer::render(const World& world, const WorldTransform& transform,
                           const double render_fps, const double simulation_hz) const {
    set_color(renderer_, 245, 245, 245);
    if (!SDL_RenderDebugTextFormat(renderer_, transform.viewport().x + 8.0F,
                                   transform.viewport().y + 8.0F,
                                   "F3 debug | sim %.0f Hz | render %.1f FPS | units %zu",
                                   simulation_hz, render_fps, world.units().size())) {
        return false;
    }

    for (const auto& unit : world.units()) {
        const Vec2 position = unit.position();
        const auto marker = transform.world_to_drawable(Point{position.x, position.y});
        const float marker_size = 8.0F;

        set_color(renderer_, 255, 255, 255);
        if (!SDL_RenderLine(renderer_, marker.x - marker_size, marker.y,
                            marker.x + marker_size, marker.y) ||
            !SDL_RenderLine(renderer_, marker.x, marker.y - marker_size,
                            marker.x, marker.y + marker_size)) {
            return false;
        }

        set_color(renderer_, 255, 215, 40);
        const Vec2 facing_end =
            position + direction_from_facing(unit.facing_angle()) * direction_length;
        if (!draw_world_line(renderer_, transform, position, facing_end)) {
            return false;
        }

        set_color(renderer_, 255, 80, 220);
        const Vec2 desired_end =
            position + direction_from_facing(unit.desired_facing_angle()) * direction_length;
        if (!draw_world_line(renderer_, transform, position, desired_end)) {
            return false;
        }

        set_color(renderer_, 80, 235, 255, 190);
        if (!draw_world_line(renderer_, transform,
                             {position.x - preferred_half_width, unit.preferred_y()},
                             {position.x + preferred_half_width, unit.preferred_y()})) {
            return false;
        }

        set_color(renderer_, 255, 255, 255);
        const auto type = to_string(unit.troop_type());
        const auto team = to_string(unit.team());
        const auto state = to_string(unit.movement_state());
        if (!SDL_RenderDebugTextFormat(
                renderer_, marker.x + 10.0F, marker.y - 19.0F, "#%u %.*s %.*s %.*s",
                unit.id(), static_cast<int>(type.size()), type.data(),
                static_cast<int>(team.size()), team.data(), static_cast<int>(state.size()),
                state.data()) ||
            !SDL_RenderDebugTextFormat(renderer_, marker.x + 10.0F, marker.y - 9.0F,
                                       "p %.0f,%.0f py %.0f m%.0f r%.0f", position.x,
                                       position.y, unit.preferred_y(), unit.move_speed(),
                                       unit.rotation_speed())) {
            return false;
        }
    }

    return true;
}

} // namespace siege
