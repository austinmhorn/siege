#include "core/simulation.hpp"

#include "core/combat_behavior.hpp"
#include "core/deployment.hpp"
#include "core/economy.hpp"
#include "core/environment_collision.hpp"
#include "core/frontline.hpp"
#include "core/movement_path.hpp"
#include "core/projectile_collision.hpp"
#include "core/scoring.hpp"
#include "core/support_positioning.hpp"
#include "core/tactical_command.hpp"
#include "core/targeting.hpp"
#include "core/weapon.hpp"
#include "core/zone_capture.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
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
    std::optional<Unit::Id> support_screen_id;
    Vec2 support_steering;
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

Vec2 weighted_steering_velocity(const Vec2 steering,
                                const float move_speed) noexcept {
    return velocity_from_steering(
        steering, move_speed * std::clamp(length(steering), 0.0F, 1.0F));
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

float team_advance_direction(const World& world, const Team team) noexcept {
    const TeamForwardDefinition* forward =
        team_forward_definition(world.map(), team);
    return forward == nullptr ? 0.0F : forward->x_direction;
}

Vec2 hold_velocity_for(const Unit& unit, const Vec2 desired_velocity,
                       const Vec2 separation) noexcept {
    if (!unit.tactical_position().has_value()) {
        return desired_velocity;
    }
    const Vec2 toward_anchor = *unit.tactical_position() - unit.position();
    const float distance = length(toward_anchor);
    const float return_start = default_tactical_rules.hold_leash_radius *
                               default_tactical_rules.hold_return_start_fraction;
    if (distance <= return_start) {
        return desired_velocity;
    }
    const float blend_distance = std::max(
        1.0F, default_tactical_rules.hold_leash_radius - return_start);
    const float return_weight =
        std::clamp((distance - return_start) / blend_distance, 0.0F, 1.0F);
    const Vec2 return_velocity = velocity_from_steering(
        toward_anchor + separation, unit.move_speed());
    return lerp(desired_velocity, return_velocity, return_weight);
}

bool returning_to_hold_anchor(const Unit& unit) noexcept {
    if (!unit.tactical_position().has_value()) {
        return false;
    }
    const float return_start = default_tactical_rules.hold_leash_radius *
                               default_tactical_rules.hold_return_start_fraction;
    return length(*unit.tactical_position() - unit.position()) > return_start;
}

Vec2 constrain_to_hold_leash(const Unit& unit, const Vec2 position) noexcept {
    if (unit.tactical_order() != TacticalOrder::hold ||
        !unit.tactical_position().has_value()) {
        return position;
    }
    const Vec2 offset = position - *unit.tactical_position();
    const float distance = length(offset);
    if (distance <= default_tactical_rules.hold_leash_radius) {
        return position;
    }
    return *unit.tactical_position() +
           normalized(offset) * default_tactical_rules.hold_leash_radius;
}

struct ProjectileHit {
    Unit* unit;
    float segment_fraction;
};

std::optional<ProjectileHit> nearest_projectile_hit(
    const Projectile& projectile, std::vector<Unit>& units) noexcept {
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
    if (nearest == nullptr) {
        return std::nullopt;
    }
    return ProjectileHit{nearest, nearest_fraction};
}

void apply_explosion(World& world, const Projectile& projectile,
                     const Vec2 position, std::vector<Unit>& units) noexcept {
    const float radius_squared =
        projectile.splash_radius() * projectile.splash_radius();
    for (auto& candidate : units) {
        if (!candidate.is_alive() || candidate.id() == projectile.source_unit_id() ||
            candidate.team() == Team::none || candidate.team() == projectile.team()) {
            continue;
        }
        if (length_squared(candidate.position() - position) <= radius_squared) {
            candidate.apply_damage(projectile.damage());
            if (!candidate.is_alive()) {
                (void)award_projectile_kill(world, projectile, candidate);
            }
        }
    }
}

bool outside_world(const World& world, const Vec2 position) noexcept {
    return position.x < 0.0F || position.x > world.map().logical_width ||
           position.y < 0.0F || position.y > world.map().logical_height;
}

} // namespace

Simulation::Simulation(World& world) noexcept : world_(world) {}

