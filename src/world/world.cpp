#include "world/world.hpp"

#include "core/economy.hpp"
#include "core/math.hpp"
#include "core/tactical_group.hpp"
#include "core/troop_definition.hpp"

#include <algorithm>

namespace siege {
namespace {

std::vector<Zone> make_zones(const MapDefinition& map) {
    std::vector<Zone> zones;
    zones.reserve(map.zones.size());
    for (const ZoneDefinition& definition : map.zones) {
        zones.emplace_back(definition.id, definition.bounds, definition.type,
                           definition.home_team);
    }
    return zones;
}

} // namespace

World::World(const MatchRules match_rules, const MapDefinition& map)
    : map_{&map}, zones_{make_zones(map)},
      players_{PlayerState{Team::team_a, default_economy_rules.starting_cash},
               PlayerState{Team::team_b, default_economy_rules.starting_cash}},
      match_state_{match_rules} {
    spawn_test_units();
}

const MapDefinition& World::map() const noexcept { return *map_; }

std::span<const Zone> World::zones() const noexcept { return zones_; }

std::span<Zone> World::zones() noexcept { return zones_; }

const std::array<PlayerState, 2>& World::players() const noexcept {
    return players_;
}

std::array<PlayerState, 2>& World::players() noexcept { return players_; }

const PlayerState* World::find_player(const Team team) const noexcept {
    for (const auto& player : players_) {
        if (player.team() == team) {
            return &player;
        }
    }
    return nullptr;
}

PlayerState* World::find_player(const Team team) noexcept {
    for (auto& player : players_) {
        if (player.team() == team) {
            return &player;
        }
    }
    return nullptr;
}

const MatchState& World::match_state() const noexcept { return match_state_; }

MatchState& World::match_state() noexcept { return match_state_; }

std::uint64_t World::scoring_tick_progress() const noexcept {
    return scoring_tick_progress_;
}

std::uint64_t World::advance_scoring_clock(
    const std::uint64_t tick_count,
    const std::uint32_t interval_ticks) noexcept {
    if (interval_ticks == 0) {
        return 0;
    }
    std::uint64_t completed_intervals = tick_count / interval_ticks;
    const std::uint64_t total =
        scoring_tick_progress_ + tick_count % interval_ticks;
    scoring_tick_progress_ = total % interval_ticks;
    completed_intervals += total / interval_ticks;
    return completed_intervals;
}

const std::vector<Unit>& World::units() const noexcept {
    return units_;
}

std::vector<Unit>& World::units() noexcept {
    return units_;
}

const Unit* World::find_unit(const Unit::Id id) const noexcept {
    for (const auto& unit : units_) {
        if (unit.id() == id) {
            return &unit;
        }
    }
    return nullptr;
}

Unit* World::find_unit(const Unit::Id id) noexcept {
    for (auto& unit : units_) {
        if (unit.id() == id) {
            return &unit;
        }
    }
    return nullptr;
}

const std::vector<PendingDeployment>& World::pending_deployments() const noexcept {
    return pending_deployments_;
}

std::vector<PendingDeployment>& World::pending_deployments() noexcept {
    return pending_deployments_;
}

PendingDeployment& World::queue_deployment(
    const Team team, const TroopType troop_type, const Vec2 position,
    const double total_seconds) {
    pending_deployments_.push_back(PendingDeployment{
        .id = next_pending_deployment_id_++,
        .team = team,
        .troop_type = troop_type,
        .position = position,
        .total_seconds = std::max(0.0, total_seconds),
        .remaining_seconds = std::max(0.0, total_seconds),
    });
    return pending_deployments_.back();
}

Unit& World::spawn_unit(const TroopType troop_type, const Team team,
                        const Vec2 position) {
    const TroopDefinition* definition = troop_definition_for(troop_type);
    // Callers validate troop types before reaching the World. Keep a safe
    // baseline here for future serialized/network requests.
    if (definition == nullptr) {
        definition = &rifle_definition;
    }
    const TeamForwardDefinition* forward =
        team_forward_definition(*map_, team);
    const float initial_facing = forward == nullptr
        ? 0.0F
        : facing_from_direction({forward->x_direction, 0.0F});
    return units_.emplace_back(
        next_unit_id_++, definition->type, team, position,
        definition->move_speed, definition->rotation_speed,
        definition->vision_range, definition->vision_angle,
        definition->awareness_radius, definition->preferred_combat_range,
        definition->range_tolerance, definition->aggression,
        definition->retreat_bias, definition->frontline_screen_weight,
        definition->support_positioning_bias,
        definition->support_rear_distance, definition->support_search_radius,
        definition->max_health, definition->hit_radius, definition->weapon,
        definition->target_category, definition->prefers_vehicle_targets,
        definition->independent_turret, definition->turret_rotation_speed,
        initial_facing, definition->mobility_mode);
}

Unit::GroupId World::allocate_tactical_group_id() noexcept {
    return next_tactical_group_id_++;
}

const std::vector<Projectile>& World::projectiles() const noexcept {
    return projectiles_;
}

std::vector<Projectile>& World::projectiles() noexcept {
    return projectiles_;
}

const std::vector<DeathEvent>& World::death_events() const noexcept {
    return death_events_;
}

const std::vector<FireEvent>& World::fire_events() const noexcept {
    return fire_events_;
}

const std::vector<ExplosionEvent>& World::explosion_events() const noexcept {
    return explosion_events_;
}

const std::vector<ZoneOwnershipEvent>& World::zone_ownership_events()
    const noexcept {
    return zone_ownership_events_;
}

std::vector<ZoneOwnershipEvent>& World::zone_ownership_events() noexcept {
    return zone_ownership_events_;
}

void World::clear_transient_events() noexcept {
    death_events_.clear();
    fire_events_.clear();
    explosion_events_.clear();
    zone_ownership_events_.clear();
}

void World::reset_for_sudden_death() noexcept {
    units_.clear();
    next_tactical_group_id_ = 1;
    projectiles_.clear();
    pending_deployments_.clear();
    clear_transient_events();
    for (const std::size_t index : map_->objective_zone_indices) {
        if (index < zones_.size()) {
            zones_[index].reset_objective();
        }
    }
    for (auto& player : players_) {
        player.reset_cash(default_economy_rules.starting_cash);
    }
    scoring_tick_progress_ = 0;
}

void World::remove_dead_units() {
    for (const auto& unit : units_) {
        if (!unit.is_alive()) {
            death_events_.push_back(DeathEvent{
                unit.id(), unit.troop_type(), unit.team(), unit.position(),
                unit.facing_angle(), unit.turret_angle(),
            });
        }
    }
    std::erase_if(units_, [](const Unit& unit) { return !unit.is_alive(); });
    cleanup_tactical_groups(*this);
}

void World::emit_fire_event(const Unit& unit) {
    fire_events_.push_back(
        FireEvent{unit.id(), unit.troop_type(), unit.weapon().type});
}

Projectile& World::spawn_projectile(const WeaponType weapon_type, const Team team,
                                    const Unit::Id source_unit_id,
                                    const Vec2 position, const Vec2 velocity,
                                    const float maximum_distance,
                                    const float damage,
                                    const float splash_radius,
                                    const float vehicle_damage_multiplier,
                                    const float splash_damage) {
    return projectiles_.emplace_back(next_projectile_id_++, weapon_type, team,
                                     source_unit_id, position, velocity,
                                     maximum_distance, damage, splash_radius,
                                     vehicle_damage_multiplier,
                                     ProjectileTrajectory::direct, Vec2{}, 0.0F,
                                     splash_damage);
}

Projectile& World::spawn_indirect_projectile(
    const WeaponType weapon_type, const Team team,
    const Unit::Id source_unit_id, const Vec2 position,
    const Vec2 impact_position, const float projectile_speed,
    const float damage, const float splash_radius,
    const float vehicle_damage_multiplier,
    const float splash_damage) {
    const Vec2 offset = impact_position - position;
    const float distance = length(offset);
    const float speed = std::max(projectile_speed, 1.0F);
    return projectiles_.emplace_back(
        next_projectile_id_++, weapon_type, team, source_unit_id, position,
        normalized(offset) * speed, distance, damage, splash_radius,
        vehicle_damage_multiplier, ProjectileTrajectory::indirect_arc,
        impact_position, distance / speed, splash_damage);
}

void World::emit_explosion_event(const Projectile& projectile,
                                 const Vec2 position) {
    explosion_events_.push_back(ExplosionEvent{
        projectile.id(), projectile.weapon_type(), projectile.team(), position,
        projectile.splash_radius(),
    });
}

void World::emit_zone_ownership_event(const std::size_t zone_id,
                                      const Team previous_owner,
                                      const Team new_owner,
                                      const ZoneTransitionType type) {
    zone_ownership_events_.push_back(
        ZoneOwnershipEvent{zone_id, previous_owner, new_owner, type});
}

void World::spawn_test_units() {
    const auto spawn = [this](const TroopDefinition& definition, const Team team,
                              const Vec2 position, const float initial_facing) {
        units_.emplace_back(next_unit_id_++, definition.type, team, position,
                            definition.move_speed, definition.rotation_speed,
                            definition.vision_range, definition.vision_angle,
                            definition.awareness_radius,
                            definition.preferred_combat_range,
                            definition.range_tolerance, definition.aggression,
                            definition.retreat_bias,
                            definition.frontline_screen_weight,
                            definition.support_positioning_bias,
                            definition.support_rear_distance,
                            definition.support_search_radius,
                            definition.max_health,
                            definition.hit_radius, definition.weapon,
                            definition.target_category,
                            definition.prefers_vehicle_targets,
                            definition.independent_turret,
                            definition.turret_rotation_speed,
                            initial_facing, definition.mobility_mode);
    };

    const Bounds& blue_home =
        map_->zones[map_->team_a_forward.home_zone_index].bounds;
    const Bounds& red_home =
        map_->zones[map_->team_b_forward.home_zone_index].bounds;
    const float north_y = map_->logical_height * (5.0F / 18.0F);
    const float south_y = map_->logical_height * (13.0F / 18.0F);
    const auto blue_x = [&blue_home](const float home_fraction) {
        return blue_home.x + blue_home.width * home_fraction;
    };
    const auto red_x = [&red_home](const float home_fraction) {
        return red_home.x + red_home.width * (1.0F - home_fraction);
    };

    constexpr float rifle_home_fraction = 0.6770833F;
    constexpr float machine_gun_home_fraction = 0.3645833F;
    constexpr float bazooka_home_fraction = 0.2083333F;

    spawn(rifle_definition, Team::team_a,
          {blue_x(rifle_home_fraction), north_y}, 0.0F);
    spawn(rifle_definition, Team::team_a,
          {blue_x(rifle_home_fraction), south_y}, 180.0F);
    spawn(machine_gun_definition, Team::team_a,
          {blue_x(machine_gun_home_fraction), north_y}, 20.0F);
    spawn(machine_gun_definition, Team::team_a,
          {blue_x(machine_gun_home_fraction), south_y}, 160.0F);

    spawn(rifle_definition, Team::team_b,
          {red_x(rifle_home_fraction), north_y}, 0.0F);
    spawn(rifle_definition, Team::team_b,
          {red_x(rifle_home_fraction), south_y}, 180.0F);
    spawn(machine_gun_definition, Team::team_b,
          {red_x(machine_gun_home_fraction), north_y}, 340.0F);
    spawn(machine_gun_definition, Team::team_b,
          {red_x(machine_gun_home_fraction), south_y}, 200.0F);

    spawn(bazooka_definition, Team::team_a,
          {blue_x(bazooka_home_fraction), north_y}, 35.0F);
    spawn(bazooka_definition, Team::team_a,
          {blue_x(bazooka_home_fraction), south_y}, 145.0F);
    spawn(bazooka_definition, Team::team_b,
          {red_x(bazooka_home_fraction), north_y}, 325.0F);
    spawn(bazooka_definition, Team::team_b,
          {red_x(bazooka_home_fraction), south_y}, 215.0F);
}

} // namespace siege
