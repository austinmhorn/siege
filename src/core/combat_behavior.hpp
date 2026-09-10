#pragma once

#include "world/unit.hpp"

namespace siege {

// Classifies target distance using an inclusive preferred-range band.
[[nodiscard]] CombatMovementState
combat_movement_for(const Unit& observer, const Unit& target) noexcept;

} // namespace siege
