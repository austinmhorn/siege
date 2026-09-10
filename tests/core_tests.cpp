#include "core/combat_behavior.hpp"
#include "core/math.hpp"
#include "core/perception.hpp"
#include "core/projectile_collision.hpp"
#include "core/simulation.hpp"
#include "core/targeting.hpp"
#include "world/unit.hpp"
#include "world/world.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <vector>

namespace {

bool check(const bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "check failed: %s\n", message);
    }
    return condition;
}

bool near(const float left, const float right, const float tolerance = 0.01F) {
    return std::abs(left - right) <= tolerance;
}

siege::Unit test_unit(const siege::Unit::Id id, const siege::Team team,
                      const siege::Vec2 position, const float facing) {
    return siege::Unit{id, siege::TroopType::rifle, team, position,
                       72.0F, 90.0F, 500.0F, 90.0F, 110.0F,
                       280.0F, 35.0F, 0.9F, 0.75F,
                       100.0F, 20.0F,
                       siege::WeaponDefinition{
                           .type = siege::WeaponType::rifle,
                           .projectile_speed = 960.0F,
                           .fire_interval = 0.60F,
                           .range = 360.0F,
                           .firing_arc = 12.0F,
                           .projectile_max_distance = 520.0F,
                           .projectile_damage = 25.0F,
                       },
                       facing};
}

void arrange_combat_scenario(siege::World& world,
                             const siege::Vec2 observer_position,
                             const siege::Vec2 target_position) {
    auto& units = world.units();
    units[0].set_position(observer_position);
    units[1].set_position({80.0F, 880.0F});
    units[2].set_position({160.0F, 960.0F});
    units[3].set_position({240.0F, 1040.0F});
    units[4].set_position(target_position);
    units[5].set_position({1760.0F, 120.0F});
    units[6].set_position({1800.0F, 900.0F});
    units[7].set_position({1700.0F, 1020.0F});
}

void isolate_collision_units(siege::World& world) {
    auto& units = world.units();
    units[0].set_position({1000.0F, 1000.0F});
    units[1].set_position({1100.0F, 1000.0F});
    units[2].set_position({1200.0F, 1000.0F});
    units[3].set_position({1300.0F, 1000.0F});
    units[4].set_position({1600.0F, 1000.0F});
    units[5].set_position({1700.0F, 1000.0F});
    units[6].set_position({1800.0F, 1000.0F});
    units[7].set_position({1900.0F, 1000.0F});
}

} // namespace

