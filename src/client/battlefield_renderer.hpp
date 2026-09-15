#pragma once

struct SDL_Renderer;

namespace siege {

class TextureCache;
class World;
class WorldTransform;

class BattlefieldRenderer {
public:
    // Draws map-authored terrain/environment and presentation-only zone
    // treatments beneath gameplay objects.
    [[nodiscard]] bool render(SDL_Renderer* renderer, TextureCache& textures,
                              const World& world,
                              const WorldTransform& transform) noexcept;

private:
    [[nodiscard]] bool authored_assets_available(
        const TextureCache& textures, const World& world) noexcept;

    bool asset_check_complete_{};
    bool authored_assets_available_{};
};

} // namespace siege
