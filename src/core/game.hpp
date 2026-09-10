#pragma once

#include "core/simulation.hpp"
#include "world/world.hpp"

struct SDL_Renderer;
struct SDL_Window;

namespace siege {

class Game {
public:
    Game() noexcept;
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    [[nodiscard]] int run();

private:
    [[nodiscard]] bool initialize();
    void shutdown() noexcept;

    SDL_Window* window_{};
    SDL_Renderer* renderer_{};
    World world_{};
    Simulation simulation_;
};

} // namespace siege

