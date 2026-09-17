#include "client/debug_renderer.hpp"

#include "client/capture_bar.hpp"
#include "client/local_control.hpp"
#include "client/ui_layout.hpp"
#include "client/world_transform.hpp"
#include "core/ai_commander.hpp"
#include "core/economy.hpp"
#include "core/environment_line_of_sight.hpp"
#include "core/math.hpp"
#include "core/mortar_observation.hpp"
#include "core/scoring.hpp"
#include "core/tactical_command.hpp"
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
constexpr std::size_t unit_block_line_count = 26;

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
                           const std::size_t explosion_effect_count,
                           const std::span<const std::uint32_t> selected_unit_ids,
                           const Team controlled_team,
                           const AiCommander& ai_commander) const {
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    for (const auto& zone : world.zones()) {
        if (zone.type() != ZoneType::objective) {
            continue;
        }
        if (const auto deployment =
                deployment_bounds(world, zone, zone.owner())) {
            set_color(renderer_, 120, 255, 150, 190);
            if (!draw_world_bounds(renderer_, transform, *deployment)) {
                return false;
            }
        }
        if (!draw_capture_meter(renderer_, transform, zone)) {
            return false;
        }
    }

    set_color(renderer_, 255, 154, 72, 175);
    for (const EnvironmentObjectDefinition& object :
         world.map().environment_objects) {
        if (object.physical.blocks_unit_movement &&
            !draw_world_bounds(renderer_, transform, object.footprint)) {
            return false;
        }
    }

    for (const auto& unit : world.units()) {
        const Vec2 position = unit.position();
        const auto marker = transform.world_to_drawable(Point{position.x, position.y});
        const float marker_size = 8.0F;

        if (unit.troop_type() != TroopType::mortar) {
            set_color(renderer_, 255, 214, 64, 115);
            if (!draw_vision_cone(renderer_, transform, unit)) {
                return false;
            }

            set_color(renderer_, 99, 230, 155, 150);
            if (!draw_world_circle(renderer_, transform, position,
                                   unit.awareness_radius())) {
                return false;
            }
        }

        if (unit.troop_type() != TroopType::mortar) {
            set_color(renderer_, 105, 165, 255, 80);
            if (!draw_world_circle(renderer_, transform, position,
                                   unit.preferred_combat_range())) {
                return false;
            }
        }

        if (unit.target_id().has_value()) {
            const Unit* target = world.find_unit(*unit.target_id());
            if (target != nullptr) {
                const bool clear = unit.troop_type() == TroopType::mortar ||
                    environment_line_of_sight_clear(
                        world.map(), position, target->position());
                set_color(renderer_, clear ? 112 : 255,
                          clear ? 220 : 92, clear ? 150 : 92, 205);
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
        if (unit.has_independent_turret()) {
            set_color(renderer_, 255, 145, 70, 235);
            const Vec2 turret_end = position +
                direction_from_facing(unit.turret_angle()) * direction_length;
            if (!draw_world_line(renderer_, transform, position, turret_end)) {
                return false;
            }
        }

        if (length_squared(unit.support_steering()) > 0.0001F) {
            set_color(renderer_, 120, 255, 150, 220);
            const Vec2 support_end =
                position + unit.support_steering() * 80.0F;
            if (!draw_world_line(renderer_, transform, position, support_end)) {
                return false;
            }
        }

        if (unit.tactical_position().has_value()) {
            const Vec2 tactical_position = *unit.tactical_position();
            set_color(renderer_, 255, 190, 80, 220);
            if (!draw_world_line(renderer_, transform, position,
                                 tactical_position)) {
                return false;
            }
            if (unit.tactical_order() == TacticalOrder::hold) {
                if (!draw_world_circle(renderer_, transform, tactical_position,
                                       default_tactical_rules
                                           .hold_leash_radius)) {
                    return false;
                }
            } else {
                const float target_size = 10.0F;
                if (!draw_world_line(
                        renderer_, transform,
                        {tactical_position.x - target_size,
                         tactical_position.y},
                        {tactical_position.x + target_size,
                         tactical_position.y}) ||
                    !draw_world_line(
                        renderer_, transform,
                        {tactical_position.x,
                         tactical_position.y - target_size},
                        {tactical_position.x,
                         tactical_position.y + target_size})) {
                    return false;
                }
            }
        }

        set_color(renderer_, 80, 235, 255, 190);
        if (!draw_world_line(renderer_, transform,
                             {position.x - preferred_half_width, unit.preferred_y()},
                             {position.x + preferred_half_width, unit.preferred_y()})) {
            return false;
        }

        if (std::ranges::find(selected_unit_ids, unit.id()) !=
                selected_unit_ids.end() && unit.troop_type() == TroopType::mortar) {
            set_color(renderer_, 255, 156, 72, 205);
            if (!draw_world_circle(renderer_, transform, position,
                                   unit.weapon().minimum_range)) {
                return false;
            }
            set_color(renderer_, 190, 112, 255, 190);
            if (!draw_world_circle(renderer_, transform, position,
                                   mortar_observation_range(world.map()))) {
                return false;
            }
        }

        if (std::ranges::find(selected_unit_ids, unit.id()) !=
                selected_unit_ids.end() &&
            unit.navigation_destination().has_value()) {
            set_color(renderer_, 208, 112, 255, 225);
            Vec2 previous = position;
            for (const Vec2 waypoint : unit.remaining_navigation_waypoints()) {
                if (!draw_world_line(renderer_, transform, previous, waypoint)) {
                    return false;
                }
                previous = waypoint;
            }
            const Vec2 destination =
                *unit.navigation_resolved_destination();
            constexpr float nav_marker_size = 7.0F;
            if (!draw_world_line(renderer_, transform,
                                 {destination.x - nav_marker_size,
                                  destination.y - nav_marker_size},
                                 {destination.x + nav_marker_size,
                                  destination.y + nav_marker_size}) ||
                !draw_world_line(renderer_, transform,
                                 {destination.x - nav_marker_size,
                                  destination.y + nav_marker_size},
                                 {destination.x + nav_marker_size,
                                  destination.y - nav_marker_size})) {
                return false;
            }
        }

    }

    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetRenderOutputSize(renderer_, &output_width, &output_height)) {
        return false;
    }
    const auto deployment_layout =
        ui_layout::deployment_layout(output_width, output_height);
    const float panel_top = panel_margin;
    const float panel_bottom =
        static_cast<float>(output_height) - deployment_layout.bar_height -
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
    const auto controlled_name = team_color_name(controlled_team);
    global.format(FontRole::debug_bold, debug_heading,
                  "controlled team: %.*s",
                  static_cast<int>(controlled_name.size()),
                  controlled_name.data());
    global.format(FontRole::debug, debug_text, "simulation: %.0f Hz",
                  simulation_hz);
    global.format(FontRole::debug, debug_text, "render: %.0f FPS", render_fps);
    global.format(FontRole::debug, debug_text, "units: %zu", world.units().size());
    global.format(FontRole::debug, debug_text, "projectiles: %zu",
                  world.projectiles().size());
    global.format(FontRole::debug, debug_text, "pending: %zu",
                  world.pending_deployments().size());
    global.format(FontRole::debug, debug_text, "selected: %zu",
                  selected_unit_ids.size());
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
    const std::uint64_t income_elapsed_ticks =
        world.match_state().phase_elapsed_ticks();
    const std::size_t income_phase =
        income_phase_index(income_elapsed_ticks);
    const Money phase_base_income =
        base_passive_income(income_elapsed_ticks);
    const Money phase_comeback_per_objective =
        comeback_income_per_objective(income_elapsed_ticks);
    const Money team_a_comeback =
        comeback_income_bonus(world, Team::team_a);
    const Money team_b_comeback =
        comeback_income_bonus(world, Team::team_b);
    global.format(FontRole::debug, debug_text, "income phase: %zu",
                  income_phase + 1);
    global.format(
        FontRole::debug, debug_text, "%s elapsed: %.2fs",
        world.match_state().phase() == MatchPhase::sudden_death
            ? "sudden death"
            : "regulation",
        static_cast<double>(income_elapsed_ticks) /
            default_economy_rules.fixed_ticks_per_second);
    global.format(FontRole::debug, debug_text,
                  "comeback/objective: $%lld/s",
                  static_cast<long long>(phase_comeback_per_objective));
    global.format(FontRole::debug, debug_text,
                  "Team A income: $%lld + $%lld = $%lld/s",
                  static_cast<long long>(phase_base_income),
                  static_cast<long long>(team_a_comeback),
                  static_cast<long long>(effective_passive_income_rate(
                      world, Team::team_a)));
    global.format(FontRole::debug, debug_text,
                  "Team B income: $%lld + $%lld = $%lld/s",
                  static_cast<long long>(phase_base_income),
                  static_cast<long long>(team_b_comeback),
                  static_cast<long long>(effective_passive_income_rate(
                      world, Team::team_b)));
    global.blank();

    global.line(FontRole::debug_bold, debug_heading, "AI COMMANDER");
    const auto ai_status = to_string(ai_commander.status());
    const auto ai_team = team_color_name(ai_commander.team());
    const auto ai_result = to_string(ai_commander.last_result());
    const auto ai_difficulty =
        to_string(ai_commander.profile().difficulty);
    const auto ai_playstyle =
        to_string(ai_commander.profile().playstyle);
    const std::size_t ai_pending = static_cast<std::size_t>(std::count_if(
        world.pending_deployments().begin(), world.pending_deployments().end(),
        [&ai_commander](const auto& deployment) {
            return deployment.team == ai_commander.team();
        }));
    global.format(FontRole::debug, debug_text, "%.*s: %.*s",
                  static_cast<int>(ai_team.size()), ai_team.data(),
                  static_cast<int>(ai_status.size()), ai_status.data());
    global.format(FontRole::debug, debug_text, "profile: %.*s / %.*s",
                  static_cast<int>(ai_difficulty.size()), ai_difficulty.data(),
                  static_cast<int>(ai_playstyle.size()), ai_playstyle.data());
    global.format(FontRole::debug, debug_text, "next: %.2fs",
                  static_cast<double>(
                      ai_commander.ticks_until_next_decision()) /
                      simulation_hz);
    const PlayerState* ai_player = world.find_player(ai_commander.team());
    const Money ai_cash = ai_player == nullptr ? 0 : ai_player->cash();
    if (const auto plan = ai_commander.planned_purchase()) {
        const auto name = troop_display_name(*plan);
        const Money cost = ai_commander.planned_purchase_cost();
        global.format(FontRole::debug, debug_text, "plan: %.*s $%lld",
                      static_cast<int>(name.size()), name.data(),
                      static_cast<long long>(cost));
        global.format(FontRole::debug, debug_text, "cash: $%lld / %s",
                      static_cast<long long>(ai_cash),
                      ai_cash >= cost ? "affordable" : "saving");
        if (const auto reason = ai_commander.planned_purchase_reason()) {
            const auto reason_text = to_string(*reason);
            global.format(FontRole::debug, debug_text,
                          "reason: %.*s%s",
                          static_cast<int>(reason_text.size()),
                          reason_text.data(),
                          ai_commander.emergency_override_active()
                              ? " / EMERGENCY"
                              : "");
        }
    } else {
        global.line(FontRole::debug, debug_muted, "plan: reevaluate");
        global.format(FontRole::debug, debug_text, "cash: $%lld",
                      static_cast<long long>(ai_cash));
    }
    if (const auto troop = ai_commander.last_troop_choice()) {
        const auto name = troop_display_name(*troop);
        global.format(FontRole::debug, debug_text, "last: %.*s / %.*s",
                      static_cast<int>(name.size()), name.data(),
                      static_cast<int>(ai_result.size()), ai_result.data());
    } else {
        global.format(FontRole::debug, debug_text, "last: none / %.*s",
                      static_cast<int>(ai_result.size()), ai_result.data());
    }
    global.format(FontRole::debug, debug_text, "pending: %zu  deployed: %llu",
                  ai_pending, static_cast<unsigned long long>(
                                  ai_commander.successful_deployments()));
    const auto ai_strategy = to_string(ai_commander.strategy());
    if (const auto objective = ai_commander.target_objective()) {
        global.format(FontRole::debug, debug_text, "strategy: %.*s -> Z%zu",
                      static_cast<int>(ai_strategy.size()), ai_strategy.data(),
                      *objective);
    } else {
        global.format(FontRole::debug, debug_text, "strategy: %.*s -> none",
                      static_cast<int>(ai_strategy.size()), ai_strategy.data());
    }
    global.format(FontRole::debug, debug_text, "strength: %d friendly / %d enemy",
                  ai_commander.relevant_friendly_strength(),
                  ai_commander.relevant_enemy_strength());
    if (const auto command = ai_commander.last_tactical_command()) {
        const auto command_name = to_string(*command);
        global.format(FontRole::debug, debug_text, "command: %.*s x%zu",
                      static_cast<int>(command_name.size()),
                      command_name.data(),
                      ai_commander.last_commanded_unit_count());
    } else {
        global.line(FontRole::debug, debug_text, "command: none");
    }
    global.format(FontRole::debug, debug_text, "strategy next: %.2fs",
                  static_cast<double>(
                      ai_commander.ticks_until_next_strategy_evaluation()) /
                      simulation_hz);
    global.blank();

    global.line(FontRole::debug_bold, debug_heading, "REGULATION SCORE");
    global.format(FontRole::debug, debug_text, "Team A score: %lld",
                  static_cast<long long>(team_a_player == nullptr
                                             ? 0
                                             : team_a_player->score()));
    global.format(FontRole::debug, debug_text, "Team B score: %lld",
                  static_cast<long long>(team_b_player == nullptr
                                             ? 0
                                             : team_b_player->score()));
    global.format(FontRole::debug, debug_text, "score tick: %llu / %u",
                  static_cast<unsigned long long>(
                      world.scoring_tick_progress()),
                  default_scoring_rules.interval_ticks);
    global.format(FontRole::debug, debug_text, "interval: %.1fs",
                  static_cast<double>(default_scoring_rules.interval_ticks) /
                      simulation_hz);
    global.blank();

    global.line(FontRole::debug_bold, debug_heading, "MATCH");
    const auto match_phase = to_string(world.match_state().phase());
    const auto match_result = to_string(world.match_state().result());
    global.format(FontRole::debug, debug_text, "state: %.*s",
                  static_cast<int>(match_phase.size()), match_phase.data());
    global.format(FontRole::debug, debug_text, "remaining ticks: %llu",
                  static_cast<unsigned long long>(
                      world.match_state().remaining_ticks()));
    global.format(FontRole::debug, debug_text, "remaining: %.2fs",
                  static_cast<double>(world.match_state().remaining_ticks()) /
                      world.match_state().rules().fixed_ticks_per_second);
    global.format(FontRole::debug, debug_text, "result: %.*s",
                  static_cast<int>(match_result.size()), match_result.data());
    if (world.match_state().phase() == MatchPhase::sudden_death) {
        const std::size_t center_index =
            world.map().center_objective_zone_index;
        if (center_index < world.zones().size()) {
            const Zone& center = world.zones()[center_index];
            global.format(FontRole::debug, debug_text, "center: %s / %+.1f",
                          short_team_name(center.owner()),
                          center.capture_value());
        }
        global.line(FontRole::debug, debug_text,
                    "deployment: home only");
    }
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
                      yes_no(is_zone_deployable(world, zone, Team::team_a)),
                      yes_no(is_zone_deployable(world, zone, Team::team_b)));
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
        const auto order = to_string(unit.tactical_order());
        cursor.format(FontRole::debug, debug_text, "order: %.*s",
                      static_cast<int>(order.size()), order.data());
        cursor.format(FontRole::debug, debug_text, "path: %s (%zu)",
                      yes_no(unit.has_movement_path()),
                      unit.remaining_waypoint_count());
        if (const auto waypoint = unit.current_waypoint()) {
            cursor.format(FontRole::debug, debug_text, "waypoint: %.0f, %.0f",
                          waypoint->x, waypoint->y);
        } else {
            cursor.line(FontRole::debug, debug_muted, "waypoint: none");
        }
        cursor.format(FontRole::debug, debug_text, "nav: %s (%zu)",
                      yes_no(unit.has_navigation_route()),
                      unit.remaining_navigation_waypoint_count());
        if (const auto destination = unit.navigation_destination()) {
            cursor.format(FontRole::debug, debug_text, "nav dest: %.0f, %.0f",
                          destination->x, destination->y);
        } else {
            cursor.line(FontRole::debug, debug_muted, "nav dest: none");
        }
        if (const auto waypoint = unit.current_navigation_waypoint()) {
            cursor.format(FontRole::debug, debug_text, "nav point: %.0f, %.0f",
                          waypoint->x, waypoint->y);
        } else {
            cursor.line(FontRole::debug, debug_muted, "nav point: none");
        }
        cursor.format(FontRole::debug, debug_text, "hp: %.0f/%.0f", unit.health(),
                      unit.max_health());
        const auto category = to_string(unit.target_category());
        cursor.format(FontRole::debug, debug_text, "category: %.*s",
                      static_cast<int>(category.size()), category.data());
        if (unit.target_id().has_value()) {
            const Unit* target = world.find_unit(*unit.target_id());
            if (target != nullptr) {
                const auto target_category = to_string(target->target_category());
                cursor.format(FontRole::debug, debug_text,
                              "target: #%u / %.*s", *unit.target_id(),
                              static_cast<int>(target_category.size()),
                              target_category.data());
            } else {
                cursor.format(FontRole::debug, debug_text, "target: #%u / missing",
                              *unit.target_id());
            }
            if (unit.troop_type() == TroopType::mortar) {
                cursor.line(FontRole::debug, debug_text,
                            "los: ignored (indirect)");
            } else {
                const bool clear = target != nullptr &&
                    environment_line_of_sight_clear(
                        world.map(), unit.position(), target->position());
                cursor.format(FontRole::debug,
                              clear ? debug_text : debug_heading, "los: %s",
                              clear ? "clear" : "blocked");
            }
        } else {
            cursor.line(FontRole::debug, debug_muted, "target: none");
            cursor.line(FontRole::debug, debug_muted,
                        unit.troop_type() == TroopType::mortar
                            ? "los: ignored (indirect)"
                            : "los: none");
        }
        cursor.format(FontRole::debug, debug_text, "pos: %.0f, %.0f",
                      unit.position().x, unit.position().y);
        cursor.format(FontRole::debug, debug_text, "preferred y: %.0f",
                      unit.preferred_y());
        cursor.format(FontRole::debug, debug_text, "move: %.0f",
                      unit.move_speed());
        cursor.format(FontRole::debug, debug_text, "hull: %.0f / %.0fdeg/s",
                      unit.facing_angle(), unit.rotation_speed());
        if (unit.has_independent_turret()) {
            cursor.format(FontRole::debug, debug_text,
                          "turret: %.0f / %.0fdeg/s", unit.turret_angle(),
                          unit.turret_rotation_speed());
        } else {
            cursor.line(FontRole::debug, debug_muted, "turret: n/a");
        }
        if (unit.troop_type() == TroopType::mortar) {
            cursor.format(FontRole::debug, debug_heading,
                          "observation: 2 zones / %.0f",
                          mortar_observation_range(world.map()));
            cursor.format(FontRole::debug, debug_text, "min range: %.0f",
                          unit.weapon().minimum_range);
            cursor.line(FontRole::debug, debug_text,
                        "weapon capability: map-wide");
            cursor.line(FontRole::debug, debug_text,
                        "enemy home: excluded");
        } else {
            cursor.format(FontRole::debug, debug_text,
                          "vision: %.0f / %.0fdeg", unit.vision_range(),
                          unit.vision_angle());
            if (unit.weapon().minimum_range > 0.0F) {
                cursor.format(FontRole::debug, debug_text,
                              "weapon: %.0f..%.0f",
                              unit.weapon().minimum_range,
                              unit.weapon().range);
            } else {
                cursor.format(FontRole::debug, debug_text,
                              "range: %.0f +/-%.0f",
                              unit.preferred_combat_range(),
                              unit.range_tolerance());
            }
            cursor.line(FontRole::debug, debug_muted,
                        "targeting: normal");
            cursor.line(FontRole::debug, debug_muted, "excluded: none");
        }
        cursor.format(FontRole::debug, debug_text, "cooldown: %.2f",
                      unit.weapon_cooldown_remaining());
        cursor.format(FontRole::debug, debug_text, "vs vehicle: x%.1f",
                      unit.weapon().vehicle_damage_multiplier);
        cursor.format(FontRole::debug, debug_text, "emplacement: %s",
                      unit.is_relocating() ? "relocating" : "stationary");
        const auto shell = std::ranges::find_if(
            world.projectiles(), [&unit](const Projectile& projectile) {
                return projectile.source_unit_id() == unit.id() &&
                       projectile.is_indirect();
            });
        if (shell != world.projectiles().end()) {
            cursor.format(FontRole::debug, debug_text,
                          "shell: %.0f,%.0f %.0f%%",
                          shell->impact_position().x,
                          shell->impact_position().y,
                          shell->flight_progress() * 100.0F);
        } else {
            cursor.line(FontRole::debug, debug_muted, "shell: none");
        }
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
