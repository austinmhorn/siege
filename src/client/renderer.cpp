#include "client/renderer.hpp"

#include "client/battlefield_renderer.hpp"
#include "client/capture_bar.hpp"
#include "client/ui_layout.hpp"
#include "client/world_transform.hpp"
#include "core/ai_commander.hpp"
#include "core/deployment.hpp"
#include "core/math.hpp"
#include "core/movement_path.hpp"
#include "core/tactical_command.hpp"
#include "core/troop_definition.hpp"
#include "core/zone_capture.hpp"
#include "world/unit.hpp"
#include "world/world.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
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
constexpr Color team_a_projectile{126, 218, 255, 255};
constexpr Color team_b_projectile{255, 174, 102, 255};
constexpr Color rocket_core{255, 244, 132, 255};
constexpr float projectile_tracer_length = 18.0F;
constexpr double corpse_fade_seconds = 10.0;
constexpr double explosion_effect_seconds = 0.35;
constexpr int explosion_segments = 32;
constexpr float pending_marker_radius = 18.0F;
constexpr double deployment_feedback_duration_seconds = 1.5;
constexpr float capture_bar_world_inset = 42.0F;
constexpr float capture_bar_world_y = 18.0F;
constexpr float capture_bar_world_height = 18.0F;
constexpr int capture_bar_gradient_segments = 48;
constexpr int capture_marker_segments = 20;
constexpr float command_menu_width = 184.0F;
constexpr float command_menu_item_height = 30.0F;
constexpr float command_menu_padding = 4.0F;
constexpr float score_panel_width = 410.0F;
constexpr float score_panel_height = 34.0F;
constexpr float score_panel_y = 42.0F;
constexpr float result_panel_width = 320.0F;
constexpr float result_panel_height = 88.0F;

constexpr std::array<TacticalOrder, 4> tactical_commands{
    TacticalOrder::advance,
    TacticalOrder::hold,
    TacticalOrder::regroup,
    TacticalOrder::automatic,
};

std::string_view command_display_name(const TacticalOrder order) noexcept {
    switch (order) {
    case TacticalOrder::advance:
        return "Advance";
    case TacticalOrder::hold:
        return "Hold Position";
    case TacticalOrder::regroup:
        return "Regroup";
    case TacticalOrder::automatic:
        return "Resume Auto";
    }
    return "Unknown";
}

std::string format_match_time(const std::uint32_t total_seconds) {
    const std::uint32_t minutes = total_seconds / 60;
    const std::uint32_t seconds = total_seconds % 60;
    std::string result = std::to_string(minutes);
    if (result.size() < 2) {
        result.insert(result.begin(), '0');
    }
    result.push_back(':');
    if (seconds < 10) {
        result.push_back('0');
    }
    result += std::to_string(seconds);
    return result;
}

std::string_view match_result_label(const MatchResult result) noexcept {
    switch (result) {
    case MatchResult::team_a:
        return "BLUE WINS";
    case MatchResult::team_b:
        return "RED WINS";
    case MatchResult::none:
        return "";
    }
    return "";
}

struct SoldierVisualLayout {
    float source_pixel_world_size;
    float render_scale;
    float legs_canvas_size;
    Vec2 legs_anchor;
    Vec2 legs_shadow_anchor;
    float death_canvas_size;
};

constexpr SoldierVisualLayout soldier_layout{
    .source_pixel_world_size = 2.0F,
    .render_scale = 0.5F,
    .legs_canvas_size = 32.0F,
    .legs_anchor = {16.0F, 16.0F},
    .legs_shadow_anchor = {16.0F, 16.0F},
    .death_canvas_size = 64.0F,
};

struct TroopVisualDefinition {
    TroopType troop_type;
    WeaponType weapon_type;
    std::string_view layer;
    std::string_view frame_prefix;
    float upper_canvas_size;
    std::size_t non_firing_frame;
    Vec2 body_anchor;
    Vec2 body_shadow_anchor;
    Vec2 firing_body_anchor;
    Vec2 firing_body_shadow_anchor;
    std::span<const std::size_t> firing_frames;
    double firing_seconds_per_frame;
};

constexpr std::array<std::size_t, 9> rifle_firing_frames{9, 8, 7, 6, 5,
                                                         4, 3, 2, 1};
constexpr std::array<std::size_t, 6> machine_gun_firing_frames{11, 12, 13,
                                                               14, 15, 16};
constexpr std::array<std::size_t, 13> bazooka_firing_frames{
    13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1,
};

constexpr TroopVisualDefinition rifle_visual{
    .troop_type = TroopType::rifle,
    .weapon_type = WeaponType::rifle,
    .layer = "rifle",
    .frame_prefix = "rifle",
    .upper_canvas_size = 64.0F,
    .non_firing_frame = 1,
    .body_anchor = {32.0F, 32.0F},
    .body_shadow_anchor = {32.0F, 32.0F},
    .firing_body_anchor = {32.0F, 32.0F},
    .firing_body_shadow_anchor = {32.0F, 32.0F},
    .firing_frames = rifle_firing_frames,
    .firing_seconds_per_frame = 0.04,
};

constexpr TroopVisualDefinition machine_gun_visual{
    .troop_type = TroopType::machine_gun,
    .weapon_type = WeaponType::machine_gun,
    .layer = "machine_gun",
    .frame_prefix = "machine_gun",
    .upper_canvas_size = 128.0F,
    .non_firing_frame = 1,
    .body_anchor = {56.0F, 40.0F},
    .body_shadow_anchor = {56.0F, 40.0F},
    .firing_body_anchor = {64.0F, 64.0F},
    .firing_body_shadow_anchor = {64.0F, 64.0F},
    .firing_frames = machine_gun_firing_frames,
    .firing_seconds_per_frame = 0.025,
};

constexpr TroopVisualDefinition bazooka_visual{
    .troop_type = TroopType::bazooka,
    .weapon_type = WeaponType::bazooka,
    .layer = "bazooka",
    .frame_prefix = "bazooka",
    .upper_canvas_size = 64.0F,
    .non_firing_frame = 1,
    .body_anchor = {30.0F, 26.0F},
    .body_shadow_anchor = {30.0F, 26.0F},
    .firing_body_anchor = {30.0F, 26.0F},
    .firing_body_shadow_anchor = {30.0F, 26.0F},
    .firing_frames = bazooka_firing_frames,
    .firing_seconds_per_frame = 0.06,
};

