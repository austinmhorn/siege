#include "client/debug_renderer.hpp"

#include "client/capture_bar.hpp"
#include "client/ui_layout.hpp"
#include "client/world_transform.hpp"
#include "core/economy.hpp"
#include "core/math.hpp"
#include "core/troop_definition.hpp"
#include "core/zone_capture.hpp"
#include "world/unit.hpp"
#include "world/world.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

namespace siege {
namespace {

constexpr float direction_length = 55.0F;
constexpr float preferred_half_width = 65.0F;
constexpr int circle_segments = 48;
constexpr int cone_arc_segments = 24;
constexpr float capture_bar_inset = 48.0F;
constexpr float capture_bar_y = 120.0F;
constexpr FontColor debug_text{245, 245, 245, 255};
constexpr FontColor debug_muted{190, 200, 208, 255};
constexpr FontColor debug_heading{255, 221, 105, 255};
constexpr float panel_margin = 8.0F;
constexpr float panel_padding = 8.0F;
constexpr float left_panel_width = 256.0F;
constexpr float unit_column_preferred_width = 190.0F;
constexpr float unit_column_minimum_width = 148.0F;
constexpr float unit_block_gap = 6.0F;
constexpr std::size_t unit_block_line_count = 12;

struct TextCursor {
    FontSystem& fonts;
    float x;
    float y;
    bool succeeded{true};

    template <typename... Args>
    void format(const FontRole role, const FontColor color,
                const char* format_string, Args&&... args) {
        if (succeeded) {
            succeeded = fonts.draw_format(x, y, role, color, format_string,
                                          std::forward<Args>(args)...);
        }
        y += Typography::debug_line_height;
    }

    void line(const FontRole role, const FontColor color,
              const std::string_view text) {
        if (succeeded) {
            succeeded = fonts.draw(x, y, text, role, color);
        }
        y += Typography::debug_line_height;
    }

