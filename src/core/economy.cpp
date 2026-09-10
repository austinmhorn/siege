#include "core/economy.hpp"

#include "world/world.hpp"

namespace siege {

Money kill_reward_for(const TroopType troop_type,
                      const EconomyRules rules) noexcept {
    switch (troop_type) {
    case TroopType::rifle:
        return rules.rifle_kill_reward;
    case TroopType::machine_gun:
        return rules.machine_gun_kill_reward;
    case TroopType::bazooka:
        return rules.bazooka_kill_reward;
    }
    return 0;
}

bool award_projectile_kill(World& world, const Projectile& projectile,
                           const Unit& victim,
                           const EconomyRules rules) noexcept {
    if (victim.is_alive() || projectile.team() == Team::none ||
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
    for (auto& player : world.players()) {
        player.accrue_passive_income(rules.passive_income_per_second,
                                     rules.fixed_ticks_per_second,
                                     fixed_tick_count);
    }
}

} // namespace siege