const TroopVisualDefinition* visual_for(const TroopType troop_type) noexcept {
    switch (troop_type) {
    case TroopType::rifle:
        return &rifle_visual;
    case TroopType::machine_gun:
        return &machine_gun_visual;
    case TroopType::bazooka:
        return &bazooka_visual;
    }
    return nullptr;
}

constexpr std::array<TroopType, 3> purchasable_troops{
    TroopType::rifle, TroopType::machine_gun, TroopType::bazooka};

SDL_FRect deployment_button_rect(const std::size_t index,
                                 const int output_width,
                                 const int output_height) noexcept {
    const float total_width =
        ui_layout::deployment_button_width *
            static_cast<float>(purchasable_troops.size()) +
        ui_layout::deployment_button_gap *
            static_cast<float>(purchasable_troops.size() - 1);
    return SDL_FRect{
        (static_cast<float>(output_width) - total_width) * 0.5F +
            static_cast<float>(index) *
                (ui_layout::deployment_button_width +
                 ui_layout::deployment_button_gap),
        static_cast<float>(output_height) - ui_layout::deployment_bar_height +
            ui_layout::deployment_button_top_inset,
        ui_layout::deployment_button_width,
        ui_layout::deployment_button_height,
    };
}

SDL_FRect command_menu_rect(const Point requested, const int output_width,
                            const int output_height) noexcept {
    const float height = command_menu_padding * 2.0F +
                         command_menu_item_height * tactical_commands.size();
    const float maximum_x =
        std::max(command_menu_padding,
                 static_cast<float>(output_width) - command_menu_width -
                     command_menu_padding);
    const float maximum_y =
        std::max(command_menu_padding,
                 static_cast<float>(output_height) -
                     ui_layout::deployment_bar_height - height -
                     command_menu_padding);
    return SDL_FRect{
        std::clamp(requested.x, command_menu_padding, maximum_x),
        std::clamp(requested.y, command_menu_padding, maximum_y),
        command_menu_width, height};
}

SDL_FRect command_item_rect(const SDL_FRect menu,
                            const std::size_t index) noexcept {
    return SDL_FRect{
        menu.x + command_menu_padding,
        menu.y + command_menu_padding +
            static_cast<float>(index) * command_menu_item_height,
        menu.w - command_menu_padding * 2.0F,
        command_menu_item_height,
    };
}

bool contains(const SDL_FRect& rectangle, const Point point) noexcept {
    return point.x >= rectangle.x && point.x < rectangle.x + rectangle.w &&
           point.y >= rectangle.y && point.y < rectangle.y + rectangle.h;
}

std::optional<Vec2> selection_world_point(SDL_Renderer* renderer,
                                          const MapDefinition& map,
                                          Point drawable,
                                          const bool clamp_to_battlefield) {
    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetRenderOutputSize(renderer, &output_width, &output_height)) {
        return std::nullopt;
    }

    const WorldTransform transform{map.logical_width, map.logical_height,
                                   output_width, output_height};
    const Bounds& viewport = transform.viewport();
    const float right = viewport.x + viewport.width;
    const float bottom = std::min(
        viewport.y + viewport.height,
        static_cast<float>(output_height) - ui_layout::deployment_bar_height);
    if (right < viewport.x || bottom < viewport.y) {
        return std::nullopt;
    }

    const bool inside = drawable.x >= viewport.x && drawable.x <= right &&
                        drawable.y >= viewport.y && drawable.y <= bottom;
    if (!inside && !clamp_to_battlefield) {
        return std::nullopt;
    }
    drawable.x = std::clamp(drawable.x, viewport.x, right);
    drawable.y = std::clamp(drawable.y, viewport.y, bottom);
    const auto world = transform.drawable_to_world(drawable);
    if (!world.has_value()) {
        return std::nullopt;
    }
    return Vec2{world->x, world->y};
}

void set_color(SDL_Renderer* renderer, const Color color) {
    SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, color.alpha);
}

struct TeamVisualVariant {
    std::string_view asset_palette;
    Color modulation;
};

constexpr TeamVisualVariant blue_soldier_variant{
    .asset_palette = "color2",
    .modulation = {255, 255, 255, 255},
};

// The CraftPix pack has green, blue, yellow, and navy authored palettes but no
// red one. Its blue-gray palette has enough neutral luminance to produce a
// readable red team variant through a single centralized texture modulation.
constexpr TeamVisualVariant red_soldier_variant{
    .asset_palette = "color2",
    .modulation = {255, 105, 95, 255},
};

const TeamVisualVariant& team_visual_variant(const Team team) noexcept {
    return team == Team::team_b ? red_soldier_variant : blue_soldier_variant;
}

std::filesystem::path body_frame_path(const Team team,
                                      const std::string_view layer,
                                      const std::string_view prefix,
                                      const std::size_t frame) {
    return std::filesystem::path{"soldiers"} /
           team_visual_variant(team).asset_palette / "soldier1" / layer /
           (std::string{prefix} + std::to_string(frame) + ".png");
}

std::filesystem::path shadow_frame_path(const std::string_view layer,
                                        const std::string_view prefix,
                                        const std::size_t frame) {
    return std::filesystem::path{"soldiers/shared/shadows"} / layer /
           (std::string{prefix} + std::to_string(frame) + ".png");
}

bool render_soldier_layer(SDL_Renderer* renderer, TextureCache& textures,
                          const WorldTransform& transform,
                          const std::filesystem::path& path,
                          const Vec2 position, const float facing,
                          const float canvas_size, const Vec2 anchor,
                          const float opacity = 1.0F,
                          const Color modulation = {255, 255, 255, 255}) {
    SDL_Texture* texture = textures.get(path);
    if (texture == nullptr) {
        return false;
    }
    const bool color_set = SDL_SetTextureColorMod(
        texture, modulation.red, modulation.green, modulation.blue);
    const bool alpha_set = SDL_SetTextureAlphaModFloat(
        texture, std::clamp(opacity, 0.0F, 1.0F));
    if (!color_set || !alpha_set) {
        SDL_SetTextureColorMod(texture, 255, 255, 255);
        SDL_SetTextureAlphaModFloat(texture, 1.0F);
        return false;
    }

    const float world_size = canvas_size * soldier_layout.source_pixel_world_size *
                             soldier_layout.render_scale;
    const float source_pixel_world_size =
        soldier_layout.source_pixel_world_size * soldier_layout.render_scale;
    const Bounds world_bounds{
        position.x - anchor.x * source_pixel_world_size,
        position.y - anchor.y * source_pixel_world_size,
        world_size,
        world_size,
    };
    const auto bounds = transform.world_to_drawable(world_bounds);
    const SDL_FRect destination{bounds.x, bounds.y, bounds.width, bounds.height};
    const SDL_FPoint pivot{destination.w * anchor.x / canvas_size,
                           destination.h * anchor.y / canvas_size};
    const bool rendered = SDL_RenderTextureRotated(
        renderer, texture, nullptr, &destination, facing, &pivot, SDL_FLIP_NONE);
    const bool alpha_restored = SDL_SetTextureAlphaModFloat(texture, 1.0F);
    const bool color_restored = SDL_SetTextureColorMod(texture, 255, 255, 255);
    return rendered && alpha_restored && color_restored;
}

