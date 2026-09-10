#include "core/simulation.hpp"

#include "core/combat_behavior.hpp"
#include "core/projectile_collision.hpp"
#include "core/targeting.hpp"
#include "core/weapon.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace siege {
namespace {

constexpr float separation_radius = 80.0F;
constexpr float separation_weight = 0.9F;
constexpr float preferred_y_scale = 120.0F;
constexpr float maximum_y_correction = 0.45F;
constexpr float world_margin = 32.0F;

struct MotionIntent {
    Vec2 velocity;
    float desired_facing;
    MovementState state;
    CombatMovementState combat_state;
};

Vec2 velocity_from_steering(const Vec2 steering, const float speed) noexcept {
    if (length_squared(steering) <= 0.0001F || speed <= 0.0F) {
        return {};
    }
    return normalized(steering) * speed;
}

Vec2 soft_separation_velocity(const Vec2 separation,
                              const float move_speed) noexcept {
    const float influence = std::min(length(separation), 1.0F);
    return velocity_from_steering(separation, move_speed * influence);
}

Vec2 separation_for(const Unit& unit, const std::vector<Unit>& units) noexcept {
    Vec2 separation{};
    for (const auto& other : units) {
        if (!other.is_alive() || other.id() == unit.id() ||
            other.team() != unit.team()) {
            continue;
        }

        Vec2 offset = unit.position() - other.position();
        float distance = length(offset);
        if (distance >= separation_radius) {
            continue;
        }

        if (distance <= 0.0001F) {
            offset = Vec2{0.0F, unit.id() < other.id() ? -1.0F : 1.0F};
            distance = 0.0F;
        } else {
            offset = offset * (1.0F / distance);
        }

        const float influence = 1.0F - distance / separation_radius;
        separation = separation + offset * influence;
    }
    return separation * separation_weight;
}

Unit* nearest_projectile_hit(const Projectile& projectile,
                             std::vector<Unit>& units) noexcept {
    Unit* nearest = nullptr;
    float nearest_fraction = 2.0F;
    for (auto& candidate : units) {
        if (!candidate.is_alive() || candidate.id() == projectile.source_unit_id() ||
            candidate.team() == Team::none || candidate.team() == projectile.team()) {
            continue;
        }

        const auto hit_fraction = swept_circle_hit_fraction(
            projectile.previous_position(), projectile.position(),
            candidate.position(), candidate.hit_radius());
        if (!hit_fraction.has_value()) {
            continue;
        }

        if (*hit_fraction < nearest_fraction ||
            (*hit_fraction == nearest_fraction && nearest != nullptr &&
             candidate.id() < nearest->id())) {
            nearest = &candidate;
            nearest_fraction = *hit_fraction;
        }
    }
    return nearest;
}

bool outside_world(const Vec2 position) noexcept {
    return position.x < 0.0F || position.x > World::width ||
           position.y < 0.0F || position.y > World::height;
}

} // namespace

Simulation::Simulation(World& world) noexcept : world_(world) {}

