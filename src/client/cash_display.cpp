#include "client/cash_display.hpp"

#include "world/world.hpp"

#include <cstddef>

namespace siege {

std::optional<Money> controlled_team_cash(
    const World& world, const LocalControlState& local_control) noexcept {
    const PlayerState* player = world.find_player(local_control.team());
    if (player == nullptr) {
        return std::nullopt;
    }
    return player->cash();
}

std::string format_cash(const Money cash) {
    std::string digits = std::to_string(cash);
    const std::size_t first_digit = cash < 0 ? 1 : 0;
    for (std::size_t position = digits.size();
         position > first_digit + 3; position -= 3) {
        digits.insert(position - 3, 1, ',');
    }
    digits.insert(first_digit, 1, '$');
    return digits;
}

} // namespace siege
