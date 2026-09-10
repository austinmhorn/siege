#pragma once

#include "world/unit.hpp"

#include <optional>
#include <span>

namespace siege {

// Retains the observer's current target while it remains perceptible. Otherwise,
// returns the nearest perceptible enemy, breaking equal-distance ties by unit ID.
[[nodiscard]] std::optional<Unit::Id>
select_target(const Unit& observer, std::span<const Unit> units) noexcept;

} // namespace siege
