#pragma once

struct SDL_Renderer;

namespace siege {

class World;

class Renderer {
public:
    explicit Renderer(SDL_Renderer* renderer) noexcept;

    [[nodiscard]] bool render(const World& world, double interpolation_alpha) const;

private:
    SDL_Renderer* renderer_{};
};

} // namespace siege

