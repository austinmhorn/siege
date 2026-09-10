#pragma once

#include "client/frame_animation.hpp"
#include "client/texture_cache.hpp"
#include "client/debug_renderer.hpp"
#include "client/world_transform.hpp"
#include "world/combat_event.hpp"

#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>

struct SDL_Renderer;

namespace siege {

class World;

class Renderer {
public:
    Renderer(SDL_Renderer* renderer, std::filesystem::path asset_root);

    void update(const World& world, double fixed_delta_seconds);
    void toggle_debug_overlay() noexcept;
    [[nodiscard]] bool cancel_placement() noexcept;
    void set_pointer_position(float drawable_x, float drawable_y) noexcept;
    void handle_left_click(World& world, float drawable_x, float drawable_y);
    [[nodiscard]] bool render(const World& world, double interpolation_alpha,
                              double render_fps) const;

private:
    struct CorpseVisual {
        DeathEvent death;
        FrameAnimation animation{{1, 2, 3, 4}, 0.12, false};
        double fade_elapsed{};
    };

    struct ExplosionVisual {
        ExplosionEvent explosion;
        double elapsed{};
    };

    [[nodiscard]] bool render_units(const World& world,
                                    const class WorldTransform& transform,
                                    double interpolation_alpha) const;
    [[nodiscard]] bool render_projectiles(const World& world,
                                          const class WorldTransform& transform,
                                          double interpolation_alpha) const;
    [[nodiscard]] bool render_corpses(
        const class WorldTransform& transform) const;
    [[nodiscard]] bool render_explosions(
        const class WorldTransform& transform) const;
    [[nodiscard]] bool render_pending_deployments(
        const World& world, const class WorldTransform& transform) const;
    [[nodiscard]] bool render_deployment_ui(
        const World& world, const class WorldTransform& transform,
        int output_width, int output_height) const;

    SDL_Renderer* renderer_{};
    mutable TextureCache textures_;
    DebugRenderer debug_renderer_;
    std::unordered_map<unsigned int, FrameAnimation> leg_animations_;
    std::unordered_map<unsigned int, FrameAnimation> firing_animations_;
    std::vector<CorpseVisual> corpses_;
    std::vector<ExplosionVisual> explosions_;
    bool debug_overlay_enabled_{};
    std::optional<TroopType> selected_troop_{};
    Point pointer_drawable_{};
};

} // namespace siege
