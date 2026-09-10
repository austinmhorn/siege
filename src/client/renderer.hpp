#pragma once

#include "client/frame_animation.hpp"
#include "client/texture_cache.hpp"

#include <filesystem>

struct SDL_Renderer;

namespace siege {

class World;

class Renderer {
public:
    Renderer(SDL_Renderer* renderer, std::filesystem::path asset_root);

    void update(double fixed_delta_seconds) noexcept;
    void rotate_test_soldier() noexcept;
    [[nodiscard]] bool render(const World& world, double interpolation_alpha) const;

private:
    [[nodiscard]] bool render_test_soldier(const class WorldTransform& transform) const;

    SDL_Renderer* renderer_{};
    mutable TextureCache textures_;
    FrameAnimation rifle_animation_;
    double test_soldier_angle_degrees_{27.0};
};

} // namespace siege
