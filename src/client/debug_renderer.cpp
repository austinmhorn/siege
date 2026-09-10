#include "client/debug_renderer.hpp"

#include "client/capture_bar.hpp"
#include "client/world_transform.hpp"
#include "core/economy.hpp"
#include "core/math.hpp"
#include "core/zone_capture.hpp"
#include "world/unit.hpp"
#include "world/world.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace siege {
namespace {

constexpr float direction_length = 55.0F;
constexpr float preferred_half_width = 65.0F;
constexpr int circle_segments = 48;
constexpr int cone_arc_segments = 24;
constexpr float capture_bar_inset = 48.0F;
constexpr float capture_bar_y = 58.0F;

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

bool draw_world_bounds(SDL_Renderer* renderer,
                       const WorldTransform& transform, const Bounds bounds) {
    const Vec2 top_left{bounds.x, bounds.y};
    const Vec2 top_right{bounds.x + bounds.width, bounds.y};
    const Vec2 bottom_right{bounds.x + bounds.width, bounds.y + bounds.height};
    const Vec2 bottom_left{bounds.x, bounds.y + bounds.height};
    return draw_world_line(renderer, transform, top_left, top_right) &&
           draw_world_line(renderer, transform, top_right, bottom_right) &&
           draw_world_line(renderer, transform, bottom_right, bottom_left) &&
           draw_world_line(renderer, transform, bottom_left, top_left);
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

bool draw_capture_meter(SDL_Renderer* renderer,
                        const WorldTransform& transform, const Zone& zone) {
    const Bounds& bounds = zone.bounds();
    const float left = bounds.x + capture_bar_inset;
    const float right = bounds.x + bounds.width - capture_bar_inset;
    const float center = (left + right) * 0.5F;

    set_color(renderer, 92, 155, 255, 210);
    if (!draw_world_line(renderer, transform, {left, capture_bar_y},
                         {center, capture_bar_y})) {
        return false;
    }
    set_color(renderer, 235, 92, 92, 210);
    if (!draw_world_line(renderer, transform, {center, capture_bar_y},
                         {right, capture_bar_y})) {
        return false;
    }

    const float capture_fraction = capture_bar_fraction(zone.capture_value());
    const float marker_x = left + (right - left) * capture_fraction;
    set_color(renderer, 255, 255, 255, 245);
    return draw_world_line(renderer, transform,
                           {marker_x, capture_bar_y - 8.0F},
                           {marker_x, capture_bar_y + 8.0F});
}

} // namespace

DebugRenderer::DebugRenderer(SDL_Renderer* renderer) noexcept : renderer_(renderer) {}

bool DebugRenderer::render(const World& world, const WorldTransform& transform,
                           const double render_fps, const double simulation_hz,
                           const std::size_t corpse_count,
                           const std::size_t firing_effect_count,
                           const std::size_t explosion_effect_count) const {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    set_color(renderer_, 245, 245, 245);
    if (!SDL_RenderDebugTextFormat(renderer_, transform.viewport().x + 8.0F,
                                   transform.viewport().y + 8.0F,
                                   "F3 debug | sim %.0f Hz | render %.1f FPS | units %zu | pending %zu | projectiles %zu | corpses %zu | firing %zu | explosions %zu",
                                   simulation_hz, render_fps, world.units().size(),
                                   world.pending_deployments().size(),
                                   world.projectiles().size(), corpse_count,
                                   firing_effect_count, explosion_effect_count)) {
        return false;
    }

    const PlayerState* team_a_player = world.find_player(Team::team_a);
    const PlayerState* team_b_player = world.find_player(Team::team_b);
    if (team_a_player == nullptr || team_b_player == nullptr ||
        !SDL_RenderDebugTextFormat(
            renderer_, transform.viewport().x + 8.0F,
            transform.viewport().y + 18.0F,
            "economy | Team A $%lld | Team B $%lld | passive +$%lld/s",
            static_cast<long long>(team_a_player->cash()),
            static_cast<long long>(team_b_player->cash()),
            static_cast<long long>(
                default_economy_rules.passive_income_per_second))) {
        return false;
    }

    for (const auto& zone : world.zones()) {
        if (zone.type() != ZoneType::objective) {
            continue;
        }
        const Bounds& bounds = zone.bounds();
        const auto label = transform.world_to_drawable(
            Point{bounds.x + bounds.width * 0.5F, 34.0F});
        const bool deployable =
            is_zone_deployable(zone, zone.owner());
        if (const auto deployment = deployment_bounds(zone, zone.owner())) {
            set_color(renderer_, 120, 255, 150, 190);
            if (!draw_world_bounds(renderer_, transform, *deployment)) {
                return false;
            }
        }
        set_color(renderer_, 245, 245, 245);
        if (!SDL_RenderDebugTextFormat(
                renderer_, label.x - 84.0F, label.y,
                "Z%zu %.*s A%d B%d P%+d C%+.1f", zone.index(),
                static_cast<int>(to_string(zone.owner()).size()),
                to_string(zone.owner()).data(),
                zone.team_a_count(), zone.team_b_count(), zone.pressure(),
                zone.capture_value()) ||
            !SDL_RenderDebugTextFormat(
                renderer_, label.x - 84.0F, label.y + 10.0F,
                "%s %s T%.1f S%s D%s",
                zone.occupied() ? "occupied" : "unoccupied",
                zone.contested() ? "contested" : "clear",
                zone.secure_timer_seconds(), zone.secured() ? "Y" : "N",
                deployable ? "Y" : "N") ||
            !draw_capture_meter(renderer_, transform, zone)) {
            return false;
        }
    }

    for (const auto& deployment : world.pending_deployments()) {
        const auto marker = transform.world_to_drawable(
            Point{deployment.position.x, deployment.position.y});
        const auto troop = to_string(deployment.troop_type);
        const auto team = to_string(deployment.team);
        set_color(renderer_, 140, 255, 175, 245);
        if (!SDL_RenderDebugTextFormat(
                renderer_, marker.x + 10.0F, marker.y + 12.0F,
                "pending #%u %.*s %.*s %.2fs", deployment.id,
                static_cast<int>(troop.size()), troop.data(),
                static_cast<int>(team.size()), team.data(),
                deployment.remaining_seconds)) {
            return false;
        }
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

        if (length_squared(unit.support_steering()) > 0.0001F) {
            set_color(renderer_, 120, 255, 150, 220);
            const Vec2 support_end =
                position + unit.support_steering() * 80.0F;
            if (!draw_world_line(renderer_, transform, position, support_end)) {
                return false;
            }
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
                                       unit.weapon_cooldown_remaining()) ||
            !SDL_RenderDebugTextFormat(renderer_, marker.x + 10.0F, marker.y + 21.0F,
                                       "projectile %.0f damage %.0f splash %.0f",
                                       unit.weapon().projectile_speed,
                                       unit.weapon().projectile_damage,
                                       unit.weapon().splash_radius)) {
            return false;
        }

        if (unit.support_positioning_bias() > 0.0F) {
            const bool support_text_rendered = unit.support_screen_id().has_value()
                ? SDL_RenderDebugTextFormat(
                      renderer_, marker.x + 10.0F, marker.y + 31.0F,
                      "ai pursue %.2f retreat %.2f support %.2f rear %.0f screen #%u",
                      unit.aggression(), unit.retreat_bias(),
                      unit.support_positioning_bias(),
                      unit.support_rear_distance(), *unit.support_screen_id())
                : SDL_RenderDebugTextFormat(
                      renderer_, marker.x + 10.0F, marker.y + 31.0F,
                      "ai pursue %.2f retreat %.2f support %.2f rear %.0f screen none",
                      unit.aggression(), unit.retreat_bias(),
                      unit.support_positioning_bias(),
                      unit.support_rear_distance());
            if (!support_text_rendered) {
                return false;
            }
        }
    }

    return true;
}

} // namespace siege