bool render_world_circle(SDL_Renderer* renderer, const WorldTransform& transform,
                         const Vec2 center, const float radius) {
    Vec2 previous = center + direction_from_facing(0.0F) * radius;
    for (int segment = 1; segment <= explosion_segments; ++segment) {
        const float angle = 360.0F * static_cast<float>(segment) /
                            static_cast<float>(explosion_segments);
        const Vec2 current = center + direction_from_facing(angle) * radius;
        const auto from =
            transform.world_to_drawable(Point{previous.x, previous.y});
        const auto to = transform.world_to_drawable(Point{current.x, current.y});
        if (!SDL_RenderLine(renderer, from.x, from.y, to.x, to.y)) {
            return false;
        }
        previous = current;
    }
    return true;
}

Color blend_color(const Color from, const Color to, const float amount) noexcept {
    const auto blend = [amount](const Uint8 left, const Uint8 right) {
        return static_cast<Uint8>(static_cast<float>(left) +
                                  (static_cast<float>(right) - left) * amount);
    };
    return Color{blend(from.red, to.red), blend(from.green, to.green),
                 blend(from.blue, to.blue), blend(from.alpha, to.alpha)};
}

bool render_drawable_circle(SDL_Renderer* renderer, const Point center,
                            const float radius) {
    constexpr float tau = 6.2831853071795864769F;
    Point previous{center.x + radius, center.y};
    for (int segment = 1; segment <= capture_marker_segments; ++segment) {
        const float angle = tau * static_cast<float>(segment) /
                            static_cast<float>(capture_marker_segments);
        const Point current{center.x + std::cos(angle) * radius,
                            center.y + std::sin(angle) * radius};
        if (!SDL_RenderLine(renderer, previous.x, previous.y,
                            current.x, current.y)) {
            return false;
        }
        previous = current;
    }
    return true;
}

bool render_filled_drawable_circle(SDL_Renderer* renderer, const Point center,
                                   const float radius) {
    const int vertical_radius = static_cast<int>(std::ceil(radius));
    for (int y = -vertical_radius; y <= vertical_radius; ++y) {
        const float offset_y = static_cast<float>(y);
        const float width_squared =
            std::max(0.0F, radius * radius - offset_y * offset_y);
        const float half_width = std::sqrt(width_squared);
        if (!SDL_RenderLine(renderer, center.x - half_width,
                            center.y + offset_y, center.x + half_width,
                            center.y + offset_y)) {
            return false;
        }
    }
    return true;
}

bool render_capture_bars(SDL_Renderer* renderer, const World& world,
                         const WorldTransform& transform) {
    constexpr Color blue{60, 145, 245, 255};
    constexpr Color midpoint{225, 229, 232, 255};
    constexpr Color red{235, 82, 82, 255};
    if (!SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND)) {
        return false;
    }
    for (const auto& zone : world.zones()) {
        if (zone.type() != ZoneType::objective) {
            continue;
        }

        const Bounds& zone_bounds = zone.bounds();
        const Bounds world_bar{
            zone_bounds.x + capture_bar_world_inset, capture_bar_world_y,
            zone_bounds.width - capture_bar_world_inset * 2.0F,
            capture_bar_world_height};
        const Bounds draw_bar = transform.world_to_drawable(world_bar);
        const SDL_FRect panel{draw_bar.x - 4.0F, draw_bar.y - 3.0F,
                              draw_bar.width + 8.0F, draw_bar.height + 6.0F};
        set_color(renderer, Color{10, 14, 18, 225});
        if (!SDL_RenderFillRect(renderer, &panel)) {
            return false;
        }

        const float segment_width =
            draw_bar.width / static_cast<float>(capture_bar_gradient_segments);
        for (int segment = 0; segment < capture_bar_gradient_segments; ++segment) {
            const float t = (static_cast<float>(segment) + 0.5F) /
                            static_cast<float>(capture_bar_gradient_segments);
            set_color(renderer, t <= 0.5F
                                    ? blend_color(blue, midpoint, t * 2.0F)
                                    : blend_color(midpoint, red,
                                                  (t - 0.5F) * 2.0F));
            const SDL_FRect stripe{
                draw_bar.x + static_cast<float>(segment) * segment_width,
                draw_bar.y, segment_width + 1.0F, draw_bar.height};
            if (!SDL_RenderFillRect(renderer, &stripe)) {
                return false;
            }
        }

        set_color(renderer, Color{255, 255, 255, 245});
        const float center_x = draw_bar.x + draw_bar.width * 0.5F;
        if (!SDL_RenderLine(renderer, center_x, draw_bar.y - 2.0F,
                            center_x, draw_bar.y + draw_bar.height + 2.0F) ||
            !SDL_RenderRect(renderer, &panel)) {
            return false;
        }
        const float marker_x =
            draw_bar.x + draw_bar.width *
                             capture_bar_fraction(zone.capture_value());
        set_color(renderer, Color{18, 22, 26, 255});
        const Point marker_center{
            marker_x, draw_bar.y + draw_bar.height * 0.5F};
        if (!render_filled_drawable_circle(
                renderer,
                marker_center, 6.0F)) {
            return false;
        }
        set_color(renderer, Color{255, 255, 255, 255});
        if (!render_drawable_circle(renderer, marker_center, 6.0F)) {
            return false;
        }
    }
    return true;
}

bool render_soldier_layer(SDL_Renderer* renderer, TextureCache& textures,
                          const WorldTransform& transform,
                          const std::filesystem::path& path,
                          const Vec2 position, const float facing,
                          const float canvas_size, const float opacity = 1.0F,
                          const Color modulation = {255, 255, 255, 255}) {
    const Vec2 centered_anchor{canvas_size * 0.5F, canvas_size * 0.5F};
    return render_soldier_layer(renderer, textures, transform, path, position,
                                facing, canvas_size, centered_anchor, opacity,
                                modulation);
}

} // namespace

