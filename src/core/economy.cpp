#include "core/economy.hpp"

#include "world/world.hpp"

namespace siege {

void update_passive_income(World& world, const std::uint64_t fixed_tick_count,
                           const EconomyRules rules) noexcept {
    for (auto& player : world.players()) {
        player.accrue_passive_income(rules.passive_income_per_second,
                                     rules.fixed_ticks_per_second,
                                     fixed_tick_count);
    }
}

} // namespace siege
