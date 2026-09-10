#include "client/renderer.hpp"

#include "client/world_transform.hpp"
#include "world/world.hpp"

#include <SDL3/SDL.h>

#include <algorithm>

namespace siege {
namespace {

struct Color {
    Uint8 red;
    Uint8 green;
    Uint8 blue;
    Uint8 alpha;
};

constexpr Color letterbox{12, 15, 20, 255};
constexpr Color neutral{66, 72, 78, 255};
constexpr Color team_a{46, 91, 132, 255};
constexpr Color team_b{132, 55, 50, 255};
constexpr Color divider{196, 203, 207, 255};

void set_color(SDL_Renderer* renderer, const Color color) {
    SDL_SetRenderDrawColor(renderer, color.red, color.green, color.blue, color.alpha);
}

Color color_for(const Zone& zone) {
    switch (zone.owner()) {
    case Team::team_a:
        return team_a;
    case Team::team_b:
        return team_b;
    case Team::none:
        return neutral;
    }
    return neutral;
}

} // namespace

Renderer::Renderer(SDL_Renderer* renderer) noexcept : renderer_(renderer) {}

bool Renderer::render(const World& world, const double interpolation_alpha) const {
    static_cast<void>(interpolation_alpha);

    int output_width = 0;
    int output_height = 0;
    if (!SDL_GetRenderOutputSize(renderer_, &output_width, &output_height)) {
        return false;
    }

    const WorldTransform transform{World::width, World::height, output_width, output_height};

    set_color(renderer_, letterbox);
    if (!SDL_RenderClear(renderer_)) {
        return false;
    }

    for (const auto& zone : world.zones()) {
        const auto bounds = transform.world_to_drawable(zone.bounds());
        const SDL_FRect rectangle{bounds.x, bounds.y, bounds.width, bounds.height};
        set_color(renderer_, color_for(zone));
        if (!SDL_RenderFillRect(renderer_, &rectangle)) {
            return false;
        }
    }

    set_color(renderer_, divider);
    const float line_width = std::max(1.0F, 3.0F * transform.scale());
    for (std::size_t index = 1; index < world.zones().size(); ++index) {
        const auto& bounds = world.zones()[index].bounds();
        const auto top = transform.world_to_drawable(Point{bounds.x, 0.0F});
        const SDL_FRect divider_rectangle{top.x - line_width * 0.5F, top.y, line_width,
                                          transform.viewport().height};
        if (!SDL_RenderFillRect(renderer_, &divider_rectangle)) {
            return false;
        }
    }

    return SDL_RenderPresent(renderer_);
}

} // namespace siege