Renderer::Renderer(SDL_Renderer* renderer, std::filesystem::path asset_root)
    : renderer_(renderer), textures_(renderer, std::move(asset_root)),
      fonts_(renderer, textures_.asset_root()), debug_renderer_(renderer, fonts_) {}

void Renderer::update(const World& world, const double fixed_delta_seconds) {
    const MatchPhase match_phase = world.match_state().phase();
    if (match_phase == MatchPhase::sudden_death &&
        observed_match_phase_ != MatchPhase::sudden_death) {
        clear_local_interaction_state();
        leg_animations_.clear();
        firing_animations_.clear();
        corpses_.clear();
        explosions_.clear();
    }
    observed_match_phase_ = match_phase;
    const bool match_active = world.match_state().active();
    if (!match_active) {
        selection_drag_.reset();
        path_drawing_.reset();
        command_menu_position_.reset();
        selected_troop_.reset();
        deployment_feedback_ = DeploymentFeedback::none;
        deployment_feedback_seconds_ = 0.0;
    }
    selection_.prune(world, local_control_.team());
    if (selection_.size() == 0) {
        command_menu_position_.reset();
    }
    if (path_drawing_.has_value()) {
        const Unit* unit = world.find_unit(path_drawing_->unit_id);
        if (unit == nullptr || !unit->is_alive() ||
            unit->team() != local_control_.team()) {
            path_drawing_.reset();
        }
    }
    deployment_feedback_seconds_ =
        std::max(0.0, deployment_feedback_seconds_ - fixed_delta_seconds);
    if (deployment_feedback_seconds_ <= 0.0) {
        deployment_feedback_ = DeploymentFeedback::none;
    }

    for (const auto& unit : world.units()) {
        auto [entry, inserted] = leg_animations_.try_emplace(
            unit.id(), std::vector<std::size_t>{1, 2, 3, 4, 5, 6, 7}, 0.10, true);
        static_cast<void>(inserted);
        if (!match_active) {
            continue;
        }
        if (unit.movement_state() == MovementState::moving) {
            entry->second.update(fixed_delta_seconds);
        } else {
            entry->second.reset();
        }
    }

    std::erase_if(leg_animations_, [&world](const auto& entry) {
        return world.find_unit(entry.first) == nullptr;
    });

    for (const auto& event : world.fire_events()) {
        const TroopVisualDefinition* visual = visual_for(event.troop_type);
        if (visual == nullptr || event.weapon_type != visual->weapon_type) {
            continue;
        }

        auto [animation, inserted] = firing_animations_.try_emplace(
            event.unit_id,
            std::vector<std::size_t>{visual->firing_frames.begin(),
                                     visual->firing_frames.end()},
            visual->firing_seconds_per_frame, false);
        if (!inserted) {
            animation->second.reset();
        }
    }
    if (match_active) {
        for (auto& [unit_id, animation] : firing_animations_) {
            static_cast<void>(unit_id);
            animation.update(fixed_delta_seconds);
        }
        std::erase_if(firing_animations_, [&world](const auto& entry) {
            return entry.second.finished() ||
                   world.find_unit(entry.first) == nullptr;
        });
    }

    for (const auto& event : world.death_events()) {
        corpses_.push_back(CorpseVisual{event});
    }
    if (match_active) {
        for (auto& corpse : corpses_) {
            corpse.animation.update(fixed_delta_seconds);
            if (corpse.animation.finished()) {
                corpse.fade_elapsed += fixed_delta_seconds;
            }
        }
        std::erase_if(corpses_, [](const CorpseVisual& corpse) {
            return corpse.fade_elapsed >= corpse_fade_seconds;
        });
    }

    for (const auto& event : world.explosion_events()) {
        explosions_.push_back(ExplosionVisual{event});
    }
    if (match_active) {
        for (auto& explosion : explosions_) {
            explosion.elapsed += fixed_delta_seconds;
        }
        std::erase_if(explosions_, [](const ExplosionVisual& explosion) {
            return explosion.elapsed >= explosion_effect_seconds;
        });
    }
}

void Renderer::toggle_debug_overlay() noexcept {
    debug_overlay_enabled_ = !debug_overlay_enabled_;
}

void Renderer::toggle_controlled_team() noexcept {
    (void)local_control_.toggle();
    clear_local_interaction_state();
}

Team Renderer::controlled_team() const noexcept {
    return local_control_.team();
}

void Renderer::clear_local_interaction_state() noexcept {
    selection_.clear();
    selection_drag_.reset();
    path_drawing_.reset();
    command_menu_position_.reset();
    selected_troop_.reset();
    deployment_feedback_ = DeploymentFeedback::none;
    deployment_feedback_seconds_ = 0.0;
}

bool Renderer::cancel_placement() noexcept {
    const bool was_active = selected_troop_.has_value();
    selected_troop_.reset();
    deployment_feedback_ = DeploymentFeedback::none;
    deployment_feedback_seconds_ = 0.0;
    return was_active;
}

void Renderer::set_pointer_position(const World& world,
                                    const float drawable_x,
                                    const float drawable_y) {
    pointer_drawable_ = Point{drawable_x, drawable_y};
    if (selection_drag_.has_value()) {
        selection_drag_->drawable_current = pointer_drawable_;
        if (const auto world_point = selection_world_point(
                renderer_, world.map(), pointer_drawable_, true)) {
            selection_drag_->current = *world_point;
        }
    }
    if (path_drawing_.has_value()) {
        if (const auto world_point = selection_world_point(
                renderer_, world.map(), pointer_drawable_, true)) {
            path_drawing_->current = *world_point;
            (void)append_path_sample(path_drawing_->sampled_points,
                                     path_drawing_->origin, *world_point, false);
        }
    }
}

