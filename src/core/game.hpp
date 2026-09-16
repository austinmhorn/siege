#pragma once

#include "core/ai_commander.hpp"
#include "core/simulation.hpp"
#include "world/world.hpp"

struct SDL_Renderer;
struct SDL_Window;

namespace siege {

class Game {
public:
    explicit Game(AiProfile ai_profile = make_ai_profile()) noexcept;
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
    AiCommander red_commander_;
};

} // namespace siege
