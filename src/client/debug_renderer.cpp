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
constexpr int circle_segments = 48;
constexpr int cone_arc_segments = 24;

void set_color(SDL_Renderer* renderer, const Uint8 red, const Uint8 green,
               const Uint8 blue, const Uint8 alpha = 255) {
    SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
}

bool draw_world_line(SDL_Renderer* renderer, const WorldTransform& transform,
                     const Vec2 from, const Vec2 to) {
    const auto draw_from = transform.world_to_drawable(Point{from.x, from.y});
    const auto draw_to = transform.world_to_drawable(Point{to.x, to.y});
    return SDL_RenderLine(renderer, draw_from.x, draw_from.y, draw_to.x, draw_to.y);
}

bool draw_world_circle(SDL_Renderer* renderer, const WorldTransform& transform,
                       const Vec2 center, const float radius) {
    Vec2 previous = center + direction_from_facing(0.0F) * radius;
    for (int segment = 1; segment <= circle_segments; ++segment) {
        const float angle = 360.0F * static_cast<float>(segment) /
                            static_cast<float>(circle_segments);
        const Vec2 current = center + direction_from_facing(angle) * radius;
        if (!draw_world_line(renderer, transform, previous, current)) {
            return false;
        }
        previous = current;
    }
    return true;
}

bool draw_vision_cone(SDL_Renderer* renderer, const WorldTransform& transform,
                      const Unit& unit) {
    const Vec2 center = unit.position();
    const float half_angle = unit.vision_angle() * 0.5F;
    const float first_angle = unit.facing_angle() - half_angle;
    Vec2 previous = center +
                    direction_from_facing(first_angle) * unit.vision_range();
    if (!draw_world_line(renderer, transform, center, previous)) {
        return false;
    }

    for (int segment = 1; segment <= cone_arc_segments; ++segment) {
        const float angle = first_angle + unit.vision_angle() *
                                              static_cast<float>(segment) /
                                              static_cast<float>(cone_arc_segments);
        const Vec2 current = center +
                             direction_from_facing(angle) * unit.vision_range();
        if (!draw_world_line(renderer, transform, previous, current)) {
            return false;
        }
        previous = current;
    }

    return draw_world_line(renderer, transform, center, previous);
}

} // namespace

DebugRenderer::DebugRenderer(SDL_Renderer* renderer) noexcept : renderer_(renderer) {}

bool DebugRenderer::render(const World& world, const WorldTransform& transform,
                           const double render_fps, const double simulation_hz,
                           const std::size_t corpse_count,
                           const std::size_t firing_effect_count) const {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    set_color(renderer_, 245, 245, 245);
    if (!SDL_RenderDebugTextFormat(renderer_, transform.viewport().x + 8.0F,
                                   transform.viewport().y + 8.0F,
                                   "F3 debug | sim %.0f Hz | render %.1f FPS | units %zu | projectiles %zu | corpses %zu | firing %zu",
                                   simulation_hz, render_fps, world.units().size(),
                                   world.projectiles().size(), corpse_count,
                                   firing_effect_count)) {
        return false;
    }

    for (const auto& unit : world.units()) {
        const Vec2 position = unit.position();
        const auto marker = transform.world_to_drawable(Point{position.x, position.y});
        const float marker_size = 8.0F;

        set_color(renderer_, 255, 214, 64, 115);
        if (!draw_vision_cone(renderer_, transform, unit)) {
            return false;
        }

        set_color(renderer_, 99, 230, 155, 150);
        if (!draw_world_circle(renderer_, transform, position,
                               unit.awareness_radius())) {
            return false;
        }

        set_color(renderer_, 105, 165, 255, 80);
        if (!draw_world_circle(renderer_, transform, position,
                               unit.preferred_combat_range())) {
            return false;
        }

        if (unit.target_id().has_value()) {
            const Unit* target = world.find_unit(*unit.target_id());
            if (target != nullptr) {
                set_color(renderer_, 255, 92, 92, 205);
                if (!draw_world_line(renderer_, transform, position,
                                     target->position())) {
                    return false;
                }
            }
        }

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
        const auto combat_state = to_string(unit.combat_movement_state());
        const bool target_text_rendered = unit.target_id().has_value()
            ? SDL_RenderDebugTextFormat(
                  renderer_, marker.x + 10.0F, marker.y - 19.0F,
                  "#%u %.*s %.*s %.*s/%.*s target #%u", unit.id(),
                  static_cast<int>(type.size()), type.data(),
                  static_cast<int>(team.size()), team.data(),
                  static_cast<int>(state.size()), state.data(),
                  static_cast<int>(combat_state.size()), combat_state.data(),
                  *unit.target_id())
            : SDL_RenderDebugTextFormat(
                  renderer_, marker.x + 10.0F, marker.y - 19.0F,
                  "#%u %.*s %.*s %.*s/%.*s target none", unit.id(),
                  static_cast<int>(type.size()), type.data(),
                  static_cast<int>(team.size()), team.data(),
                  static_cast<int>(state.size()), state.data(),
                  static_cast<int>(combat_state.size()), combat_state.data());
        if (!target_text_rendered ||
            !SDL_RenderDebugTextFormat(renderer_, marker.x + 10.0F, marker.y - 9.0F,
                                       "p %.0f,%.0f py %.0f m%.0f r%.0f", position.x,
                                       position.y, unit.preferred_y(), unit.move_speed(),
                                       unit.rotation_speed()) ||
            !SDL_RenderDebugTextFormat(renderer_, marker.x + 10.0F, marker.y + 1.0F,
                                       "vision %.0f/%.0f aware %.0f combat %.0f+/-%.0f",
                                       unit.vision_range(), unit.vision_angle(),
                                       unit.awareness_radius(),
                                       unit.preferred_combat_range(),
                                       unit.range_tolerance()) ||
            !SDL_RenderDebugTextFormat(renderer_, marker.x + 10.0F, marker.y + 11.0F,
                                       "hp %.0f/%.0f weapon %.0f arc %.0f cd %.2f",
                                       unit.health(), unit.max_health(),
                                       unit.weapon().range,
                                       unit.weapon().firing_arc,
                                       unit.weapon_cooldown_remaining())) {
            return false;
        }
    }

    return true;
}

} // namespace siege
