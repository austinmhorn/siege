#pragma once

namespace siege {

class Unit;

[[nodiscard]] bool inside_vision_cone(const Unit& observer,
                                      const Unit& target) noexcept;
[[nodiscard]] bool inside_awareness_radius(const Unit& observer,
                                           const Unit& target) noexcept;
[[nodiscard]] bool can_perceive(const Unit& observer, const Unit& target) noexcept;

} // namespace siege
