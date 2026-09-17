#pragma once

#include "world/player_state.hpp"
#include "world/unit.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace siege {

class World;
class Projectile;

struct IncomePhaseRules {
    std::uint32_t begins_at_second;
    Money base_income_per_second;
    Money comeback_income_per_enemy_objective;
};

struct EconomyRules {
    Money starting_cash;
    std::array<IncomePhaseRules, 3> income_phases;
    std::uint32_t fixed_ticks_per_second;
    Money rifle_kill_reward;
    Money machine_gun_kill_reward;
    Money bazooka_kill_reward;
    Money medium_tank_kill_reward;
    Money anti_tank_kill_reward;
    Money mortar_kill_reward;
    Money objective_capture_reward;
};

inline constexpr EconomyRules default_economy_rules{
    .starting_cash = 25'000,
    .income_phases = {{
        {.begins_at_second = 0,
         .base_income_per_second = 200,
         .comeback_income_per_enemy_objective = 25},
        {.begins_at_second = 30,
         .base_income_per_second = 400,
         .comeback_income_per_enemy_objective = 50},
        {.begins_at_second = 120,
         .base_income_per_second = 600,
         .comeback_income_per_enemy_objective = 75},
    }},
    .fixed_ticks_per_second = 60,
    .rifle_kill_reward = 250,
    .machine_gun_kill_reward = 400,
    .bazooka_kill_reward = 600,
    .medium_tank_kill_reward = 1'200,
    .anti_tank_kill_reward = 700,
    .mortar_kill_reward = 750,
    .objective_capture_reward = 1'000,
};

[[nodiscard]] std::size_t income_phase_index(
    std::uint64_t elapsed_ticks,
    EconomyRules rules = default_economy_rules) noexcept;

[[nodiscard]] IncomePhaseRules income_phase_rules(
    std::uint64_t elapsed_ticks,
    const EconomyRules& rules = default_economy_rules) noexcept;

[[nodiscard]] Money base_passive_income(
    std::uint64_t elapsed_ticks,
    EconomyRules rules = default_economy_rules) noexcept;

[[nodiscard]] Money comeback_income_per_objective(
    std::uint64_t elapsed_ticks,
    EconomyRules rules = default_economy_rules) noexcept;

[[nodiscard]] Money kill_reward_for(
    TroopType troop_type,
    EconomyRules rules = default_economy_rules) noexcept;

[[nodiscard]] Money comeback_income_bonus(
    const World& world, Team team,
    EconomyRules rules = default_economy_rules) noexcept;

[[nodiscard]] Money comeback_income_bonus(
    const World& world, Team team, std::uint64_t elapsed_ticks,
    EconomyRules rules = default_economy_rules) noexcept;

[[nodiscard]] Money effective_passive_income_rate(
    const World& world, Team team,
    EconomyRules rules = default_economy_rules) noexcept;

[[nodiscard]] Money effective_passive_income_rate(
    const World& world, Team team, std::uint64_t elapsed_ticks,
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