void Renderer::handle_primary_pointer_press(World& world,
                                            const float drawable_x,
                                            const float drawable_y) {
    if (selection_drag_.has_value()) {
        return;
    }
    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetRenderOutputSize(renderer_, &output_width, &output_height)) {
        return;
    }

    const Point click{drawable_x, drawable_y};
    if (command_menu_position_.has_value()) {
        const SDL_FRect menu = command_menu_rect(
            *command_menu_position_, output_width, output_height);
        for (std::size_t index = 0; index < tactical_commands.size(); ++index) {
            if (contains(command_item_rect(menu, index), click)) {
                selection_.prune(world, local_control_.team());
                (void)apply_tactical_order(world, selection_.ids(),
                                           tactical_commands[index],
                                           local_control_.team());
                command_menu_position_.reset();
                return;
            }
        }
        command_menu_position_.reset();
        return;
    }
    for (std::size_t index = 0; index < purchasable_troops.size(); ++index) {
        if (contains(deployment_button_rect(index, output_width, output_height),
                     click)) {
            selected_troop_ = purchasable_troops[index];
            deployment_feedback_ = DeploymentFeedback::none;
            deployment_feedback_seconds_ = 0.0;
            return;
        }
    }

    if (click.y >=
        static_cast<float>(output_height) - ui_layout::deployment_bar_height) {
        return;
    }

    const WorldTransform transform{world.map().logical_width,
                                   world.map().logical_height, output_width,
                                   output_height};
    const auto world_point = transform.drawable_to_world(click);
    if (!selected_troop_.has_value()) {
        if (!world_point.has_value()) {
            return;
        }
        const auto unit_id = pick_path_unit(world, local_control_.team(),
                                            Vec2{world_point->x, world_point->y});
        if (!should_begin_individual_path(PointerDispatch::primary, false,
                                          unit_id.has_value())) {
            return;
        }
        const Unit* unit = world.find_unit(*unit_id);
        path_drawing_ = PathDrawing{*unit_id, unit->position(), unit->position(), {}};
        path_drawing_->sampled_points.reserve(32);
        return;
    }
    if (!world_point.has_value()) {
        deployment_feedback_ = DeploymentFeedback::invalid_location;
        deployment_feedback_seconds_ = deployment_feedback_duration_seconds;
        return;
    }
    const DeploymentResult result = request_deployment(
        world, local_control_.team(), *selected_troop_,
        Vec2{world_point->x, world_point->y});
    if (result == DeploymentResult::accepted) {
        selected_troop_.reset();
        deployment_feedback_ = DeploymentFeedback::none;
        deployment_feedback_seconds_ = 0.0;
    } else {
        deployment_feedback_ =
            result == DeploymentResult::insufficient_cash
                ? DeploymentFeedback::insufficient_cash
                : DeploymentFeedback::invalid_location;
        deployment_feedback_seconds_ = deployment_feedback_duration_seconds;
    }
}

void Renderer::handle_primary_pointer_release(World& world,
                                              const float drawable_x,
                                              const float drawable_y) {
    if (!path_drawing_.has_value()) {
        return;
    }
    if (const auto endpoint = selection_world_point(
            renderer_, world.map(), Point{drawable_x, drawable_y}, true)) {
        path_drawing_->current = *endpoint;
        (void)append_path_sample(path_drawing_->sampled_points,
                                 path_drawing_->origin, *endpoint, true);
    }
    Unit* unit = world.find_unit(path_drawing_->unit_id);
    if (unit != nullptr && unit->is_alive() &&
        unit->team() == local_control_.team() &&
        !path_drawing_->sampled_points.empty()) {
        (void)assign_movement_path(
            world, unit->id(), std::move(path_drawing_->sampled_points),
            local_control_.team());
    }
    path_drawing_.reset();
}

void Renderer::handle_secondary_pointer_press(const World& world,
                                              const float drawable_x,
                                              const float drawable_y) {
    if (selected_troop_.has_value()) {
        return;
    }
    const auto world_point = selection_world_point(
        renderer_, world.map(), Point{drawable_x, drawable_y}, false);
    if (!world_point.has_value()) {
        return;
    }
    const Point drawable{drawable_x, drawable_y};
    selection_drag_ =
        SelectionDrag{*world_point, *world_point, drawable, drawable};
}

void Renderer::handle_secondary_pointer_release(World& world,
                                                const float drawable_x,
                                                const float drawable_y) {
    if (!selection_drag_.has_value()) {
        return;
    }
    const Point drawable_release{drawable_x, drawable_y};
    selection_drag_->drawable_current = drawable_release;
    if (const auto release = selection_world_point(
            renderer_, world.map(), Point{drawable_x, drawable_y}, true)) {
        selection_drag_->current = *release;
    }
    if (classify_secondary_gesture(selection_drag_->drawable_start,
                                   selection_drag_->drawable_current) ==
        SecondaryGesture::drag) {
        selection_.replace_from_rectangle(world, local_control_.team(),
                                          selection_drag_->start,
                                          selection_drag_->current);
        command_menu_position_.reset();
    } else {
        selection_.prune(world, local_control_.team());
        command_menu_position_ = selection_.size() > 0
            ? std::optional<Point>{drawable_release}
            : std::nullopt;
    }
    selection_drag_.reset();
}

bool Renderer::render(const World& world, const AiCommander& ai_commander,
                      const double interpolation_alpha,
                      const double render_fps) const {
    fonts_.begin_frame();
    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetRenderOutputSize(renderer_, &output_width, &output_height)) {
        return false;
    }

    const WorldTransform transform{world.map().logical_width,
                                   world.map().logical_height, output_width,
                                   output_height};

    set_color(renderer_, letterbox);
    if (!SDL_RenderClear(renderer_)) {
        return false;
    }

    if (!battlefield_renderer_.render(renderer_, textures_, world, transform)) {
        return false;
    }

    if (selected_troop_.has_value()) {
        if (!SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND)) {
            return false;
        }
        for (const auto& zone : world.zones()) {
            const auto deployment =
                deployment_bounds(world, zone, local_control_.team());
            if (!deployment.has_value()) {
                continue;
            }
            const Bounds draw_bounds = transform.world_to_drawable(*deployment);
            const SDL_FRect rectangle{draw_bounds.x, draw_bounds.y,
                                      draw_bounds.width, draw_bounds.height};
            set_color(renderer_, Color{76, 220, 126, 42});
            if (!SDL_RenderFillRect(renderer_, &rectangle)) {
                return false;
            }
            set_color(renderer_, Color{105, 255, 155, 210});
            if (!SDL_RenderRect(renderer_, &rectangle)) {
                return false;
            }
        }
    }

    if (!render_capture_bars(renderer_, world, transform)) {
        return false;
    }

    if (!render_corpses(transform) ||
        !render_projectiles(world, transform, interpolation_alpha) ||
        !render_explosions(transform) ||
        !render_movement_paths(world, transform, interpolation_alpha) ||
        !render_units(world, transform, interpolation_alpha) ||
        !render_pending_deployments(world, transform) ||
        !render_selection(world, transform, interpolation_alpha)) {
        return false;
    }

    if (!render_match_hud(world, output_width)) {
        return false;
    }

    if (debug_overlay_enabled_ &&
        !debug_renderer_.render(world, transform, render_fps, 60.0,
                                corpses_.size(), firing_animations_.size(),
                                explosions_.size(), selection_.size(),
                                local_control_.team(), ai_commander)) {
        return false;
    }

    if (!render_deployment_ui(world, transform, output_width, output_height)) {
        return false;
    }

    if (!render_command_menu(output_width, output_height)) {
        return false;
    }

    if (!render_match_result(world, output_width, output_height)) {
        return false;
    }

    return SDL_RenderPresent(renderer_);
}