int main() {
    using namespace siege;

    bool passed = true;
    Unit clockwise_wrap = test_unit(100, Team::team_a, {}, 350.0F);
    clockwise_wrap.set_desired_facing_angle(10.0F);
    clockwise_wrap.rotate_toward_desired(0.1);
    passed &= check(near(clockwise_wrap.facing_angle(), 359.0F),
                    "rotation takes the shortest positive wraparound step");

    Unit counterclockwise_wrap = test_unit(101, Team::team_b, {}, 10.0F);
    counterclockwise_wrap.set_desired_facing_angle(350.0F);
    counterclockwise_wrap.rotate_toward_desired(0.1);
    passed &= check(near(counterclockwise_wrap.facing_angle(), 1.0F),
                    "rotation takes the shortest negative wraparound step");

    const Unit observer = test_unit(200, Team::team_a, {100.0F, 100.0F}, 270.0F);
    const Unit ahead = test_unit(201, Team::team_b, {300.0F, 100.0F}, 90.0F);
    passed &= check(inside_vision_cone(observer, ahead),
                    "ahead and in range is inside vision cone");
    passed &= check(can_perceive(observer, ahead),
                    "ahead and in range is perceptible");

    const Unit out_of_range =
        test_unit(202, Team::team_b, {601.0F, 100.0F}, 90.0F);
    passed &= check(!inside_vision_cone(observer, out_of_range),
                    "ahead and out of range is outside vision cone");
    passed &= check(!can_perceive(observer, out_of_range),
                    "ahead and out of all ranges is not perceptible");

    const Unit outside_angle =
        test_unit(203, Team::team_b, {200.0F, 300.0F}, 90.0F);
    passed &= check(!inside_vision_cone(observer, outside_angle),
                    "target outside cone angle is not visible through cone");

    const Unit behind = test_unit(204, Team::team_b, {0.0F, 100.0F}, 90.0F);
    passed &= check(!inside_vision_cone(observer, behind),
                    "target behind observer is not visible through cone");
    passed &= check(inside_awareness_radius(observer, behind),
                    "target behind and inside awareness radius is aware");
    passed &= check(can_perceive(observer, behind),
                    "awareness makes a close target behind perceptible");

    const Unit wrap_observer = test_unit(205, Team::team_a, {}, 359.0F);
    const Vec2 wrap_position = direction_from_facing(1.0F) * 200.0F;
    const Unit wrap_target = test_unit(206, Team::team_b, wrap_position, 180.0F);
    passed &= check(inside_vision_cone(wrap_observer, wrap_target),
                    "vision cone handles zero-degree wraparound");

    const Unit range_boundary =
        test_unit(207, Team::team_b, {600.0F, 100.0F}, 90.0F);
    passed &= check(inside_vision_cone(observer, range_boundary),
                    "vision range boundary is inclusive");
    const Vec2 cone_boundary_position =
        observer.position() + direction_from_facing(315.0F) * 300.0F;
    const Unit cone_boundary =
        test_unit(208, Team::team_b, cone_boundary_position, 90.0F);
    passed &= check(inside_vision_cone(observer, cone_boundary),
                    "vision angular boundary is inclusive");
    const Unit awareness_boundary =
        test_unit(209, Team::team_b, {-10.0F, 100.0F}, 90.0F);
    passed &= check(inside_awareness_radius(observer, awareness_boundary),
                    "awareness radius boundary is inclusive");

    const Unit friendly = test_unit(210, Team::team_a, {200.0F, 100.0F}, 90.0F);
    passed &= check(!can_perceive(observer, friendly),
                    "perception queries reject friendly units");

    std::vector<Unit> acquisition_units{
        test_unit(300, Team::team_a, {100.0F, 100.0F}, 270.0F),
        test_unit(302, Team::team_b, {350.0F, 100.0F}, 90.0F),
        test_unit(301, Team::team_b, {250.0F, 100.0F}, 90.0F),
    };
    passed &= check(select_target(acquisition_units[0], acquisition_units) == 301,
                    "nearest visible enemy is acquired");

    std::vector<Unit> visibility_units{
        test_unit(310, Team::team_a, {100.0F, 100.0F}, 270.0F),
        test_unit(311, Team::team_b, {-50.0F, 100.0F}, 90.0F),
        test_unit(312, Team::team_b, {300.0F, 100.0F}, 90.0F),
        test_unit(309, Team::team_a, {150.0F, 100.0F}, 90.0F),
    };
    passed &= check(select_target(visibility_units[0], visibility_units) == 312,
                    "closer invisible enemy and friendly unit are ignored");

    std::vector<Unit> friendly_units{
        test_unit(313, Team::team_a, {100.0F, 100.0F}, 270.0F),
        test_unit(314, Team::team_a, {150.0F, 100.0F}, 90.0F),
        test_unit(315, Team::team_b, {300.0F, 100.0F}, 90.0F),
    };
    passed &= check(select_target(friendly_units[0], friendly_units) == 315,
                    "closer friendly unit is ignored during acquisition");

    std::vector<Unit> persistence_units{
        test_unit(320, Team::team_a, {100.0F, 100.0F}, 270.0F),
        test_unit(321, Team::team_b, {350.0F, 100.0F}, 90.0F),
        test_unit(322, Team::team_b, {200.0F, 100.0F}, 90.0F),
    };
    persistence_units[0].set_target_id(321);
    passed &= check(select_target(persistence_units[0], persistence_units) == 321,
                    "current target is retained while perceptible");
    persistence_units[1].set_position({700.0F, 100.0F});
    passed &= check(select_target(persistence_units[0], persistence_units) == 322,
                    "new target is acquired after current target is lost");
    persistence_units[2].set_position({800.0F, 100.0F});
    passed &= check(!select_target(persistence_units[0], persistence_units).has_value(),
                    "target is cleared when no enemy remains perceptible");

    std::vector<Unit> tie_units{
        test_unit(330, Team::team_a, {0.0F, 0.0F}, 0.0F),
        test_unit(332, Team::team_b, {-100.0F, 100.0F}, 180.0F),
        test_unit(331, Team::team_b, {100.0F, 100.0F}, 180.0F),
    };
    passed &= check(select_target(tie_units[0], tie_units) == 331,
                    "equal-distance target tie selects the lowest unit ID");

    World facing_world;
    facing_world.units()[4].set_position({150.0F, 350.0F});
    facing_world.units()[5].set_position({1800.0F, 500.0F});
    facing_world.units()[6].set_position({1800.0F, 700.0F});
    facing_world.units()[7].set_position({1800.0F, 900.0F});
    Simulation facing_simulation{facing_world};
    facing_simulation.update(1.0 / 60.0);
    passed &= check(facing_world.units()[0].target_id() ==
                        facing_world.units()[4].id(),
                    "simulation stores the acquired target ID");
    passed &= check(near(facing_world.units()[0].desired_facing_angle(), 0.0F),
                    "desired facing points toward the acquired target");

    World no_target_world;
    const Vec2 no_target_start = no_target_world.units()[0].position();
    Simulation no_target_simulation{no_target_world};
    no_target_simulation.update(1.0 / 60.0);
    passed &= check(no_target_world.units()[0].position().x > no_target_start.x,
                    "unit without target continues normal advance");
    passed &= check(no_target_world.units()[0].combat_movement_state() ==
                        CombatMovementState::advancing,
                    "unit without target reports advancing");

    World closing_world;
    arrange_combat_scenario(closing_world, {500.0F, 200.0F},
                            {500.0F, 600.0F});
    const float closing_start_y = closing_world.units()[0].position().y;
    Simulation closing_simulation{closing_world};
    closing_simulation.update(1.0 / 60.0);
    passed &= check(closing_world.units()[0].position().y > closing_start_y,
                    "target beyond range makes observer close distance");
    passed &= check(closing_world.units()[0].combat_movement_state() ==
                        CombatMovementState::closing,
                    "target beyond range reports closing");

    World engaging_world;
    arrange_combat_scenario(engaging_world, {500.0F, 200.0F},
                            {500.0F, 480.0F});
    const Vec2 engaging_start = engaging_world.units()[0].position();
    Simulation engaging_simulation{engaging_world};
    engaging_simulation.update(1.0 / 60.0);
    passed &= check(length(engaging_world.units()[0].position() - engaging_start) <
                        0.001F,
                    "target inside range band makes observer hold position");
    passed &= check(engaging_world.units()[0].combat_movement_state() ==
                        CombatMovementState::engaging,
                    "target inside range band reports engaging");

    World retreating_world;
    arrange_combat_scenario(retreating_world, {500.0F, 200.0F},
                            {500.0F, 300.0F});
    const float retreat_start_y = retreating_world.units()[0].position().y;
    Simulation retreating_simulation{retreating_world};
    retreating_simulation.update(1.0 / 60.0);
    passed &= check(retreating_world.units()[0].position().y < retreat_start_y,
                    "target too close makes observer retreat");
    passed &= check(retreating_world.units()[0].combat_movement_state() ==
                        CombatMovementState::retreating,
                    "target too close reports retreating");

    World target_loss_world;
    arrange_combat_scenario(target_loss_world, {500.0F, 400.0F},
                            {500.0F, 1000.0F});
    target_loss_world.units()[0].set_target_id(target_loss_world.units()[4].id());
    const Vec2 target_loss_start = target_loss_world.units()[0].position();
    Simulation target_loss_simulation{target_loss_world};
    target_loss_simulation.update(1.0 / 60.0);
    passed &= check(!target_loss_world.units()[0].target_id().has_value(),
                    "lost target is cleared before positioning");
    passed &= check(target_loss_world.units()[0].position().x > target_loss_start.x &&
                        target_loss_world.units()[0].position().y < target_loss_start.y,
                    "target loss resumes advance and preferred_y return");

    const Unit boundary_observer =
        test_unit(340, Team::team_a, {100.0F, 100.0F}, 0.0F);
    const Unit upper_boundary_target =
        test_unit(341, Team::team_b, {100.0F, 415.0F}, 180.0F);
    const Unit lower_boundary_target =
        test_unit(342, Team::team_b, {100.0F, 345.0F}, 180.0F);
    for (int repeat = 0; repeat < 10; ++repeat) {
        passed &= check(combat_movement_for(boundary_observer,
                                            upper_boundary_target) ==
                            CombatMovementState::engaging &&
                            combat_movement_for(boundary_observer,
                                                lower_boundary_target) ==
                            CombatMovementState::engaging,
                        "range-band boundaries remain deterministically engaging");
    }

    World separation_world;
    arrange_combat_scenario(separation_world, {500.0F, 200.0F},
                            {500.0F, 480.0F});
    separation_world.units()[1].set_position({520.0F, 200.0F});
    const float separated_start_x = separation_world.units()[0].position().x;
    Simulation separation_simulation{separation_world};
    separation_simulation.update(1.0 / 60.0);
    passed &= check(separation_world.units()[0].position().x < separated_start_x,
                    "separation still moves an engaging unit away from a nearby friendly");

    World no_fire_world;
    Simulation no_fire_simulation{no_fire_world};
    no_fire_simulation.update(1.0 / 60.0);
    passed &= check(no_fire_world.projectiles().empty(),
                    "unit without a target does not fire");

    Unit weapon_observer =
        test_unit(350, Team::team_a, {100.0F, 100.0F}, 0.0F);
    const Unit weapon_out_of_range =
        test_unit(351, Team::team_b, {100.0F, 461.0F}, 180.0F);
    weapon_observer.set_target_id(weapon_out_of_range.id());
    passed &= check(!can_fire_at(weapon_observer, weapon_out_of_range),
                    "target outside weapon range cannot be fired upon");

    const Unit weapon_outside_arc =
        test_unit(352, Team::team_b, {200.0F, 100.0F}, 180.0F);
    weapon_observer.set_target_id(weapon_outside_arc.id());
    passed &= check(!can_fire_at(weapon_observer, weapon_outside_arc),
                    "target outside current-facing firing arc cannot be fired upon");

    World firing_world;
    arrange_combat_scenario(firing_world, {500.0F, 200.0F},
                            {500.0F, 480.0F});
    Simulation firing_simulation{firing_world};
    firing_simulation.update(1.0 / 60.0);
    passed &= check(firing_world.projectiles().size() == 1,
                    "valid aligned target creates one projectile");
    const Projectile::Id first_projectile_id =
        firing_world.projectiles().front().id();
    const Vec2 fired_position = firing_world.projectiles().front().position();
    const Vec2 fired_velocity = firing_world.projectiles().front().velocity();
    const Vec2 direction_to_target = normalized(
        firing_world.units()[4].position() - fired_position);
    passed &= check(dot(normalized(fired_velocity), direction_to_target) > 0.999F,
                    "projectile direction points toward target");

    firing_simulation.update(1.0 / 60.0);
    passed &= check(firing_world.projectiles().size() == 1,
                    "weapon cooldown prevents an immediate repeat shot");
    passed &= check(near(
                        length(firing_world.projectiles().front().position() -
                               fired_position),
                        length(fired_velocity) / 60.0F, 0.02F),
                    "projectile advances by velocity times fixed timestep");

    for (int tick = 1; tick < 35; ++tick) {
        firing_simulation.update(1.0 / 60.0);
    }
    passed &= check(firing_world.units()[0].weapon_cooldown_remaining() > 0.0F,
                    "cooldown remains active before full fire interval");
    firing_simulation.update(1.0 / 60.0);
    passed &= check(!firing_world.projectiles().empty() &&
                        firing_world.projectiles().back().id() > first_projectile_id,
                    "fixed-step cooldown permits the next shot at the fire interval");

    World expired_projectile_world;
    expired_projectile_world.spawn_projectile(
        WeaponType::rifle, Team::team_a,
        expired_projectile_world.units()[0].id(), {100.0F, 100.0F},
        {100.0F, 0.0F}, 1.0F, 25.0F);
    Simulation expired_projectile_simulation{expired_projectile_world};
    expired_projectile_simulation.update(1.0 / 60.0);
    passed &= check(expired_projectile_world.projectiles().empty(),
                    "expired projectile is removed");

    World out_of_bounds_projectile_world;
    out_of_bounds_projectile_world.spawn_projectile(
        WeaponType::rifle, Team::team_b,
        out_of_bounds_projectile_world.units()[4].id(), {1919.0F, 100.0F},
        {120.0F, 0.0F}, 100.0F, 25.0F);
    Simulation out_of_bounds_projectile_simulation{out_of_bounds_projectile_world};
    out_of_bounds_projectile_simulation.update(1.0 / 60.0);
    passed &= check(out_of_bounds_projectile_world.projectiles().empty(),
                    "out-of-bounds projectile is removed");

    const auto direct_sweep = swept_circle_hit_fraction(
        {100.0F, 100.0F}, {300.0F, 100.0F}, {200.0F, 100.0F}, 20.0F);
    passed &= check(direct_sweep.has_value() && near(*direct_sweep, 0.4F),
                    "swept collision returns the first segment hit fraction");

    World hostile_hit_world;
    isolate_collision_units(hostile_hit_world);
    hostile_hit_world.units()[4].set_position({200.0F, 100.0F});
    hostile_hit_world.spawn_projectile(
        WeaponType::rifle, Team::team_a, hostile_hit_world.units()[0].id(),
        {100.0F, 100.0F}, {12000.0F, 0.0F}, 1000.0F, 25.0F);
    Simulation hostile_hit_simulation{hostile_hit_world};
    hostile_hit_simulation.update(1.0 / 60.0);
    passed &= check(near(hostile_hit_world.units()[4].health(), 75.0F),
                    "hostile projectile hit reduces health");
    passed &= check(hostile_hit_world.projectiles().empty(),
                    "projectile disappears after its first hit");

    World friendly_hit_world;
    isolate_collision_units(friendly_hit_world);
    friendly_hit_world.units()[1].set_position({200.0F, 100.0F});
    friendly_hit_world.spawn_projectile(
        WeaponType::rifle, Team::team_a, friendly_hit_world.units()[0].id(),
        {100.0F, 100.0F}, {12000.0F, 0.0F}, 1000.0F, 25.0F);
    Simulation friendly_hit_simulation{friendly_hit_world};
    friendly_hit_simulation.update(1.0 / 60.0);
    passed &= check(near(friendly_hit_world.units()[1].health(), 100.0F),
                    "friendly projectile cannot damage a friendly unit");

    World self_hit_world;
    isolate_collision_units(self_hit_world);
    self_hit_world.units()[0].set_position({200.0F, 100.0F});
    self_hit_world.spawn_projectile(
        WeaponType::rifle, Team::team_a, self_hit_world.units()[0].id(),
        {100.0F, 100.0F}, {12000.0F, 0.0F}, 1000.0F, 25.0F);
    Simulation self_hit_simulation{self_hit_world};
    self_hit_simulation.update(1.0 / 60.0);
    passed &= check(near(self_hit_world.units()[0].health(), 100.0F),
                    "projectile cannot damage its source unit");

    World swept_hit_world;
    isolate_collision_units(swept_hit_world);
    swept_hit_world.units()[4].set_position({200.0F, 100.0F});
    swept_hit_world.spawn_projectile(
        WeaponType::rifle, Team::team_a, swept_hit_world.units()[0].id(),
        {100.0F, 100.0F}, {12000.0F, 0.0F}, 1000.0F, 25.0F);
    Simulation swept_hit_simulation{swept_hit_world};
    swept_hit_simulation.update(1.0 / 60.0);
    passed &= check(near(swept_hit_world.units()[4].health(), 75.0F),
                    "swept collision catches a projectile crossing between ticks");

    World nearest_hit_world;
    isolate_collision_units(nearest_hit_world);
    nearest_hit_world.units()[4].set_position({180.0F, 100.0F});
    nearest_hit_world.units()[5].set_position({240.0F, 100.0F});
    nearest_hit_world.spawn_projectile(
        WeaponType::rifle, Team::team_a, nearest_hit_world.units()[0].id(),
        {100.0F, 100.0F}, {12000.0F, 0.0F}, 1000.0F, 25.0F);
    Simulation nearest_hit_simulation{nearest_hit_world};
    nearest_hit_simulation.update(1.0 / 60.0);
    passed &= check(near(nearest_hit_world.units()[4].health(), 75.0F) &&
                        near(nearest_hit_world.units()[5].health(), 100.0F),
                    "projectile damages only the nearest intersected enemy");

    Unit defeated = test_unit(400, Team::team_b, {200.0F, 100.0F}, 90.0F);
    defeated.apply_damage(125.0F);
    passed &= check(near(defeated.health(), 0.0F),
                    "damage clamps health at zero");
    passed &= check(!defeated.is_alive(),
                    "zero-health unit becomes non-alive");

    World dead_unit_world;
    arrange_combat_scenario(dead_unit_world, {500.0F, 200.0F},
                            {500.0F, 480.0F});
    auto& dead_unit = dead_unit_world.units()[0];
    const Vec2 dead_position = dead_unit.position();
    dead_unit.set_target_id(dead_unit_world.units()[4].id());
    dead_unit.apply_damage(dead_unit.max_health());
    Simulation dead_unit_simulation{dead_unit_world};
    dead_unit_simulation.update(1.0 / 60.0);
    passed &= check(length(dead_unit.position() - dead_position) < 0.001F &&
                        dead_unit.movement_state() == MovementState::idle &&
                        dead_unit.combat_movement_state() ==
                            CombatMovementState::inactive,
                    "dead unit cannot move and reports inactive");
    passed &= check(!dead_unit.target_id().has_value() &&
                        !select_target(dead_unit, dead_unit_world.units()).has_value(),
                    "dead unit cannot retain or acquire a target");
    bool dead_unit_fired = false;
    for (const auto& projectile : dead_unit_world.projectiles()) {
        dead_unit_fired |= projectile.source_unit_id() == dead_unit.id();
    }
    passed &= check(!dead_unit_fired, "dead unit cannot fire");

    std::vector<Unit> dead_target_units{
        test_unit(410, Team::team_a, {100.0F, 100.0F}, 270.0F),
        test_unit(411, Team::team_b, {200.0F, 100.0F}, 90.0F),
        test_unit(412, Team::team_b, {300.0F, 100.0F}, 90.0F),
    };
    dead_target_units[0].set_target_id(411);
    dead_target_units[1].apply_damage(dead_target_units[1].max_health());
    passed &= check(select_target(dead_target_units[0], dead_target_units) == 412,
                    "dead target is cleared and another valid target is acquired");
    passed &= check(!can_perceive(dead_target_units[0], dead_target_units[1]),
                    "dead unit cannot be targeted through perception");

    World world;
    Simulation simulation{world};
    std::array<Vec2, 8> spawn_positions{};
    for (std::size_t index = 0; index < world.units().size(); ++index) {
        spawn_positions[index] = world.units()[index].position();
    }

    simulation.update(1.0 / 60.0);
    for (const auto& unit : world.units()) {
        passed &= check(std::abs(shortest_angle_delta(unit.facing_angle(),
                                                      unit.desired_facing_angle())) > 1.0F,
                        "facing rotates gradually instead of snapping");
    }

    for (int tick = 1; tick < 300; ++tick) {
        simulation.update(1.0 / 60.0);
    }

    for (std::size_t index = 0; index < world.units().size(); ++index) {
        const auto& unit = world.units()[index];
        const float x_delta = unit.position().x - spawn_positions[index].x;
        if (unit.team() == Team::team_a) {
            passed &= check(x_delta > 300.0F, "team_a advances toward increasing x");
        } else {
            passed &= check(x_delta < -300.0F, "team_b advances toward decreasing x");
        }
        passed &= check(std::abs(unit.position().y - unit.preferred_y()) < 55.0F,
                        "unit remains near its spawn preferred_y");
        passed &= check(unit.movement_state() == MovementState::moving,
                        "unit reports moving while advancing");
    }

    const float team_a_spawn_pair_distance =
        length(world.units()[0].position() - world.units()[1].position());
    const float team_b_spawn_pair_distance =
        length(world.units()[4].position() - world.units()[5].position());
    passed &= check(team_a_spawn_pair_distance > 25.0F,
                    "overlapping team_a units softly separate");
    passed &= check(team_b_spawn_pair_distance > 25.0F,
                    "overlapping team_b units softly separate");

    if (!passed) {
        return 1;
    }

    std::printf("core movement checks passed at tick %llu\n", simulation.tick_count());
    return 0;
}
