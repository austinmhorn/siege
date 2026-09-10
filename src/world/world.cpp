#include "world/world.hpp"

#include "core/troop_definition.hpp"

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

Projectile& World::spawn_projectile(const WeaponType weapon_type, const Team team,
                                    const Unit::Id source_unit_id,
                                    const Vec2 position, const Vec2 velocity,
                                    const float maximum_distance,
                                    const float damage) {
    return projectiles_.emplace_back(next_projectile_id_++, weapon_type, team,
                                     source_unit_id, position, velocity,
                                     maximum_distance, damage);
}

void World::spawn_test_units() {
    const auto spawn = [this](const Team team, const Vec2 position,
                              const float initial_facing) {
        units_.emplace_back(next_unit_id_++, rifle_definition.type, team, position,
                            rifle_definition.move_speed,
                            rifle_definition.rotation_speed,
                            rifle_definition.vision_range,
                            rifle_definition.vision_angle,
                            rifle_definition.awareness_radius,
                            rifle_definition.preferred_combat_range,
                            rifle_definition.range_tolerance,
                            rifle_definition.aggression,
                            rifle_definition.retreat_bias,
                            rifle_definition.max_health,
                            rifle_definition.hit_radius,
                            rifle_definition.weapon, initial_facing);
    };

    spawn(Team::team_a, {150.0F, 250.0F}, 0.0F);
    spawn(Team::team_a, {150.0F, 250.0F}, 180.0F);
    spawn(Team::team_a, {180.0F, 520.0F}, 20.0F);
    spawn(Team::team_a, {150.0F, 790.0F}, 160.0F);

    spawn(Team::team_b, {1770.0F, 290.0F}, 0.0F);
    spawn(Team::team_b, {1770.0F, 290.0F}, 180.0F);
    spawn(Team::team_b, {1740.0F, 560.0F}, 340.0F);
    spawn(Team::team_b, {1770.0F, 830.0F}, 200.0F);
}

} // namespace siege
