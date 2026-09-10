#pragma once

#include "client/frame_animation.hpp"
#include "client/texture_cache.hpp"
#include "client/debug_renderer.hpp"

#include <filesystem>
#include <unordered_map>

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
    [[nodiscard]] bool render_units(const World& world,
                                    const class WorldTransform& transform,
                                    double interpolation_alpha) const;

    SDL_Renderer* renderer_{};
    mutable TextureCache textures_;
    DebugRenderer debug_renderer_;
    std::unordered_map<unsigned int, FrameAnimation> leg_animations_;
    bool debug_overlay_enabled_{};
};

} // namespace siege