bool Renderer::render_match_hud(const World& world,
                                const int output_width) const {
    const PlayerState* team_a_player = world.find_player(Team::team_a);
    const PlayerState* team_b_player = world.find_player(Team::team_b);
    const Score team_a_score = team_a_player == nullptr ? 0 : team_a_player->score();
    const Score team_b_score = team_b_player == nullptr ? 0 : team_b_player->score();
    const std::string timer = world.match_state().phase() ==
                                      MatchPhase::sudden_death
                                  ? "SUDDEN DEATH"
                                  : format_match_time(
                                        world.match_state()
                                            .remaining_display_seconds());
    const float panel_x =
        (static_cast<float>(output_width) - score_panel_width) * 0.5F;
    const SDL_FRect panel{panel_x, score_panel_y, score_panel_width,
                          score_panel_height};
    if (!SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND)) {
        return false;
    }
    set_color(renderer_, Color{10, 14, 18, 205});
    if (!SDL_RenderFillRect(renderer_, &panel)) {
        return false;
    }
    set_color(renderer_, Color{190, 198, 204, 225});
    if (!SDL_RenderRect(renderer_, &panel)) {
        return false;
    }

    const auto draw_centered = [&](const float center_x,
                                   const std::string& text,
                                   const FontColor color) {
        float width = 0.0F;
        float height = 0.0F;
        if (!fonts_.measure(text, FontRole::heading_bold, width, height)) {
            return false;
        }
        return fonts_.draw(center_x - width * 0.5F,
                           panel.y + (panel.h - height) * 0.5F, text,
                           FontRole::heading_bold, color);
    };

    return draw_centered(panel.x + 42.0F, "BLUE",
                         FontColor{116, 194, 255, 255}) &&
           draw_centered(panel.x + 120.0F, std::to_string(team_a_score),
                         FontColor{245, 248, 250, 255}) &&
           draw_centered(panel.x + 205.0F, timer,
                         FontColor{255, 221, 105, 255}) &&
           draw_centered(panel.x + 290.0F, std::to_string(team_b_score),
                         FontColor{245, 248, 250, 255}) &&
           draw_centered(panel.x + 368.0F, "RED",
                         FontColor{255, 132, 122, 255});
}

bool Renderer::render_match_result(const World& world, const int output_width,
                                   const int output_height) const {
    if (world.match_state().active()) {
        return true;
    }
    const std::string_view label =
        match_result_label(world.match_state().result());
    const SDL_FRect panel{
        (static_cast<float>(output_width) - result_panel_width) * 0.5F,
        (static_cast<float>(output_height) - result_panel_height) * 0.5F,
        result_panel_width, result_panel_height};
    if (!SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND)) {
        return false;
    }
    set_color(renderer_, Color{8, 11, 15, 225});
    if (!SDL_RenderFillRect(renderer_, &panel)) {
        return false;
    }
    set_color(renderer_, Color{235, 240, 244, 255});
    if (!SDL_RenderRect(renderer_, &panel)) {
        return false;
    }
    float width = 0.0F;
    float height = 0.0F;
    if (!fonts_.measure(label, FontRole::heading_bold, width, height)) {
        return false;
    }
    return fonts_.draw(panel.x + (panel.w - width) * 0.5F,
                       panel.y + (panel.h - height) * 0.5F, label,
                       FontRole::heading_bold,
                       FontColor{255, 255, 255, 255});
}

bool Renderer::render_movement_paths(
    const World& world, const WorldTransform& transform,
    const double interpolation_alpha) const {
    if (!SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND)) {
        return false;
    }

    const auto render_path = [&](const Vec2 start,
                                 const std::span<const Vec2> waypoints,
                                 const std::optional<Vec2> cursor) {
        std::vector<SDL_FPoint> points;
        points.reserve(1 + waypoints.size() + (cursor.has_value() ? 1 : 0));
        const auto add_point = [&](const Vec2 world_point) {
            const Point drawable = transform.world_to_drawable(
                Point{world_point.x, world_point.y});
            points.push_back(SDL_FPoint{drawable.x, drawable.y});
        };
        add_point(start);
        for (const Vec2 waypoint : waypoints) {
            add_point(waypoint);
        }
        if (cursor.has_value() &&
            (waypoints.empty() || length_squared(*cursor - waypoints.back()) > 0.0001F)) {
            add_point(*cursor);
        }
        if (points.size() >= 2) {
            set_color(renderer_, Color{95, 206, 255, 185});
            if (!SDL_RenderLines(renderer_, points.data(),
                                 static_cast<int>(points.size()))) {
                return false;
            }
        }
        if (points.size() >= 2) {
            set_color(renderer_, Color{210, 245, 255, 235});
            const Point endpoint{points.back().x, points.back().y};
            if (!render_drawable_circle(renderer_, endpoint, 5.0F)) {
                return false;
            }
        }
        return true;
    };

    const float alpha = static_cast<float>(interpolation_alpha);
    for (const auto& unit : world.units()) {
        if (!unit.has_movement_path()) {
            continue;
        }
        const Vec2 start = lerp(unit.previous_position(), unit.position(), alpha);
        if (!render_path(start, unit.remaining_waypoints(), std::nullopt)) {
            return false;
        }
    }

    if (path_drawing_.has_value()) {
        const Unit* unit = world.find_unit(path_drawing_->unit_id);
        if (unit != nullptr) {
            const Vec2 start = lerp(unit->previous_position(), unit->position(), alpha);
            if (!render_path(start, path_drawing_->sampled_points,
                             path_drawing_->current)) {
                return false;
            }
        }
    }
    return true;
}

