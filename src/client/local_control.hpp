#pragma once

#include "world/zone.hpp"

#include <string_view>

namespace siege {

// Client-only authority selector used by local input. It never changes
// simulation ownership or either team's authoritative state.
class LocalControlState {
public:
    [[nodiscard]] constexpr Team team() const noexcept { return team_; }

    [[nodiscard]] constexpr Team toggle() noexcept {
        team_ = team_ == Team::team_a ? Team::team_b : Team::team_a;
        return team_;
    }

private:
    Team team_{Team::team_a};
};

[[nodiscard]] constexpr std::string_view team_color_name(
    const Team team) noexcept {
    return team == Team::team_b ? "RED" : "BLUE";
}

} // namespace siege
