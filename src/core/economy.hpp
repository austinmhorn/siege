#pragma once

#include "world/player_state.hpp"
#include "world/unit.hpp"

#include <cstdint>

namespace siege {

class World;
class Projectile;

struct EconomyRules {
    Money starting_cash;
    Money passive_income_per_second;
    std::uint32_t fixed_ticks_per_second;
    Money rifle_kill_reward;
    Money machine_gun_kill_reward;
    Money bazooka_kill_reward;
    Money objective_capture_reward;
};

inline constexpr EconomyRules default_economy_rules{
    .starting_cash = 25'000,
    .passive_income_per_second = 100,
    .fixed_ticks_per_second = 60,
    .rifle_kill_reward = 250,
    .machine_gun_kill_reward = 400,
    .bazooka_kill_reward = 600,
    .objective_capture_reward = 1'000,
};

[[nodiscard]] Money kill_reward_for(
    TroopType troop_type,
    EconomyRules rules = default_economy_rules) noexcept;

// Awards only a valid hostile lethal hit whose source is still an active unit.
[[nodiscard]] bool award_projectile_kill(
    World& world, const Projectile& projectile, const Unit& victim,
    EconomyRules rules = default_economy_rules) noexcept;

void award_zone_capture_rewards(
    World& world, EconomyRules rules = default_economy_rules) noexcept;

void update_passive_income(
    World& world, std::uint64_t fixed_tick_count = 1,
    EconomyRules rules = default_economy_rules) noexcept;

} // namespace siege
