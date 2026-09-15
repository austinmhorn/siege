#pragma once

struct SDL_Renderer;

namespace siege {

class World;
class WorldTransform;

// Draws presentation-only terrain and zone treatments beneath gameplay objects.
// Keeping this pass separate makes it possible to replace the procedural theme
// with map-authored presentation data without changing simulation geometry.
[[nodiscard]] bool render_battlefield(SDL_Renderer* renderer,
                                      const World& world,
                                      const WorldTransform& transform) noexcept;

} // namespace siege
