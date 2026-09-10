#pragma once

#include <cstddef>

struct SDL_Renderer;

namespace siege {

class World;
class WorldTransform;

class DebugRenderer {
public:
    explicit DebugRenderer(SDL_Renderer* renderer) noexcept;

    [[nodiscard]] bool render(const World& world, const WorldTransform& transform,
                              double render_fps, double simulation_hz,
                              std::size_t corpse_count,
                              std::size_t firing_effect_count) const;

private:
    SDL_Renderer* renderer_{};
};

} // namespace siege
