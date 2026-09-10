#include "core/game.hpp"

#include "client/renderer.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace siege {

Game::Game() noexcept : simulation_(world_) {}

Game::~Game() {
    shutdown();
}

bool Game::initialize() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return false;
    }

    constexpr auto flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (!SDL_CreateWindowAndRenderer("siege", 1280, 720, flags, &window_, &renderer_)) {
        std::fprintf(stderr, "Window creation failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetWindowMinimumSize(window_, 640, 360);
    return true;
}

int Game::run() {
    if (!initialize()) {
        return 1;
    }

    constexpr double fixed_delta = 1.0 / 60.0;
    constexpr double maximum_frame_delta = 0.25;
    constexpr int maximum_updates_per_frame = 8;

    const char* base_path = SDL_GetBasePath();
    if (base_path == nullptr) {
        std::fprintf(stderr, "Could not resolve executable path: %s\n", SDL_GetError());
        return 1;
    }
    const auto asset_root = std::filesystem::path{base_path} / "assets";
    Renderer client_renderer{renderer_, asset_root};
    using clock = std::chrono::steady_clock;
    auto previous_time = clock::now();
    double accumulator = 0.0;
    bool running = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                       event.key.key == SDLK_R) {
                client_renderer.rotate_test_soldier();
            }
        }

        const auto current_time = clock::now();
        const std::chrono::duration<double> elapsed = current_time - previous_time;
        previous_time = current_time;
        accumulator += std::min(elapsed.count(), maximum_frame_delta);

        int update_count = 0;
        while (accumulator >= fixed_delta && update_count < maximum_updates_per_frame) {
            simulation_.update(fixed_delta);
            client_renderer.update(fixed_delta);
            accumulator -= fixed_delta;
            ++update_count;
        }
        if (update_count == maximum_updates_per_frame && accumulator >= fixed_delta) {
            accumulator = std::fmod(accumulator, fixed_delta);
        }

        const double interpolation_alpha = accumulator / fixed_delta;
        if (!client_renderer.render(world_, interpolation_alpha)) {
            std::fprintf(stderr, "Rendering failed: %s\n", SDL_GetError());
            return 1;
        }
    }

    return 0;
}

void Game::shutdown() noexcept {
    if (renderer_ != nullptr) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

} // namespace siege