void Simulation::update(const double fixed_delta_seconds) noexcept {
    world_.clear_transient_events();
    auto& units = world_.units();
    for (auto& unit : units) {
        unit.begin_simulation_step();
        unit.tick_weapon_cooldown(fixed_delta_seconds);
    }

    auto& projectiles = world_.projectiles();
    for (auto& projectile : projectiles) {
        projectile.begin_simulation_step();
        projectile.advance(fixed_delta_seconds);
    }
    for (auto projectile = projectiles.begin(); projectile != projectiles.end();) {
        if (Unit* hit = nearest_projectile_hit(*projectile, units)) {
            hit->apply_damage(projectile->damage());
            projectile = projectiles.erase(projectile);
        } else if (projectile->expired() ||
                   outside_world(projectile->position())) {
            projectile = projectiles.erase(projectile);
        } else {
            ++projectile;
        }
    }

    world_.remove_dead_units();

    std::vector<std::optional<Unit::Id>> target_ids;
    target_ids.reserve(units.size());
    for (const auto& unit : units) {
        target_ids.push_back(select_target(unit, units));
    }

    std::vector<MotionIntent> intents;
    intents.reserve(units.size());
    for (std::size_t index = 0; index < units.size(); ++index) {
        const auto& unit = units[index];
        if (!unit.is_alive()) {
            intents.push_back(MotionIntent{{}, unit.facing_angle(),
                                           MovementState::idle,
                                           CombatMovementState::inactive});
            continue;
        }

        const float advance_x = unit.team() == Team::team_a ? 1.0F : -1.0F;
        const bool reached_edge =
            (unit.team() == Team::team_a && unit.position().x >= World::width - world_margin) ||
            (unit.team() == Team::team_b && unit.position().x <= world_margin);
        Vec2 velocity{};
        float desired_facing = unit.facing_angle();
        MovementState state = MovementState::idle;
        CombatMovementState combat_state = CombatMovementState::advancing;
        const Vec2 separation = separation_for(unit, units);
        if (unit.team() != Team::none && !reached_edge) {
            const float y_error = unit.preferred_y() - unit.position().y;
            Vec2 steering{
                advance_x,
                std::clamp(y_error / preferred_y_scale, -maximum_y_correction,
                           maximum_y_correction),
            };
            steering = steering + separation;
            const Vec2 direction = normalized(steering);
            velocity = direction * unit.move_speed();
            desired_facing = facing_from_direction(direction);
            state = MovementState::moving;
        }

        if (target_ids[index].has_value()) {
            const Unit* target = world_.find_unit(*target_ids[index]);
            const Vec2 target_direction = target->position() - unit.position();
            combat_state = combat_movement_for(unit, *target);
            if (length_squared(target_direction) > 0.0001F) {
                const Vec2 toward_target = normalized(target_direction);
                desired_facing = facing_from_direction(target_direction);
                switch (combat_state) {
                case CombatMovementState::inactive:
                    break;
                case CombatMovementState::closing:
                    velocity = velocity_from_steering(
                        toward_target + separation,
                        unit.move_speed() * std::clamp(unit.aggression(), 0.0F, 1.0F));
                    break;
                case CombatMovementState::engaging:
                    velocity = soft_separation_velocity(separation, unit.move_speed());
                    break;
                case CombatMovementState::retreating:
                    velocity = velocity_from_steering(
                        toward_target * -1.0F + separation,
                        unit.move_speed() * std::clamp(unit.retreat_bias(), 0.0F, 1.0F));
                    break;
                case CombatMovementState::advancing:
                    break;
                }
                state = length_squared(velocity) > 0.0001F
                            ? MovementState::moving
                            : MovementState::idle;
            } else {
                velocity = soft_separation_velocity(separation, unit.move_speed());
                state = length_squared(velocity) > 0.0001F
                            ? MovementState::moving
                            : MovementState::idle;
            }
        }
        intents.push_back(MotionIntent{velocity, desired_facing, state,
                                       combat_state});
    }

    for (std::size_t index = 0; index < units.size(); ++index) {
        auto& unit = units[index];
        const auto& intent = intents[index];
        const auto next_position =
            unit.position() + intent.velocity * static_cast<float>(fixed_delta_seconds);
        unit.set_position(Vec2{
            std::clamp(next_position.x, world_margin, World::width - world_margin),
            std::clamp(next_position.y, world_margin, World::height - world_margin),
        });
        unit.set_target_id(target_ids[index]);
        unit.set_desired_facing_angle(intent.desired_facing);
        unit.rotate_toward_desired(fixed_delta_seconds);
        unit.set_movement_state(intent.state);
        unit.set_combat_movement_state(intent.combat_state);
    }

    for (auto& unit : units) {
        if (!unit.target_id().has_value()) {
            continue;
        }

        const Unit* target = world_.find_unit(*unit.target_id());
        if (target == nullptr || !can_fire_at(unit, *target)) {
            continue;
        }

        const Vec2 direction = normalized(target->position() - unit.position());
        world_.spawn_projectile(unit.weapon().type, unit.team(), unit.id(),
                                unit.position(),
                                direction * unit.weapon().projectile_speed,
                                unit.weapon().projectile_max_distance,
                                unit.weapon().projectile_damage);
        world_.emit_fire_event(unit);
        unit.reset_weapon_cooldown();
    }

    ++tick_count_;
}

unsigned long long Simulation::tick_count() const noexcept {
    return tick_count_;
}

} // namespace siege
