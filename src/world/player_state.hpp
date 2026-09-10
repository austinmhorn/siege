#pragma once

#include "world/zone.hpp"

#include <cstdint>

namespace siege {

using Money = std::int64_t;

class PlayerState {
public:
    PlayerState(Team team, Money starting_cash) noexcept;

    [[nodiscard]] Team team() const noexcept;
    [[nodiscard]] Money cash() const noexcept;
    [[nodiscard]] bool can_afford(Money amount) const noexcept;
    [[nodiscard]] bool try_spend(Money amount) noexcept;

    void accrue_passive_income(Money cash_per_second,
                               std::uint32_t fixed_ticks_per_second,
                               std::uint64_t fixed_tick_count = 1) noexcept;

private:
    Team team_{Team::none};
    Money cash_{};
    Money passive_income_remainder_{};
};

} // namespace siege
