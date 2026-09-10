#pragma once

#include <algorithm>

namespace siege {

// Production UI direction: Team A (+100) is blue/left and Team B (-100)
// is red/right. Simulation capture semantics remain unchanged.
[[nodiscard]] constexpr float capture_bar_fraction(
    const float capture_value) noexcept {
    return (100.0F - std::clamp(capture_value, -100.0F, 100.0F)) /
           200.0F;
}

} // namespace siege