bool Renderer::render_selection(const World& world,
                                const WorldTransform& transform,
                                const double interpolation_alpha) const {
    if (!SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND)) {
        return false;
    }

    const float alpha = static_cast<float>(interpolation_alpha);
    const float ring_radius = std::max(12.0F, 31.0F * transform.scale());
    for (const Unit::Id id : selection_.ids()) {
        const Unit* unit = world.find_unit(id);
        if (unit == nullptr || !unit->is_alive() ||
            unit->team() != local_control_.team()) {
            continue;
        }
        const Vec2 position =
            lerp(unit->previous_position(), unit->position(), alpha);
        const Point center = transform.world_to_drawable(
            Point{position.x, position.y});
        set_color(renderer_, Color{78, 178, 255, 120});
        if (!render_drawable_circle(renderer_, center, ring_radius + 2.0F)) {
            return false;
        }
        set_color(renderer_, Color{160, 224, 255, 245});
        if (!render_drawable_circle(renderer_, center, ring_radius)) {
            return false;
        }
    }

    if (!selection_drag_.has_value()) {
        return true;
    }
    const Point first = transform.world_to_drawable(
        Point{selection_drag_->start.x, selection_drag_->start.y});
    const Point second = transform.world_to_drawable(
        Point{selection_drag_->current.x, selection_drag_->current.y});
    const SDL_FRect rectangle{
        std::min(first.x, second.x), std::min(first.y, second.y),
        std::abs(second.x - first.x), std::abs(second.y - first.y)};
    set_color(renderer_, Color{70, 155, 255, 42});
    if (!SDL_RenderFillRect(renderer_, &rectangle)) {
        return false;
    }
    set_color(renderer_, Color{135, 213, 255, 245});
    return SDL_RenderRect(renderer_, &rectangle);
}

bool Renderer::render_pending_deployments(
    const World& world, const WorldTransform& transform) const {
    if (!SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND)) {
        return false;
    }
    for (const auto& deployment : world.pending_deployments()) {
        const float progress = deployment.total_seconds <= 0.0
            ? 1.0F
            : static_cast<float>(std::clamp(
                  1.0 - deployment.remaining_seconds / deployment.total_seconds,
                  0.0, 1.0));
        set_color(renderer_, Color{116, 240, 162, 225});
        if (!render_world_circle(renderer_, transform, deployment.position,
                                 pending_marker_radius)) {
            return false;
        }
        const auto marker = transform.world_to_drawable(
            Point{deployment.position.x, deployment.position.y});
        const float width = 42.0F * transform.scale();
        const SDL_FRect background{marker.x - width * 0.5F,
                                   marker.y + 22.0F * transform.scale(), width,
                                   std::max(3.0F, 6.0F * transform.scale())};
        set_color(renderer_, Color{22, 28, 32, 220});
        if (!SDL_RenderFillRect(renderer_, &background)) {
            return false;
        }
        SDL_FRect fill = background;
        fill.w *= progress;
        set_color(renderer_, Color{105, 255, 155, 240});
        if (!SDL_RenderFillRect(renderer_, &fill)) {
            return false;
        }
        const auto troop = troop_display_name(deployment.troop_type);
        if (!fonts_.draw_format(
                marker.x - 30.0F, marker.y - 30.0F * transform.scale(),
                FontRole::debug_bold, FontColor{245, 250, 247, 255},
                "%.*s %.1fs",
                static_cast<int>(troop.size()), troop.data(),
                deployment.remaining_seconds)) {
            return false;
        }
    }
    return true;
}

bool Renderer::render_deployment_ui(const World& world,
                                    const WorldTransform& transform,
                                    const int output_width,
                                    const int output_height) const {
    if (!SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND)) {
        return false;
    }

    const SDL_FRect bar{0.0F,
                        static_cast<float>(output_height) -
                            ui_layout::deployment_bar_height,
                        static_cast<float>(output_width),
                        ui_layout::deployment_bar_height};
    set_color(renderer_, Color{10, 14, 18, 225});
    if (!SDL_RenderFillRect(renderer_, &bar)) {
        return false;
    }

    const PlayerState* player = world.find_player(local_control_.team());
    for (std::size_t index = 0; index < purchasable_troops.size(); ++index) {
        const TroopType troop = purchasable_troops[index];
        const TroopDefinition* definition = troop_definition_for(troop);
        if (definition == nullptr) {
            continue;
        }
        const SDL_FRect button =
            deployment_button_rect(index, output_width, output_height);
        const bool selected = selected_troop_ == troop;
        const bool affordable =
            player != nullptr && player->can_afford(definition->purchase_cost);
        set_color(renderer_, selected
                                 ? Color{58, 134, 88, 245}
                                 : affordable ? Color{48, 57, 65, 245}
                                              : Color{55, 42, 42, 245});
        if (!SDL_RenderFillRect(renderer_, &button)) {
            return false;
        }
        set_color(renderer_, selected ? Color{130, 255, 168, 255}
                                      : Color{170, 180, 188, 255});
        if (!SDL_RenderRect(renderer_, &button)) {
            return false;
        }
        const auto name = troop_display_name(troop);
        if (!fonts_.draw_format(
                button.x + 12.0F, button.y + 7.0F, FontRole::body_bold,
                FontColor{245, 245, 245, 255}, "%.*s",
                static_cast<int>(name.size()), name.data()) ||
            !fonts_.draw_format(
                button.x + 12.0F, button.y + 25.0F, FontRole::debug,
                FontColor{245, 245, 245, 255}, "$%lld",
                static_cast<long long>(definition->purchase_cost)) ||
            !fonts_.draw_format(
                button.x + 12.0F, button.y + 41.0F, FontRole::debug,
                FontColor{232, 236, 238, 255},
                "deploy %.2fs",
                definition->deployment_seconds)) {
            return false;
        }
        if (selected) {
            const SDL_FRect inner{button.x + 2.0F, button.y + 2.0F,
                                  button.w - 4.0F, button.h - 4.0F};
            set_color(renderer_, Color{130, 255, 168, 255});
            if (!SDL_RenderRect(renderer_, &inner)) {
                return false;
            }
        }
    }

    if (deployment_feedback_ != DeploymentFeedback::none) {
        const bool insufficient =
            deployment_feedback_ == DeploymentFeedback::insufficient_cash;
        const FontColor feedback_color =
            insufficient ? FontColor{255, 204, 92, 255}
                         : FontColor{255, 112, 112, 255};
        if (!fonts_.draw(
                14.0F,
                static_cast<float>(output_height) -
                    ui_layout::deployment_bar_height -
                    18.0F,
                insufficient ? "Insufficient cash"
                             : "Invalid deployment location",
                FontRole::heading_bold, feedback_color)) {
            return false;
        }
    }

    if (!selected_troop_.has_value()) {
        return true;
    }
    const auto world_point = transform.drawable_to_world(pointer_drawable_);
    if (!world_point.has_value()) {
        return true;
    }
    const Vec2 position{world_point->x, world_point->y};
    const bool valid =
        is_valid_deployment_location(world, local_control_.team(), position);
    set_color(renderer_, valid ? Color{120, 255, 160, 235}
                               : Color{255, 100, 100, 235});
    const auto marker = transform.world_to_drawable(*world_point);
    constexpr float cursor_size = 10.0F;
    return SDL_RenderLine(renderer_, marker.x - cursor_size, marker.y,
                          marker.x + cursor_size, marker.y) &&
           SDL_RenderLine(renderer_, marker.x, marker.y - cursor_size,
                          marker.x, marker.y + cursor_size);
}

