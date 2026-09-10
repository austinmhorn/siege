#include "world/world.hpp"

#include "core/troop_definition.hpp"

#include <algorithm>

namespace siege {
namespace {

constexpr float zone_width = World::width / static_cast<float>(World::zone_count);

constexpr Bounds zone_bounds(const std::size_t index) noexcept {
    return Bounds{static_cast<float>(index) * zone_width, 0.0F, zone_width,
                  World::height};
}

} // namespace

World::World()
    : zones_{Zone{0, zone_bounds(0), ZoneType::home, Team::team_a},
             Zone{1, zone_bounds(1), ZoneType::objective, Team::none},
             Zone{2, zone_bounds(2), ZoneType::objective, Team::none},
             Zone{3, zone_bounds(3), ZoneType::objective, Team::none},
             Zone{4, zone_bounds(4), ZoneType::home, Team::team_b}} {
    spawn_test_units();
}

const std::array<Zone, World::zone_count>& World::zones() const noexcept {
    return zones_;
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

void World::clear_transient_events() noexcept {
    death_events_.clear();
    fire_events_.clear();
    explosion_events_.clear();
}

void World::remove_dead_units() {
    for (const auto& unit : units_) {
        if (!unit.is_alive()) {
            death_events_.push_back(DeathEvent{
                unit.id(), unit.troop_type(), unit.team(), unit.position(),
                unit.facing_angle(),
            });
        }
    }
    std::erase_if(units_, [](const Unit& unit) { return !unit.is_alive(); });
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
                                    const float splash_radius) {
    return projectiles_.emplace_back(next_projectile_id_++, weapon_type, team,
                                     source_unit_id, position, velocity,
                                     maximum_distance, damage, splash_radius);
}

void World::emit_explosion_event(const Projectile& projectile,
                                 const Vec2 position) {
    explosion_events_.push_back(ExplosionEvent{
        projectile.id(), projectile.weapon_type(), projectile.team(), position,
        projectile.splash_radius(),
    });
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
                            definition.retreat_bias, definition.max_health,
                            definition.hit_radius, definition.weapon,
                            initial_facing);
    };

    spawn(rifle_definition, Team::team_a, {150.0F, 250.0F}, 0.0F);
    spawn(rifle_definition, Team::team_a, {150.0F, 250.0F}, 180.0F);
    spawn(machine_gun_definition, Team::team_a, {180.0F, 520.0F}, 20.0F);
    spawn(machine_gun_definition, Team::team_a, {150.0F, 790.0F}, 160.0F);

    spawn(rifle_definition, Team::team_b, {1770.0F, 290.0F}, 0.0F);
    spawn(rifle_definition, Team::team_b, {1770.0F, 290.0F}, 180.0F);
    spawn(machine_gun_definition, Team::team_b, {1740.0F, 560.0F}, 340.0F);
    spawn(machine_gun_definition, Team::team_b, {1770.0F, 830.0F}, 200.0F);

    spawn(bazooka_definition, Team::team_a, {170.0F, 660.0F}, 35.0F);
    spawn(bazooka_definition, Team::team_a, {150.0F, 970.0F}, 145.0F);
    spawn(bazooka_definition, Team::team_b, {1750.0F, 700.0F}, 325.0F);
    spawn(bazooka_definition, Team::team_b, {1770.0F, 1010.0F}, 215.0F);
}

} // namespace siege