    void blank() noexcept { y += Typography::debug_line_height; }
};

const char* short_team_name(const Team team) noexcept {
    switch (team) {
    case Team::team_a:
        return "A";
    case Team::team_b:
        return "B";
    case Team::none:
        return "neutral";
    }
    return "?";
}

const char* yes_no(const bool value) noexcept {
    return value ? "yes" : "no";
}

void set_color(SDL_Renderer* renderer, const Uint8 red, const Uint8 green,
               const Uint8 blue, const Uint8 alpha = 255) {
    SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
}

bool draw_panel(SDL_Renderer* renderer, const SDL_FRect rectangle) {
    set_color(renderer, 7, 10, 14, 220);
    if (!SDL_RenderFillRect(renderer, &rectangle)) {
        return false;
    }
    set_color(renderer, 120, 135, 148, 190);
    return SDL_RenderRect(renderer, &rectangle);
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

DebugRenderer::DebugRenderer(SDL_Renderer* renderer, FontSystem& fonts) noexcept
    : renderer_(renderer), fonts_(fonts) {}

bool DebugRenderer::render(const World& world, const WorldTransform& transform,
                           const double render_fps, const double simulation_hz,
                           const std::size_t corpse_count,
                           const std::size_t firing_effect_count,
                           const std::size_t explosion_effect_count) const {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    for (const auto& zone : world.zones()) {
        if (zone.type() != ZoneType::objective) {
            continue;
        }
        if (const auto deployment = deployment_bounds(zone, zone.owner())) {
            set_color(renderer_, 120, 255, 150, 190);
            if (!draw_world_bounds(renderer_, transform, *deployment)) {
                return false;
            }
        }
        if (!draw_capture_meter(renderer_, transform, zone)) {
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

    }

    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetRenderOutputSize(renderer_, &output_width, &output_height)) {
        return false;
    }
    const float panel_top = panel_margin;
    const float panel_bottom =
        static_cast<float>(output_height) - ui_layout::deployment_bar_height -
        panel_margin;
    const float panel_height = std::max(1.0F, panel_bottom - panel_top);

    const SDL_FRect left_panel{panel_margin, panel_top, left_panel_width,
                               panel_height};
    if (!draw_panel(renderer_, left_panel)) {
        return false;
    }

    TextCursor global{fonts_, left_panel.x + panel_padding,
                      left_panel.y + panel_padding};
    global.line(FontRole::debug_bold, debug_heading, "SIEGE F3");
    global.format(FontRole::debug, debug_text, "simulation: %.0f Hz",
                  simulation_hz);
    global.format(FontRole::debug, debug_text, "render: %.0f FPS", render_fps);
    global.format(FontRole::debug, debug_text, "units: %zu", world.units().size());
    global.format(FontRole::debug, debug_text, "projectiles: %zu",
                  world.projectiles().size());
    global.format(FontRole::debug, debug_text, "pending: %zu",
                  world.pending_deployments().size());
    global.format(FontRole::debug, debug_text, "corpses: %zu", corpse_count);
    global.format(FontRole::debug, debug_text, "firing effects: %zu",
                  firing_effect_count);
    global.format(FontRole::debug, debug_text, "explosion effects: %zu",
                  explosion_effect_count);
    global.blank();

    global.line(FontRole::debug_bold, debug_heading, "ECONOMY");
    const PlayerState* team_a_player = world.find_player(Team::team_a);
    const PlayerState* team_b_player = world.find_player(Team::team_b);
    global.format(FontRole::debug, debug_text, "Team A cash: $%lld",
                  static_cast<long long>(team_a_player == nullptr
                                             ? 0
                                             : team_a_player->cash()));
    global.format(FontRole::debug, debug_text, "Team B cash: $%lld",
                  static_cast<long long>(team_b_player == nullptr
                                             ? 0
                                             : team_b_player->cash()));
    global.format(FontRole::debug, debug_text, "passive: $%lld/s",
                  static_cast<long long>(
                      default_economy_rules.passive_income_per_second));
    global.blank();

    global.line(FontRole::debug_bold, debug_heading, "OBJECTIVES");
    for (const auto& zone : world.zones()) {
        if (zone.type() != ZoneType::objective) {
            continue;
        }
        global.format(FontRole::debug_bold, debug_text, "Zone %zu", zone.index());
        global.format(FontRole::debug, debug_text, "owner: %s",
                      short_team_name(zone.owner()));
        global.format(FontRole::debug, debug_text, "presence: A%d / B%d",
                      zone.team_a_count(), zone.team_b_count());
        global.format(FontRole::debug, debug_text, "pressure: %+d",
                      zone.pressure());
        global.format(FontRole::debug, debug_text, "capture: %+.1f",
                      zone.capture_value());
        global.format(FontRole::debug, debug_text, "occupied: %s",
                      yes_no(zone.occupied()));
        global.format(FontRole::debug, debug_text, "contested: %s",
                      yes_no(zone.contested()));
        global.format(FontRole::debug, debug_text, "secure: %.1f / %.1fs",
                      zone.secure_timer_seconds(),
                      default_zone_security_rules.secure_duration_seconds);
        global.format(FontRole::debug, debug_text, "secured: %s",
                      yes_no(zone.secured()));
        global.format(FontRole::debug, debug_text, "deploy A/B: %s / %s",
                      yes_no(is_zone_deployable(zone, Team::team_a)),
                      yes_no(is_zone_deployable(zone, Team::team_b)));
    }
    if (!global.succeeded) {
        return false;
    }

    const float unit_block_height =
        static_cast<float>(unit_block_line_count) * Typography::debug_line_height +
        unit_block_gap;
    const std::size_t blocks_per_column = std::max<std::size_t>(
        1, static_cast<std::size_t>(panel_height / unit_block_height));
    const float right_region_left = left_panel.x + left_panel.w + panel_margin;
    const float right_region_width = std::max(
        unit_column_minimum_width,
        static_cast<float>(output_width) - right_region_left - panel_margin);
    const std::size_t requested_columns = std::max<std::size_t>(
        1, (world.units().size() + blocks_per_column - 1) / blocks_per_column);
    const std::size_t maximum_columns = std::max<std::size_t>(
        1, static_cast<std::size_t>(right_region_width /
                                    unit_column_minimum_width));
    const std::size_t column_count =
        std::min(requested_columns, maximum_columns);
    const float column_width = std::min(
        unit_column_preferred_width,
        right_region_width / static_cast<float>(column_count));
    const float columns_left = static_cast<float>(output_width) - panel_margin -
                               column_width * static_cast<float>(column_count);

    for (std::size_t column = 0; column < column_count; ++column) {
        const SDL_FRect panel{
            columns_left + column_width * static_cast<float>(column), panel_top,
            column_width, panel_height};
        if (!draw_panel(renderer_, panel)) {
            return false;
        }
    }

    const std::size_t capacity = blocks_per_column * column_count;
    const bool has_overflow = world.units().size() > capacity;
    const std::size_t displayed_units =
        has_overflow && capacity > 0 ? capacity - 1
                                     : std::min(world.units().size(), capacity);
    for (std::size_t index = 0; index < displayed_units; ++index) {
        const Unit& unit = world.units()[index];
        const std::size_t column = index / blocks_per_column;
        const std::size_t row = index % blocks_per_column;
        TextCursor cursor{
            fonts_,
            columns_left + column_width * static_cast<float>(column) +
                panel_padding,
            panel_top + panel_padding +
                static_cast<float>(row) * unit_block_height};
        const auto name = troop_display_name(unit.troop_type());
        cursor.format(FontRole::debug_bold, debug_heading, "#%u %.*s", unit.id(),
                      static_cast<int>(name.size()), name.data());
        cursor.format(FontRole::debug, debug_text, "team: %s",
                      short_team_name(unit.team()));
        const auto state = to_string(unit.combat_movement_state());
        cursor.format(FontRole::debug, debug_text, "state: %.*s",
                      static_cast<int>(state.size()), state.data());
        cursor.format(FontRole::debug, debug_text, "hp: %.0f/%.0f", unit.health(),
                      unit.max_health());
        if (unit.target_id().has_value()) {
            cursor.format(FontRole::debug, debug_text, "target: #%u",
                          *unit.target_id());
        } else {
            cursor.line(FontRole::debug, debug_muted, "target: none");
        }
        cursor.format(FontRole::debug, debug_text, "pos: %.0f, %.0f",
                      unit.position().x, unit.position().y);
        cursor.format(FontRole::debug, debug_text, "preferred y: %.0f",
                      unit.preferred_y());
        cursor.format(FontRole::debug, debug_text, "move: %.0f",
                      unit.move_speed());
        cursor.format(FontRole::debug, debug_text, "rotation: %.0f",
                      unit.rotation_speed());
        cursor.format(FontRole::debug, debug_text, "vision: %.0f / %.0fdeg",
                      unit.vision_range(), unit.vision_angle());
        cursor.format(FontRole::debug, debug_text, "range: %.0f +/-%.0f",
                      unit.preferred_combat_range(), unit.range_tolerance());
        cursor.format(FontRole::debug, debug_text, "cooldown: %.2f",
                      unit.weapon_cooldown_remaining());
        if (!cursor.succeeded) {
            return false;
        }
    }

    if (has_overflow) {
        const std::size_t overflow_slot = capacity - 1;
        const std::size_t column = overflow_slot / blocks_per_column;
        const std::size_t row = overflow_slot % blocks_per_column;
        TextCursor overflow{
            fonts_,
            columns_left + column_width * static_cast<float>(column) +
                panel_padding,
            panel_top + panel_padding +
                static_cast<float>(row) * unit_block_height};
        overflow.format(FontRole::debug_bold, debug_heading, "+%zu more units",
                        world.units().size() - displayed_units);
        if (!overflow.succeeded) {
            return false;
        }
    }

    return true;
}

} // namespace siege
