#pragma once

#include "client/font_system.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

struct SDL_Renderer;

namespace siege {

class AiCommander;
class World;
class WorldTransform;
enum class Team;

class DebugRenderer {
public:
    DebugRenderer(SDL_Renderer* renderer, FontSystem& fonts) noexcept;

    [[nodiscard]] bool render(const World& world, const WorldTransform& transform,
                              double render_fps, double simulation_hz,
                              std::size_t corpse_count,
                              std::size_t firing_effect_count,
                              std::size_t explosion_effect_count,
                              std::span<const std::uint32_t> selected_unit_ids,
                              Team controlled_team,
                              const AiCommander& ai_commander) const;

private:
    SDL_Renderer* renderer_{};
    FontSystem& fonts_;
};

} // namespace siege
