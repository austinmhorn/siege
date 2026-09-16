#include "core/game.hpp"

#include "client/pointer_input.hpp"
#include "client/renderer.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>

namespace siege {
namespace {

std::optional<PointerButton> pointer_button_for(
    const Uint8 sdl_button) noexcept {
    if (sdl_button == SDL_BUTTON_LEFT) {
        return PointerButton::primary;
    }
    if (sdl_button == SDL_BUTTON_RIGHT) {
        return PointerButton::secondary;
    }
    return std::nullopt;
}

} // namespace

Game::Game(const AiProfile ai_profile) noexcept
    : simulation_(world_), red_commander_(Team::team_b, ai_profile) {}

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
    PointerInputRouter pointer_input;
    using clock = std::chrono::steady_clock;
    auto previous_time = clock::now();
    double accumulator = 0.0;
    double render_fps = 0.0;
    bool running = true;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                running = false;
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                       event.key.key == SDLK_ESCAPE) {
                if (!client_renderer.cancel_placement()) {
                    running = false;
                }
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                       (event.key.key == SDLK_F3 || event.key.key == SDLK_D)) {
                client_renderer.toggle_debug_overlay();
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                       event.key.key == SDLK_F4) {
                pointer_input.cancel();
                client_renderer.toggle_controlled_team();
                red_commander_.update(world_, client_renderer.controlled_team(),
                                      0);
            } else if (event.type == SDL_EVENT_MOUSE_MOTION) {
                if (SDL_ConvertEventToRenderCoordinates(renderer_, &event)) {
                    client_renderer.set_pointer_position(world_, event.motion.x,
                                                         event.motion.y);
                }
            } else if ((event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                        event.type == SDL_EVENT_MOUSE_BUTTON_UP) &&
                       pointer_button_for(event.button.button).has_value()) {
                if (!SDL_ConvertEventToRenderCoordinates(renderer_, &event)) {
                    continue;
                }
                client_renderer.set_pointer_position(world_, event.button.x,
                                                     event.button.y);
                const PointerButton button =
                    *pointer_button_for(event.button.button);
                if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                    const bool control_held =
                        (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
                    const PointerDispatch dispatch =
                        pointer_input.press(button, control_held);
                    if (!world_.match_state().active()) {
                        continue;
                    }
                    if (dispatch == PointerDispatch::secondary) {
                        client_renderer.handle_secondary_pointer_press(
                            world_, event.button.x, event.button.y);
                    } else if (dispatch == PointerDispatch::primary) {
                        client_renderer.handle_primary_pointer_press(
                            world_, event.button.x, event.button.y);
                    }
                } else {
                    const PointerDispatch dispatch = pointer_input.release(button);
                    if (!world_.match_state().active()) {
                        continue;
                    }
                    if (dispatch == PointerDispatch::secondary) {
                        client_renderer.handle_secondary_pointer_release(
                            world_, event.button.x, event.button.y);
                    } else if (dispatch == PointerDispatch::primary) {
                        client_renderer.handle_primary_pointer_release(
                            world_, event.button.x, event.button.y);
                    }
                }
            }
        }

        const auto current_time = clock::now();
        const std::chrono::duration<double> elapsed = current_time - previous_time;
        previous_time = current_time;
        if (elapsed.count() > 0.0) {
            const double instantaneous_fps = 1.0 / elapsed.count();
            render_fps = render_fps == 0.0 ? instantaneous_fps
                                           : render_fps * 0.9 + instantaneous_fps * 0.1;
        }
        accumulator += std::min(elapsed.count(), maximum_frame_delta);

        int update_count = 0;
        while (accumulator >= fixed_delta && update_count < maximum_updates_per_frame) {
            simulation_.update(fixed_delta);
            red_commander_.update(world_, client_renderer.controlled_team());
            client_renderer.update(world_, fixed_delta);
            accumulator -= fixed_delta;
            ++update_count;
        }
        if (update_count == maximum_updates_per_frame && accumulator >= fixed_delta) {
            accumulator = std::fmod(accumulator, fixed_delta);
        }

        const double interpolation_alpha = accumulator / fixed_delta;
        if (!client_renderer.render(world_, red_commander_, interpolation_alpha,
                                    render_fps)) {
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