bool Renderer::render_command_menu(const int output_width,
                                   const int output_height) const {
    if (!command_menu_position_.has_value()) {
        return true;
    }
    if (!SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND)) {
        return false;
    }
    const SDL_FRect menu = command_menu_rect(
        *command_menu_position_, output_width, output_height);
    set_color(renderer_, Color{10, 14, 18, 245});
    if (!SDL_RenderFillRect(renderer_, &menu)) {
        return false;
    }
    for (std::size_t index = 0; index < tactical_commands.size(); ++index) {
        const SDL_FRect item = command_item_rect(menu, index);
        const bool hovered = contains(item, pointer_drawable_);
        set_color(renderer_, hovered ? Color{60, 91, 120, 245}
                                     : Color{34, 42, 50, 245});
        if (!SDL_RenderFillRect(renderer_, &item)) {
            return false;
        }
        set_color(renderer_, Color{145, 165, 180, 255});
        if (!SDL_RenderRect(renderer_, &item)) {
            return false;
        }
        const std::string_view label =
            command_display_name(tactical_commands[index]);
        if (!fonts_.draw(item.x + 10.0F, item.y + 8.0F, label,
                         FontRole::body_bold,
                         FontColor{245, 245, 245, 255})) {
            return false;
        }
    }
    set_color(renderer_, Color{225, 232, 238, 255});
    return SDL_RenderRect(renderer_, &menu);
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

        const Color trail_color = projectile.team() == Team::team_a
                                      ? team_a_projectile
                                      : team_b_projectile;
        set_color(renderer_, trail_color);
        if (!SDL_RenderLine(renderer_, draw_trail.x, draw_trail.y,
                            draw_position.x, draw_position.y)) {
            return false;
        }
        if (projectile.weapon_type() == WeaponType::bazooka) {
            set_color(renderer_, rocket_core);
            if (!SDL_RenderPoint(renderer_, draw_position.x, draw_position.y) ||
                !SDL_RenderPoint(renderer_, draw_position.x - 1.0F, draw_position.y) ||
                !SDL_RenderPoint(renderer_, draw_position.x + 1.0F, draw_position.y) ||
                !SDL_RenderPoint(renderer_, draw_position.x, draw_position.y - 1.0F) ||
                !SDL_RenderPoint(renderer_, draw_position.x, draw_position.y + 1.0F)) {
                return false;
            }
        } else if (!SDL_RenderPoint(renderer_, draw_position.x, draw_position.y)) {
            return false;
        }
    }
    return true;
}

bool Renderer::render_explosions(const WorldTransform& transform) const {
    if (!SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND)) {
        return false;
    }
    for (const auto& effect : explosions_) {
        const float progress = static_cast<float>(std::clamp(
            effect.elapsed / explosion_effect_seconds, 0.0, 1.0));
        const float radius = effect.explosion.radius * (0.25F + 0.75F * progress);
        set_color(renderer_, Color{255, 194, 74,
                                   static_cast<Uint8>(220.0F * (1.0F - progress))});
        if (!render_world_circle(renderer_, transform, effect.explosion.position,
                                 radius)) {
            return false;
        }
    }
    return true;
}

bool Renderer::render_corpses(const WorldTransform& transform) const {
    for (const auto& corpse : corpses_) {
        if (visual_for(corpse.death.troop_type) == nullptr) {
            continue;
        }
        const std::size_t frame = corpse.animation.current_frame();
        const float opacity = static_cast<float>(
            1.0 - std::clamp(corpse.fade_elapsed / corpse_fade_seconds, 0.0, 1.0));
        if (!render_soldier_layer(
                renderer_, textures_, transform,
                shadow_frame_path("death1", "death1_", frame),
                corpse.death.position, corpse.death.facing_angle,
                soldier_layout.death_canvas_size, opacity) ||
            !render_soldier_layer(
                renderer_, textures_, transform,
                body_frame_path(corpse.death.team, "death1", "death1_", frame),
                corpse.death.position, corpse.death.facing_angle,
                soldier_layout.death_canvas_size, opacity,
                team_visual_variant(corpse.death.team).modulation)) {
            return false;
        }
    }
    return true;
}

bool Renderer::render_units(const World& world, const WorldTransform& transform,
                            const double interpolation_alpha) const {
    for (const auto& unit : world.units()) {
        const TroopVisualDefinition* visual = visual_for(unit.troop_type());
        if (visual == nullptr) {
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
        std::size_t upper_frame = visual->non_firing_frame;
        bool firing = false;
        if (const auto animation = firing_animations_.find(unit.id());
            animation != firing_animations_.end()) {
            upper_frame = animation->second.current_frame();
            firing = true;
        }

        const Vec2 body_anchor =
            firing ? visual->firing_body_anchor : visual->body_anchor;
        const Vec2 body_shadow_anchor =
            firing ? visual->firing_body_shadow_anchor
                   : visual->body_shadow_anchor;

        struct Layer {
            std::filesystem::path path;
            float canvas_size;
            Vec2 anchor;
            Color modulation;
        };
        constexpr Color no_modulation{255, 255, 255, 255};
        const Color team_modulation =
            team_visual_variant(unit.team()).modulation;
        const std::array layers{
            Layer{shadow_frame_path("legs", "legs", leg_frame),
                  soldier_layout.legs_canvas_size,
                  soldier_layout.legs_shadow_anchor, no_modulation},
            Layer{shadow_frame_path(visual->layer, visual->frame_prefix,
                                    upper_frame),
                  visual->upper_canvas_size, body_shadow_anchor,
                  no_modulation},
            Layer{body_frame_path(unit.team(), "legs", "legs", leg_frame),
                  soldier_layout.legs_canvas_size, soldier_layout.legs_anchor,
                  team_modulation},
            Layer{body_frame_path(unit.team(), visual->layer,
                                  visual->frame_prefix, upper_frame),
                  visual->upper_canvas_size, body_anchor, team_modulation},
        };

        for (const auto& [path, canvas_size, anchor, modulation] : layers) {
            if (!render_soldier_layer(renderer_, textures_, transform, path,
                                      position, facing, canvas_size, anchor,
                                      1.0F, modulation)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace siege
