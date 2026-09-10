#pragma once

#include "client/frame_animation.hpp"
#include "client/texture_cache.hpp"
#include "client/debug_renderer.hpp"
#include "world/combat_event.hpp"

#include <filesystem>
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
    [[nodiscard]] bool render(const World& world, double interpolation_alpha,
                              double render_fps) const;

private:
    struct CorpseVisual {
        DeathEvent death;
        FrameAnimation animation{{4, 3, 2, 1}, 0.12, false};
        double fade_elapsed{};
    };

    [[nodiscard]] bool render_units(const World& world,
                                    const class WorldTransform& transform,
                                    double interpolation_alpha) const;
    [[nodiscard]] bool render_projectiles(const World& world,
                                          const class WorldTransform& transform,
                                          double interpolation_alpha) const;
    [[nodiscard]] bool render_corpses(
        const class WorldTransform& transform) const;

    SDL_Renderer* renderer_{};
    mutable TextureCache textures_;
    DebugRenderer debug_renderer_;
    std::unordered_map<unsigned int, FrameAnimation> leg_animations_;
    std::unordered_map<unsigned int, FrameAnimation> firing_animations_;
    std::vector<CorpseVisual> corpses_;
    bool debug_overlay_enabled_{};
};

} // namespace siege