void Simulation::update(const double fixed_delta_seconds) noexcept {
    world_.clear_transient_events();
    if (!world_.match_state().active()) {
        return;
    }
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
        if (const auto hit = nearest_projectile_hit(*projectile, units)) {
            if (projectile->splash_radius() > 0.0F) {
                const Vec2 impact_position =
                    projectile->previous_position() +
                    (projectile->position() - projectile->previous_position()) *
                        hit->segment_fraction;
                world_.emit_explosion_event(*projectile, impact_position);
                apply_explosion(world_, *projectile, impact_position, units);
            } else {
                hit->unit->apply_damage(projectile->damage());
                if (!hit->unit->is_alive()) {
                    (void)award_projectile_kill(world_, *projectile,
                                                *hit->unit);
                }
            }
            projectile = projectiles.erase(projectile);
        } else if (projectile->expired() ||
                   outside_world(world_, projectile->position())) {
            projectile = projectiles.erase(projectile);
        } else {
            ++projectile;
        }
    }

    world_.remove_dead_units();

    for (auto& unit : units) {
        while (unit.current_waypoint().has_value() &&
               length(unit.position() - *unit.current_waypoint()) <=
                   default_movement_path_rules.waypoint_reach_radius) {
            const Vec2 reached = *unit.current_waypoint();
            const bool final_waypoint = unit.remaining_waypoint_count() == 1;
            unit.advance_movement_path();
            if (final_waypoint) {
                unit.set_preferred_y(reached.y);
                unit.set_tactical_order(TacticalOrder::automatic);
            }
        }
        if (unit.tactical_order() == TacticalOrder::regroup &&
            unit.tactical_position().has_value() &&
            length(unit.position() - *unit.tactical_position()) <=
                default_tactical_rules.regroup_completion_radius) {
            unit.set_tactical_order(TacticalOrder::automatic);
        }
    }

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
            intents.push_back(MotionIntent{
                {}, unit.facing_angle(), MovementState::idle,
                CombatMovementState::inactive, std::nullopt, {}});
            continue;
        }

        const float advance_x = unit.tactical_order() == TacticalOrder::advance
            ? team_advance_direction(world_, unit.team())
            : autonomous_advance_x(world_, unit.team(), unit.position());
        const float team_direction = team_advance_direction(world_, unit.team());
        const bool reached_edge =
            (team_direction > 0.0F &&
             unit.position().x >= world_.map().logical_width - world_margin) ||
            (team_direction < 0.0F && unit.position().x <= world_margin);
        Vec2 velocity{};
        const float forward_facing =
            team_forward_facing_angle(world_.map(), unit.team());
        float desired_facing = unit.team() == Team::none
            ? unit.facing_angle()
            : forward_facing;
        MovementState state = MovementState::idle;
        CombatMovementState combat_state = CombatMovementState::advancing;
        const Vec2 separation = separation_for(unit, units);
        SupportPositioning support =
            support_positioning_for(unit, units, world_.map());
        if (unit.team() != Team::none && !reached_edge) {
            const float y_error = unit.preferred_y() - unit.position().y;
            Vec2 primary_steering{
                advance_x,
                std::clamp(y_error / preferred_y_scale, -maximum_y_correction,
                           maximum_y_correction),
            };
            primary_steering = primary_steering + support.steering;
            const Vec2 steering = primary_steering + separation;
            velocity = length_squared(support.steering) > 0.0001F
                           ? weighted_steering_velocity(steering,
                                                        unit.move_speed())
                           : velocity_from_steering(steering,
                                                    unit.move_speed());
            if (length_squared(velocity) > 0.0001F) {
                if (length_squared(primary_steering) > 0.0001F) {
                    desired_facing = facing_from_direction(velocity);
                }
                state = MovementState::moving;
            }
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
                    velocity = length_squared(support.steering) > 0.0001F
                        ? weighted_steering_velocity(
                              toward_target + separation + support.steering,
                              unit.move_speed() *
                                  std::clamp(unit.aggression(), 0.0F, 1.0F))
                        : velocity_from_steering(
                              toward_target + separation,
                              unit.move_speed() *
                                  std::clamp(unit.aggression(), 0.0F, 1.0F));
                    break;
                case CombatMovementState::engaging:
                    velocity = soft_separation_velocity(
                        separation + support.steering, unit.move_speed());
                    break;
                case CombatMovementState::retreating:
                    support = {};
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

        if (unit.tactical_order() == TacticalOrder::hold) {
            support = {};
            const Vec2 desired_velocity = target_ids[index].has_value()
                ? velocity
                : soft_separation_velocity(separation, unit.move_speed());
            velocity = hold_velocity_for(unit, desired_velocity, separation);
            if (!target_ids[index].has_value()) {
                desired_facing = forward_facing;
                if (returning_to_hold_anchor(unit) &&
                    length_squared(velocity) > 0.0001F) {
                    desired_facing = facing_from_direction(velocity);
                }
            }
            state = length_squared(velocity) > 0.0001F
                        ? MovementState::moving
                        : MovementState::idle;
        } else if (unit.tactical_order() == TacticalOrder::regroup &&
                   unit.tactical_position().has_value()) {
            support = {};
            const Vec2 toward_regroup =
                *unit.tactical_position() - unit.position();
            velocity = velocity_from_steering(toward_regroup + separation,
                                              unit.move_speed());
            if (!target_ids[index].has_value() &&
                length_squared(toward_regroup) > 0.0001F) {
                desired_facing = facing_from_direction(toward_regroup);
            }
            state = length_squared(velocity) > 0.0001F
                        ? MovementState::moving
                        : MovementState::idle;
        }

        if (unit.current_waypoint().has_value()) {
            support = {};
            const Vec2 toward_waypoint = *unit.current_waypoint() - unit.position();
            const bool immediate_combat_danger =
                target_ids[index].has_value() &&
                combat_state == CombatMovementState::retreating;
            if (!immediate_combat_danger) {
                velocity = velocity_from_steering(toward_waypoint + separation,
                                                  unit.move_speed());
                if (!target_ids[index].has_value() &&
                    length_squared(toward_waypoint) > 0.0001F) {
                    desired_facing = facing_from_direction(toward_waypoint);
                }
                state = length_squared(velocity) > 0.0001F
                            ? MovementState::moving
                            : MovementState::idle;
            }
        }
        intents.push_back(MotionIntent{velocity, desired_facing, state,
                                       combat_state, support.screen_id,
                                       support.steering});
    }

    for (std::size_t index = 0; index < units.size(); ++index) {
        auto& unit = units[index];
        const auto& intent = intents[index];
        const auto unconstrained_position =
            unit.position() + intent.velocity * static_cast<float>(fixed_delta_seconds);
        const auto frontline_position = constrain_to_frontline(
            world_, unit.team(), unit.position(), unconstrained_position);
        const auto next_position = constrain_to_hold_leash(unit, frontline_position);
        const Vec2 world_position{
            std::clamp(next_position.x, world_margin,
                       world_.map().logical_width - world_margin),
            std::clamp(next_position.y, world_margin,
                       world_.map().logical_height - world_margin),
        };
        const Vec2 final_position = resolve_unit_environment_movement(
            world_.map(), unit.position(), world_position, unit.hit_radius());
        const bool moved =
            length_squared(final_position - unit.position()) > 0.0001F;
        unit.set_position(final_position);
        unit.set_target_id(target_ids[index]);
        unit.set_desired_facing_angle(intent.desired_facing);
        unit.rotate_toward_desired(fixed_delta_seconds);
        unit.set_movement_state(
            intent.state == MovementState::moving && moved
                ? MovementState::moving
                : MovementState::idle);
        unit.set_combat_movement_state(intent.combat_state);
        unit.set_support_positioning(intent.support_screen_id,
                                     intent.support_steering);
    }

    update_zone_capture(world_, fixed_delta_seconds);
    award_zone_capture_rewards(world_);
    if (resolve_sudden_death_center_capture(world_)) {
        ++tick_count_;
        return;
    }
    update_objective_scoring(world_);
    update_passive_income(world_);

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
                                unit.weapon().projectile_damage,
                                unit.weapon().splash_radius);
        world_.emit_fire_event(unit);
        unit.reset_weapon_cooldown();
    }

    update_pending_deployments(world_, fixed_delta_seconds);

    const PlayerState* team_a = world_.find_player(Team::team_a);
    const PlayerState* team_b = world_.find_player(Team::team_b);
    const Score team_a_score = team_a == nullptr ? 0 : team_a->score();
    const Score team_b_score = team_b == nullptr ? 0 : team_b->score();
    const MatchTransition transition =
        world_.match_state().advance(1, team_a_score, team_b_score);
    if (transition == MatchTransition::sudden_death) {
        world_.reset_for_sudden_death();
    }

    ++tick_count_;
}

unsigned long long Simulation::tick_count() const noexcept {
    return tick_count_;
}

} // namespace siege
