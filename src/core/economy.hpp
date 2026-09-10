#pragma once

#include "world/player_state.hpp"

#include <cstdint>

namespace siege {

class World;

struct EconomyRules {
    Money starting_cash;
    Money passive_income_per_second;
    std::uint32_t fixed_ticks_per_second;
};

inline constexpr EconomyRules default_economy_rules{
    .starting_cash = 25'000,
    .passive_income_per_second = 100,
    .fixed_ticks_per_second = 60,
};

void update_passive_income(
    World& world, std::uint64_t fixed_tick_count = 1,
    EconomyRules rules = default_economy_rules) noexcept;

} // namespace siege
