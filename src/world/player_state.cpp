#include "world/player_state.hpp"

#include <algorithm>

namespace siege {

PlayerState::PlayerState(const Team team, const Money starting_cash) noexcept
    : team_(team), cash_(std::max<Money>(0, starting_cash)) {}

Team PlayerState::team() const noexcept { return team_; }

Money PlayerState::cash() const noexcept { return cash_; }

bool PlayerState::can_afford(const Money amount) const noexcept {
    return amount >= 0 && cash_ >= amount;
}

bool PlayerState::try_spend(const Money amount) noexcept {
    if (!can_afford(amount)) {
        return false;
    }
    cash_ -= amount;
    return true;
}

void PlayerState::credit(const Money amount) noexcept {
    if (amount > 0) {
        cash_ += amount;
    }
}

void PlayerState::accrue_passive_income(
    const Money cash_per_second, const std::uint32_t fixed_ticks_per_second,
    const std::uint64_t fixed_tick_count) noexcept {
    if (cash_per_second <= 0 || fixed_ticks_per_second == 0 ||
        fixed_tick_count == 0) {
        return;
    }

    const Money income_numerator =
        passive_income_remainder_ +
        cash_per_second * static_cast<Money>(fixed_tick_count);
    cash_ += income_numerator /
             static_cast<Money>(fixed_ticks_per_second);
    passive_income_remainder_ =
        income_numerator % static_cast<Money>(fixed_ticks_per_second);
}

} // namespace siege
