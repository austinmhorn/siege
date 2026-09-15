#pragma once

#include "client/local_control.hpp"
#include "world/player_state.hpp"

#include <optional>
#include <string>

namespace siege {

class World;

[[nodiscard]] std::optional<Money> controlled_team_cash(
    const World& world, const LocalControlState& local_control) noexcept;

[[nodiscard]] std::string format_cash(Money cash);

} // namespace siege
