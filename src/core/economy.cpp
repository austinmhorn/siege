#include "core/economy.hpp"

#include "world/world.hpp"

#include <algorithm>

namespace siege {

std::size_t income_phase_index(const std::uint64_t elapsed_ticks,
                               const EconomyRules rules) noexcept {
    std::size_t selected = 0;
    for (std::size_t index = 1; index < rules.income_phases.size(); ++index) {
        const std::uint64_t threshold =
            static_cast<std::uint64_t>(
                rules.income_phases[index].begins_at_second) *
            rules.fixed_ticks_per_second;
        if (elapsed_ticks < threshold) {
            break;
        }
        selected = index;
    }
    return selected;
}

IncomePhaseRules income_phase_rules(
    const std::uint64_t elapsed_ticks,
    const EconomyRules& rules) noexcept {
    return rules.income_phases[income_phase_index(elapsed_ticks, rules)];
}

Money base_passive_income(const std::uint64_t elapsed_ticks,
                          const EconomyRules rules) noexcept {
    return income_phase_rules(elapsed_ticks, rules).base_income_per_second;
}

Money comeback_income_per_objective(
    const std::uint64_t elapsed_ticks,
    const EconomyRules rules) noexcept {
    return income_phase_rules(elapsed_ticks, rules)
        .comeback_income_per_enemy_objective;
}

Money kill_reward_for(const TroopType troop_type,
                      const EconomyRules rules) noexcept {
    switch (troop_type) {
    case TroopType::rifle:
        return rules.rifle_kill_reward;
    case TroopType::machine_gun:
        return rules.machine_gun_kill_reward;
    case TroopType::bazooka:
        return rules.bazooka_kill_reward;
    case TroopType::medium_tank:
        return rules.medium_tank_kill_reward;
    case TroopType::anti_tank:
        return rules.anti_tank_kill_reward;
    case TroopType::mortar:
        return rules.mortar_kill_reward;
    }
    return 0;
}

Money comeback_income_bonus(const World& world, const Team team,
                            const EconomyRules rules) noexcept {
    return comeback_income_bonus(world, team,
                                 world.match_state().phase_elapsed_ticks(),
                                 rules);
}

Money comeback_income_bonus(const World& world, const Team team,
                            const std::uint64_t elapsed_ticks,
                            const EconomyRules rules) noexcept {
    if (team != Team::team_a && team != Team::team_b) {
        return 0;
    }

    const Team opposing_team =
        team == Team::team_a ? Team::team_b : Team::team_a;
    Money enemy_owned_objectives = 0;
    for (const std::size_t zone_index :
         world.map().objective_zone_indices) {
        if (zone_index < world.zones().size() &&
            world.zones()[zone_index].owner() == opposing_team) {
            ++enemy_owned_objectives;
        }
    }
    return enemy_owned_objectives *
        comeback_income_per_objective(elapsed_ticks, rules);
}

Money effective_passive_income_rate(const World& world, const Team team,
                                    const EconomyRules rules) noexcept {
    return effective_passive_income_rate(
        world, team, world.match_state().phase_elapsed_ticks(), rules);
}

Money effective_passive_income_rate(const World& world, const Team team,
                                    const std::uint64_t elapsed_ticks,
                                    const EconomyRules rules) noexcept {
    return base_passive_income(elapsed_ticks, rules) +
        comeback_income_bonus(world, team, elapsed_ticks, rules);
}

bool award_projectile_kill(World& world, const Projectile& projectile,
                           const Unit& victim,
                           const EconomyRules rules) noexcept {
    if (!world.match_state().active() || victim.is_alive() ||
        projectile.team() == Team::none ||
        victim.team() == Team::none || victim.team() == projectile.team() ||
        victim.id() == projectile.source_unit_id()) {
        return false;
    }

    const Unit* source = world.find_unit(projectile.source_unit_id());
    PlayerState* killer = world.find_player(projectile.team());
    if (source == nullptr || !source->is_alive() ||
        source->team() != projectile.team() || killer == nullptr) {
        return false;
    }

    killer->credit(kill_reward_for(victim.troop_type(), rules));
    return true;
}

void award_zone_capture_rewards(World& world,
                                const EconomyRules rules) noexcept {
    if (!world.match_state().active()) {
        return;
    }
    for (auto& event : world.zone_ownership_events()) {
        if (event.reward_processed) {
            continue;
        }
        event.reward_processed = true;
        if (event.type != ZoneTransitionType::captured ||
            event.previous_owner != Team::none ||
            (event.new_owner != Team::team_a &&
             event.new_owner != Team::team_b)) {
            continue;
        }
        if (PlayerState* player = world.find_player(event.new_owner)) {
            player->credit(rules.objective_capture_reward);
        }
    }
}

void update_passive_income(World& world, const std::uint64_t fixed_tick_count,
                           const EconomyRules rules) noexcept {
    if (!world.match_state().active() || fixed_tick_count == 0) {
        return;
    }

    std::uint64_t elapsed_ticks =
        world.match_state().phase_elapsed_ticks();
    std::uint64_t remaining_ticks = fixed_tick_count;
    while (remaining_ticks > 0) {
        const std::size_t phase_index =
            income_phase_index(elapsed_ticks, rules);
        std::uint64_t phase_ticks = remaining_ticks;
        if (phase_index + 1 < rules.income_phases.size()) {
            const std::uint64_t next_phase_tick =
                static_cast<std::uint64_t>(
                    rules.income_phases[phase_index + 1].begins_at_second) *
                rules.fixed_ticks_per_second;
            if (elapsed_ticks < next_phase_tick) {
                phase_ticks = std::min(
                    remaining_ticks, next_phase_tick - elapsed_ticks);
            }
        }

        for (auto& player : world.players()) {
            player.accrue_passive_income(
                effective_passive_income_rate(
                    world, player.team(), elapsed_ticks, rules),
                rules.fixed_ticks_per_second, phase_ticks);
        }
        elapsed_ticks += phase_ticks;
        remaining_ticks -= phase_ticks;
    }
}

} // namespace siege
