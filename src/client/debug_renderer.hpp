#pragma once

#include "client/font_system.hpp"

#include <cstddef>

struct SDL_Renderer;

namespace siege {

class World;
class WorldTransform;

class DebugRenderer {
public:
    DebugRenderer(SDL_Renderer* renderer, FontSystem& fonts) noexcept;

    [[nodiscard]] bool render(const World& world, const WorldTransform& transform,
                              double render_fps, double simulation_hz,
                              std::size_t corpse_count,
                              std::size_t firing_effect_count,
                              std::size_t explosion_effect_count) const;

private:
    SDL_Renderer* renderer_{};
    FontSystem& fonts_;
};

} // namespace siege
