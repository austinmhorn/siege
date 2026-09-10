#include "core/combat_behavior.hpp"
#include "core/math.hpp"
#include "core/perception.hpp"
#include "core/projectile_collision.hpp"
#include "core/simulation.hpp"
#include "core/support_positioning.hpp"
#include "core/targeting.hpp"
#include "core/troop_definition.hpp"
#include "core/zone_capture.hpp"
#include "world/unit.hpp"
#include "world/world.hpp"

#include <algorithm>
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
                       1.0F, 0.0F, 0.0F, 0.0F,
                       100.0F, 20.0F,
                       siege::WeaponDefinition{
                           .type = siege::WeaponType::rifle,
                           .projectile_speed = 960.0F,
                           .fire_interval = 0.60F,
                           .range = 360.0F,
                           .firing_arc = 12.0F,
                           .projectile_max_distance = 520.0F,
                           .projectile_damage = 25.0F,
                           .splash_radius = 0.0F,
                       },
                       facing};
}

siege::Unit unit_from_definition(const siege::Unit::Id id,
                                 const siege::Team team,
                                 const siege::Vec2 position,
                                 const float facing,
                                 const siege::TroopDefinition& definition) {
    return siege::Unit{id, definition.type, team, position,
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
                       definition.hit_radius, definition.weapon, facing};
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
    units[8].set_position({300.0F, 980.0F});
    units[9].set_position({500.0F, 980.0F});
    units[10].set_position({1500.0F, 980.0F});
    units[11].set_position({1650.0F, 980.0F});
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
    units[8].set_position({500.0F, 900.0F});
    units[9].set_position({700.0F, 900.0F});
    units[10].set_position({1400.0F, 900.0F});
    units[11].set_position({1500.0F, 900.0F});
}

} // namespace

int main() {
    using namespace siege;

    bool passed = true;
    passed &= check(rifle_definition.type == TroopType::rifle &&
                        near(rifle_definition.move_speed, 72.0F) &&
                        near(rifle_definition.rotation_speed, 90.0F) &&
                        near(rifle_definition.preferred_combat_range, 280.0F) &&
                        near(rifle_definition.weapon.fire_interval, 0.60F) &&
                        near(rifle_definition.weapon.projectile_damage, 25.0F) &&
                        near(rifle_definition.aggression, 0.9F) &&
                        near(rifle_definition.retreat_bias, 0.75F) &&
                        near(rifle_definition.frontline_screen_weight, 1.0F) &&
                        near(rifle_definition.support_positioning_bias, 0.0F) &&
                        near(rifle_definition.zone_control_weight, 1.0F),
                    "rifle definition remains unchanged");
    passed &= check(machine_gun_definition.type == TroopType::machine_gun &&
                        machine_gun_definition.weapon.type ==
                            WeaponType::machine_gun &&
                        near(machine_gun_definition.move_speed, 54.0F) &&
                        near(machine_gun_definition.rotation_speed, 60.0F) &&
                        near(machine_gun_definition.vision_range, 600.0F) &&
                        near(machine_gun_definition.preferred_combat_range, 390.0F) &&
                        near(machine_gun_definition.range_tolerance, 45.0F) &&
                        near(machine_gun_definition.aggression, 0.62F) &&
                        near(machine_gun_definition.retreat_bias, 0.85F) &&
                        near(machine_gun_definition.support_positioning_bias, 0.85F) &&
                        near(machine_gun_definition.support_rear_distance, 120.0F) &&
                        near(machine_gun_definition.support_search_radius, 420.0F) &&
                        near(machine_gun_definition.weapon.fire_interval, 0.18F) &&
                        near(machine_gun_definition.weapon.range, 480.0F) &&
                        near(machine_gun_definition.weapon.projectile_damage, 10.0F) &&
                        near(machine_gun_definition.zone_control_weight, 1.0F),
                    "machine_gun definition exposes its distinct gameplay profile");
    passed &= check(bazooka_definition.type == TroopType::bazooka &&
                        bazooka_definition.weapon.type == WeaponType::bazooka &&
                        near(bazooka_definition.move_speed, 60.0F) &&
                        near(bazooka_definition.rotation_speed, 50.0F) &&
                        near(bazooka_definition.preferred_combat_range, 520.0F) &&
                        near(bazooka_definition.aggression, 0.50F) &&
                        near(bazooka_definition.retreat_bias, 1.0F) &&
                        near(bazooka_definition.support_positioning_bias, 1.10F) &&
                        near(bazooka_definition.support_rear_distance, 180.0F) &&
                        near(bazooka_definition.support_search_radius, 500.0F) &&
                        near(bazooka_definition.weapon.fire_interval, 2.60F) &&
                        near(bazooka_definition.weapon.range, 650.0F) &&
                        near(bazooka_definition.weapon.projectile_speed, 480.0F) &&
                        near(bazooka_definition.weapon.projectile_damage, 70.0F) &&
                        near(bazooka_definition.weapon.splash_radius, 115.0F) &&
                        near(bazooka_definition.max_health, 80.0F) &&
                        near(bazooka_definition.zone_control_weight, 1.0F),
                    "bazooka definition exposes its explosive long-range profile");

    World presence_world;
    presence_world.units().clear();
    presence_world.units().push_back(
        test_unit(1000, Team::team_a, {500.0F, 200.0F}, 0.0F));
    presence_world.units().push_back(
        test_unit(1001, Team::team_b, {500.0F, 300.0F}, 0.0F));
    presence_world.units().back().apply_damage(100.0F);
    update_zone_capture(presence_world, 1.0);
    passed &= check(presence_world.zones()[1].team_a_count() == 1 &&
                        presence_world.zones()[1].team_b_count() == 0,
                    "living unit is counted in its objective and dead unit is ignored");

    World boundary_world;
    boundary_world.units().clear();
    boundary_world.units().push_back(
        test_unit(1002, Team::team_a, {768.0F, 200.0F}, 0.0F));
    update_zone_capture(boundary_world, 0.0);
    const auto boundary_zone = zone_index_for_position(
        boundary_world, boundary_world.units().front().position());
    int boundary_presence = 0;
    for (const auto& zone : boundary_world.zones()) {
        boundary_presence += zone.team_a_count() + zone.team_b_count();
    }
    passed &= check(boundary_zone == 2 && boundary_presence == 1 &&
                        boundary_world.zones()[1].team_a_count() == 0 &&
                        boundary_world.zones()[2].team_a_count() == 1,
                    "shared boundary belongs only to the zone on its right");

    World equal_world;
    equal_world.units().clear();
    equal_world.zones()[1].advance_capture(17.0F);
    equal_world.units().push_back(
        test_unit(1010, Team::team_a, {500.0F, 200.0F}, 0.0F));
    equal_world.units().push_back(
        test_unit(1011, Team::team_b, {500.0F, 300.0F}, 0.0F));
    update_zone_capture(equal_world, 1.0);
    passed &= check(equal_world.zones()[1].pressure() == 0 &&
                        near(equal_world.zones()[1].capture_value(), 17.0F),
                    "equal presence leaves capture unchanged");

    World advantage_world;
    advantage_world.units().clear();
    advantage_world.units().push_back(
        test_unit(1020, Team::team_a, {500.0F, 200.0F}, 0.0F));
    update_zone_capture(advantage_world, 1.0);
    passed &= check(advantage_world.zones()[1].pressure() == 1 &&
                        near(advantage_world.zones()[1].capture_value(), 5.0F),
                    "Team A numerical advantage moves capture toward +100");
    advantage_world.units().clear();
    advantage_world.units().push_back(
        test_unit(1021, Team::team_b, {500.0F, 200.0F}, 0.0F));
    advantage_world.units().push_back(
        test_unit(1022, Team::team_b, {500.0F, 300.0F}, 0.0F));
    update_zone_capture(advantage_world, 1.0);
    passed &= check(advantage_world.zones()[1].pressure() == -2 &&
                        near(advantage_world.zones()[1].capture_value(), -5.0F),
                    "Team B numerical advantage moves capture toward -100");

    World pressure_world;
    pressure_world.units().clear();
    for (Unit::Id id = 1030; id < 1036; ++id) {
        pressure_world.units().push_back(test_unit(
            id, Team::team_a,
            {500.0F, 100.0F + static_cast<float>(id - 1030) * 40.0F}, 0.0F));
    }
    update_zone_capture(pressure_world, 1.0);
    passed &= check(pressure_world.zones()[1].pressure() == 6 &&
                        near(pressure_world.zones()[1].capture_value(), 15.0F),
                    "larger advantage captures faster but maximum pressure clamps at three");

    World clamp_world;
    clamp_world.units().clear();
    clamp_world.units().push_back(
        test_unit(1040, Team::team_a, {500.0F, 200.0F}, 0.0F));
    update_zone_capture(clamp_world, 100.0);
    passed &= check(near(clamp_world.zones()[1].capture_value(), 100.0F),
                    "capture meter clamps at positive 100");
    clamp_world.units().front() =
        test_unit(1041, Team::team_b, {500.0F, 200.0F}, 0.0F);
    update_zone_capture(clamp_world, 100.0);
    passed &= check(near(clamp_world.zones()[1].capture_value(), -100.0F),
                    "capture meter clamps at negative 100");
    clamp_world.units().clear();
    update_zone_capture(clamp_world, 20.0);
    passed &= check(near(clamp_world.zones()[1].capture_value(), -100.0F),
                    "empty objective retains its capture progress");

    World home_world;
    home_world.units().clear();
    home_world.units().push_back(
        test_unit(1050, Team::team_a, {100.0F, 200.0F}, 0.0F));
    home_world.zones()[0].advance_capture(50.0F);
    update_zone_capture(home_world, 10.0);
    passed &= check(near(home_world.zones()[0].capture_value(), 0.0F) &&
                        home_world.zones()[0].team_a_count() == 0,
                    "home zones do not track capture presence or capture progress");

    World ownership_world;
    ownership_world.units().clear();
    passed &= check(ownership_world.zones()[1].owner() == Team::none &&
                        ownership_world.zones()[2].owner() == Team::none &&
                        ownership_world.zones()[3].owner() == Team::none,
                    "all objectives begin neutral");
    ownership_world.zones()[1].advance_capture(50.0F);
    update_zone_capture(ownership_world, 0.0);
    passed &= check(ownership_world.zones()[1].owner() == Team::none &&
                        ownership_world.zone_ownership_events().empty(),
                    "partial neutral progress does not assign ownership");

    ownership_world.zones()[1].advance_capture(50.0F);
    update_zone_capture(ownership_world, 0.0);
    passed &= check(
        ownership_world.zones()[1].owner() == Team::team_a &&
            ownership_world.zone_ownership_events().size() == 1 &&
            ownership_world.zone_ownership_events().front().zone_id == 1 &&
            ownership_world.zone_ownership_events().front().previous_owner ==
                Team::none &&
            ownership_world.zone_ownership_events().front().new_owner ==
                Team::team_a &&
            ownership_world.zone_ownership_events().front().type ==
                ZoneTransitionType::captured,
        "neutral objective captures for Team A at positive 100");
    ownership_world.clear_transient_events();
    update_zone_capture(ownership_world, 0.0);
    passed &= check(ownership_world.zone_ownership_events().empty(),
                    "captured boundary does not emit duplicate events");

    ownership_world.zones()[1].advance_capture(-60.0F);
    update_zone_capture(ownership_world, 0.0);
    passed &= check(ownership_world.zones()[1].owner() == Team::team_a &&
                        ownership_world.zone_ownership_events().empty(),
                    "Team A objective remains owned while its meter is positive");
    ownership_world.zones()[1].advance_capture(-40.0F);
    update_zone_capture(ownership_world, 0.0);
    passed &= check(
        ownership_world.zones()[1].owner() == Team::none &&
            ownership_world.zone_ownership_events().size() == 1 &&
            ownership_world.zone_ownership_events().front().previous_owner ==
                Team::team_a &&
            ownership_world.zone_ownership_events().front().new_owner ==
                Team::none &&
            ownership_world.zone_ownership_events().front().type ==
                ZoneTransitionType::neutralized,
        "Team A objective neutralizes when its meter reaches zero");
    ownership_world.clear_transient_events();
    update_zone_capture(ownership_world, 0.0);
    passed &= check(ownership_world.zone_ownership_events().empty(),
                    "neutral boundary does not emit duplicate events");

    ownership_world.zones()[1].advance_capture(-60.0F);
    update_zone_capture(ownership_world, 0.0);
    passed &= check(ownership_world.zones()[1].owner() == Team::none,
                    "negative partial progress remains neutral after neutralization");
    ownership_world.zones()[1].advance_capture(-40.0F);
    update_zone_capture(ownership_world, 0.0);
    passed &= check(
        ownership_world.zones()[1].owner() == Team::team_b &&
            ownership_world.zone_ownership_events().size() == 1 &&
            ownership_world.zone_ownership_events().front().previous_owner ==
                Team::none &&
            ownership_world.zone_ownership_events().front().new_owner ==
                Team::team_b &&
            ownership_world.zone_ownership_events().front().type ==
                ZoneTransitionType::captured,
        "enemy must continue to negative 100 before Team B captures");

    World direct_b_world;
    direct_b_world.units().clear();
    direct_b_world.zones()[2].advance_capture(-100.0F);
    update_zone_capture(direct_b_world, 0.0);
    passed &= check(direct_b_world.zones()[2].owner() == Team::team_b,
                    "neutral objective captures for Team B at negative 100");

    World crossing_world;
    crossing_world.units().clear();
    crossing_world.zones()[2].advance_capture(100.0F);
    update_zone_capture(crossing_world, 0.0);
    crossing_world.clear_transient_events();
    crossing_world.zones()[2].advance_capture(-200.0F);
    update_zone_capture(crossing_world, 0.0);
    passed &= check(
        crossing_world.zones()[2].owner() == Team::team_b &&
            crossing_world.zone_ownership_events().size() == 2 &&
            crossing_world.zone_ownership_events()[0].previous_owner ==
                Team::team_a &&
            crossing_world.zone_ownership_events()[0].new_owner == Team::none &&
            crossing_world.zone_ownership_events()[1].previous_owner ==
                Team::none &&
            crossing_world.zone_ownership_events()[1].new_owner == Team::team_b,
        "large meter changes still emit neutralization before enemy capture");

    home_world.zones()[0].set_owner(Team::team_b);
    home_world.zones()[4].set_owner(Team::team_a);
    passed &= check(home_world.zones()[0].owner() == Team::team_a &&
                        home_world.zones()[4].owner() == Team::team_b,
                    "home-zone ownership remains permanent");

    Unit machine_gun_rotation = unit_from_definition(
        99, Team::team_a, {}, 0.0F, machine_gun_definition);
    machine_gun_rotation.set_desired_facing_angle(90.0F);
    machine_gun_rotation.rotate_toward_desired(0.1);
    passed &= check(near(machine_gun_rotation.facing_angle(), 6.0F),
                    "machine_gun rotation uses its troop-specific speed");
    Unit bazooka_rotation = unit_from_definition(
        98, Team::team_a, {}, 0.0F, bazooka_definition);
    bazooka_rotation.set_desired_facing_angle(90.0F);
    bazooka_rotation.rotate_toward_desired(0.1);
    passed &= check(near(bazooka_rotation.facing_angle(), 5.0F),
                    "bazooka rotation uses its troop-specific speed");

    World mixed_world;
    std::array<int, 3> team_a_counts{};
    std::array<int, 3> team_b_counts{};
    for (const auto& unit : mixed_world.units()) {
        const std::size_t type_index = static_cast<std::size_t>(unit.troop_type());
        (unit.team() == Team::team_a ? team_a_counts : team_b_counts)[type_index]++;
    }
    passed &= check(team_a_counts == std::array<int, 3>{2, 2, 2} &&
                        team_b_counts == std::array<int, 3>{2, 2, 2},
                    "demo world contains two of every troop type per team");
    const Vec2 rifle_move_start = mixed_world.units()[0].position();
    const Vec2 machine_gun_move_start = mixed_world.units()[2].position();
    const Vec2 bazooka_move_start = mixed_world.units()[8].position();
    Simulation mixed_simulation{mixed_world};
    mixed_simulation.update(1.0 / 60.0);
    passed &= check(
        near(length(mixed_world.units()[0].position() - rifle_move_start),
             rifle_definition.move_speed / 60.0F) &&
            near(length(mixed_world.units()[2].position() - machine_gun_move_start),
                 machine_gun_definition.move_speed / 60.0F) &&
            near(length(mixed_world.units()[8].position() - bazooka_move_start),
                 bazooka_definition.move_speed / 60.0F),
        "all troops use their troop-specific movement speeds");

    const Unit support_screen = unit_from_definition(
        400, Team::team_a, {400.0F, 300.0F}, 270.0F, rifle_definition);
    const Unit supported_machine_gun = unit_from_definition(
        401, Team::team_a, {400.0F, 300.0F}, 270.0F,
        machine_gun_definition);
    const Unit supported_bazooka = unit_from_definition(
        402, Team::team_a, {400.0F, 300.0F}, 270.0F,
        bazooka_definition);
    const std::vector<Unit> support_formation{
        support_screen, supported_machine_gun, supported_bazooka};
    const SupportPositioning rifle_support =
        support_positioning_for(support_formation[0], support_formation);
    const SupportPositioning machine_gun_support =
        support_positioning_for(support_formation[1], support_formation);
    const SupportPositioning bazooka_support =
        support_positioning_for(support_formation[2], support_formation);
    passed &= check(!rifle_support.screen_id.has_value() &&
                        length_squared(rifle_support.steering) <= 0.0001F,
                    "rifle retains frontline behavior without support bias");
    passed &= check(machine_gun_support.screen_id == support_screen.id() &&
                        machine_gun_support.steering.x < 0.0F,
                    "machine_gun biases toward a safer position behind a rifle");
    passed &= check(bazooka_support.screen_id == support_screen.id() &&
                        bazooka_support.steering.x < 0.0F &&
                        length(bazooka_support.steering) >
                            length(machine_gun_support.steering),
                    "bazooka uses the stronger and deeper rear-positioning bias");

    World unsupported_advance_world;
    isolate_collision_units(unsupported_advance_world);
    unsupported_advance_world.units()[2].set_position({500.0F, 500.0F});
    const float unsupported_start_x =
        unsupported_advance_world.units()[2].position().x;
    Simulation unsupported_advance_simulation{unsupported_advance_world};
    unsupported_advance_simulation.update(1.0 / 60.0);
    passed &= check(
        unsupported_advance_world.units()[2].position().x >
            unsupported_start_x &&
            !unsupported_advance_world.units()[2].support_screen_id().has_value(),
        "support troop advances normally when no useful friendly screen exists");

    World supported_retreat_world;
    isolate_collision_units(supported_retreat_world);
    supported_retreat_world.units()[2].set_position({500.0F, 400.0F});
    supported_retreat_world.units()[0].set_position({520.0F, 400.0F});
    supported_retreat_world.units()[4].set_position({500.0F, 300.0F});
    const float supported_retreat_start_y =
        supported_retreat_world.units()[2].position().y;
    Simulation supported_retreat_simulation{supported_retreat_world};
    supported_retreat_simulation.update(1.0 / 60.0);
    passed &= check(
        supported_retreat_world.units()[2].combat_movement_state() ==
                CombatMovementState::retreating &&
            supported_retreat_world.units()[2].position().y >
                supported_retreat_start_y &&
            !supported_retreat_world.units()[2].support_screen_id().has_value(),
        "retreat overrides support positioning when an enemy rushes close");

    const Unit deterministic_support = unit_from_definition(
        410, Team::team_a, {300.0F, 300.0F}, 270.0F,
        machine_gun_definition);
    const Unit higher_id_screen = unit_from_definition(
        412, Team::team_a, {200.0F, 200.0F}, 270.0F,
        rifle_definition);
    const Unit lower_id_screen = unit_from_definition(
        411, Team::team_a, {400.0F, 400.0F}, 270.0F,
        rifle_definition);
    const std::vector<Unit> tied_screens{
        deterministic_support, higher_id_screen, lower_id_screen};
    for (int repeat = 0; repeat < 10; ++repeat) {
        passed &= check(
            support_positioning_for(tied_screens[0], tied_screens).screen_id ==
                lower_id_screen.id(),
            "equal-distance support screens resolve by lowest unit ID");
    }

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
    facing_world.units()[0].set_position({150.0F, 250.0F});
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
    passed &= check(no_fire_world.fire_events().empty(),
                    "no firing event is emitted without an actual shot");

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
    passed &= check(firing_world.fire_events().size() == 1 &&
                        firing_world.fire_events().front().unit_id ==
                            firing_world.units()[0].id(),
                    "actual projectile creation emits one firing event");
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
    passed &= check(firing_world.fire_events().empty(),
                    "cooldown-blocked firing emits no firing event");
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

    World machine_gun_firing_world;
    isolate_collision_units(machine_gun_firing_world);
    Unit& machine_gunner = machine_gun_firing_world.units()[2];
    Unit& machine_gun_target = machine_gun_firing_world.units()[4];
    machine_gunner.set_position({500.0F, 300.0F});
    machine_gun_target.set_position(
        machine_gunner.position() + direction_from_facing(20.0F) * 390.0F);
    const Unit::Id machine_gunner_id = machine_gunner.id();
    const Unit::Id machine_gun_target_id = machine_gun_target.id();
    Simulation machine_gun_firing_simulation{machine_gun_firing_world};
    machine_gun_firing_simulation.update(1.0 / 60.0);
    passed &= check(machine_gunner.target_id() == machine_gun_target_id &&
                        machine_gun_firing_world.projectiles().size() == 1 &&
                        machine_gun_firing_world.projectiles().front().weapon_type() ==
                            WeaponType::machine_gun &&
                        near(machine_gun_firing_world.projectiles().front().damage(),
                             machine_gun_definition.weapon.projectile_damage),
                    "machine_gun targeting and projectile creation use generic combat systems");
    passed &= check(machine_gun_firing_world.fire_events().size() == 1 &&
                        machine_gun_firing_world.fire_events().front().troop_type ==
                            TroopType::machine_gun &&
                        machine_gun_firing_world.fire_events().front().weapon_type ==
                            WeaponType::machine_gun,
                    "machine_gun actual shot emits troop-specific firing event");
    for (int tick = 0; tick < 10; ++tick) {
        machine_gun_firing_simulation.update(1.0 / 60.0);
        passed &= check(machine_gun_firing_world.fire_events().empty(),
                        "machine_gun cooldown prevents an early repeat shot");
    }
    machine_gun_firing_simulation.update(1.0 / 60.0);
    passed &= check(machine_gun_firing_world.fire_events().size() == 1 &&
                        machine_gun_firing_world.fire_events().front().unit_id ==
                            machine_gunner_id,
                    "machine_gun cadence uses its shorter fixed-step fire interval");

    World bazooka_firing_world;
    const Unit::Id bazooka_unit_id = bazooka_firing_world.units()[8].id();
    const Unit::Id bazooka_target_id = bazooka_firing_world.units()[10].id();
    std::erase_if(bazooka_firing_world.units(),
                  [bazooka_unit_id, bazooka_target_id](const Unit& unit) {
                      return unit.id() != bazooka_unit_id &&
                             unit.id() != bazooka_target_id;
                  });
    Unit& bazooka_unit = *bazooka_firing_world.find_unit(bazooka_unit_id);
    Unit& bazooka_target = *bazooka_firing_world.find_unit(bazooka_target_id);
    bazooka_unit.set_position({500.0F, 300.0F});
    bazooka_target.set_position(
        bazooka_unit.position() + direction_from_facing(35.0F) * 520.0F);
    Simulation bazooka_firing_simulation{bazooka_firing_world};
    bazooka_firing_simulation.update(1.0 / 60.0);
    passed &= check(bazooka_firing_world.projectiles().size() == 1 &&
                        bazooka_firing_world.projectiles().front().weapon_type() ==
                            WeaponType::bazooka &&
                        near(length(bazooka_firing_world.projectiles().front().velocity()),
                             bazooka_definition.weapon.projectile_speed) &&
                        near(bazooka_firing_world.projectiles().front().splash_radius(),
                             bazooka_definition.weapon.splash_radius),
                    "aligned bazooka creates a slower explosive projectile");
    passed &= check(bazooka_firing_world.fire_events().size() == 1 &&
                        bazooka_firing_world.fire_events().front().unit_id ==
                            bazooka_unit_id &&
                        bazooka_firing_world.fire_events().front().troop_type ==
                            TroopType::bazooka,
                    "actual bazooka shot emits one firing event");
    for (int tick = 0; tick < 155; ++tick) {
        bazooka_firing_simulation.update(1.0 / 60.0);
        const bool bazooka_fired = std::ranges::any_of(
            bazooka_firing_world.fire_events(),
            [bazooka_unit_id](const FireEvent& event) {
                return event.unit_id == bazooka_unit_id;
            });
        passed &= check(!bazooka_fired,
                        "bazooka reload prevents an early repeat shot");
    }
    bazooka_firing_simulation.update(1.0 / 60.0);
    passed &= check(std::ranges::any_of(
                        bazooka_firing_world.fire_events(),
                        [bazooka_unit_id](const FireEvent& event) {
                            return event.unit_id == bazooka_unit_id;
                        }),
                    "bazooka cadence uses its long fixed-step reload interval");

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

    World machine_gun_single_target_world;
    isolate_collision_units(machine_gun_single_target_world);
    machine_gun_single_target_world.units()[4].set_position({180.0F, 100.0F});
    machine_gun_single_target_world.units()[5].set_position({220.0F, 100.0F});
    machine_gun_single_target_world.spawn_projectile(
        WeaponType::machine_gun, Team::team_a,
        machine_gun_single_target_world.units()[2].id(), {100.0F, 100.0F},
        {12000.0F, 0.0F}, 1000.0F, 10.0F);
    Simulation machine_gun_single_target_simulation{
        machine_gun_single_target_world};
    machine_gun_single_target_simulation.update(1.0 / 60.0);
    passed &= check(
        near(machine_gun_single_target_world.units()[4].health(), 90.0F) &&
            near(machine_gun_single_target_world.units()[5].health(), 100.0F),
        "machine_gun projectile remains single-target");

    World splash_world;
    isolate_collision_units(splash_world);
    Unit& splash_source = splash_world.units()[8];
    Unit& splash_friendly = splash_world.units()[0];
    Unit& direct_enemy = splash_world.units()[4];
    Unit& clustered_enemy = splash_world.units()[5];
    Unit& outside_enemy = splash_world.units()[6];
    splash_source.set_position({170.0F, 100.0F});
    splash_friendly.set_position({190.0F, 130.0F});
    direct_enemy.set_position({200.0F, 100.0F});
    clustered_enemy.set_position({250.0F, 100.0F});
    outside_enemy.set_position({310.0F, 100.0F});
    splash_world.spawn_projectile(
        WeaponType::bazooka, Team::team_a, splash_source.id(),
        {100.0F, 100.0F}, {12000.0F, 0.0F}, 1000.0F, 70.0F, 115.0F);
    Simulation splash_simulation{splash_world};
    splash_simulation.update(1.0 / 60.0);
    passed &= check(splash_world.explosion_events().size() == 1 &&
                        splash_world.explosion_events().front().weapon_type ==
                            WeaponType::bazooka,
                    "direct bazooka collision emits one explosion");
    passed &= check(near(direct_enemy.health(), 30.0F) &&
                        near(clustered_enemy.health(), 30.0F),
                    "clustered hostile units each receive splash damage once");
    passed &= check(near(outside_enemy.health(), 100.0F),
                    "hostile unit outside splash radius takes no damage");
    passed &= check(near(splash_friendly.health(), 100.0F) &&
                        near(splash_source.health(), 80.0F),
                    "bazooka source and friendly units are immune to splash");
    passed &= check(splash_world.projectiles().empty(),
                    "bazooka projectile is removed after exploding");

    World machine_gun_lifecycle_world;
    isolate_collision_units(machine_gun_lifecycle_world);
    const Unit::Id machine_gun_source_id =
        machine_gun_lifecycle_world.units()[2].id();
    const Unit::Id defeated_machine_gun_id =
        machine_gun_lifecycle_world.units()[6].id();
    machine_gun_lifecycle_world.units()[6].set_position({200.0F, 100.0F});
    machine_gun_lifecycle_world.spawn_projectile(
        WeaponType::machine_gun, Team::team_a, machine_gun_source_id,
        {100.0F, 100.0F}, {12000.0F, 0.0F}, 1000.0F, 100.0F);
    Simulation machine_gun_lifecycle_simulation{machine_gun_lifecycle_world};
    machine_gun_lifecycle_simulation.update(1.0 / 60.0);
    passed &= check(
        machine_gun_lifecycle_world.find_unit(defeated_machine_gun_id) == nullptr &&
            machine_gun_lifecycle_world.death_events().size() == 1 &&
            machine_gun_lifecycle_world.death_events().front().unit_id ==
                defeated_machine_gun_id &&
            machine_gun_lifecycle_world.death_events().front().troop_type ==
                TroopType::machine_gun,
        "generic damage and death lifecycle remove a defeated machine_gun");

    World bazooka_lifecycle_world;
    isolate_collision_units(bazooka_lifecycle_world);
    Unit& bazooka_observer = bazooka_lifecycle_world.units()[0];
    Unit& defeated_bazooka = bazooka_lifecycle_world.units()[10];
    Unit& replacement_target = bazooka_lifecycle_world.units()[4];
    bazooka_observer.set_position({100.0F, 100.0F});
    defeated_bazooka.set_position({100.0F, 200.0F});
    replacement_target.set_position({100.0F, 300.0F});
    const Unit::Id bazooka_observer_id = bazooka_observer.id();
    const Unit::Id defeated_bazooka_id = defeated_bazooka.id();
    const Unit::Id replacement_target_id = replacement_target.id();
    bazooka_observer.set_target_id(defeated_bazooka_id);
    defeated_bazooka.apply_damage(defeated_bazooka.max_health());
    Simulation bazooka_lifecycle_simulation{bazooka_lifecycle_world};
    bazooka_lifecycle_simulation.update(1.0 / 60.0);
    const Unit* surviving_observer =
        bazooka_lifecycle_world.find_unit(bazooka_observer_id);
    passed &= check(
        bazooka_lifecycle_world.find_unit(defeated_bazooka_id) == nullptr &&
            bazooka_lifecycle_world.death_events().size() == 1 &&
            bazooka_lifecycle_world.death_events().front().troop_type ==
                TroopType::bazooka,
        "bazooka death uses the generic removal and death-event lifecycle");
    passed &= check(surviving_observer != nullptr &&
                        surviving_observer->target_id() == replacement_target_id,
                    "unit retargets after its bazooka target is removed");

    Unit defeated = test_unit(400, Team::team_b, {200.0F, 100.0F}, 90.0F);
    defeated.apply_damage(125.0F);
    passed &= check(near(defeated.health(), 0.0F),
                    "damage clamps health at zero");
    passed &= check(!defeated.is_alive(),
                    "zero-health unit becomes non-alive");

    World lifecycle_world;
    arrange_combat_scenario(lifecycle_world, {500.0F, 200.0F},
                            {500.0F, 300.0F});
    lifecycle_world.units()[5].set_position({500.0F, 450.0F});
    const Unit::Id observer_id = lifecycle_world.units()[0].id();
    const Unit::Id defeated_id = lifecycle_world.units()[4].id();
    const Unit::Id replacement_id = lifecycle_world.units()[5].id();
    lifecycle_world.units()[0].set_target_id(defeated_id);
    lifecycle_world.units()[4].set_target_id(observer_id);
    lifecycle_world.units()[4].apply_damage(
        lifecycle_world.units()[4].max_health());
    const Unit& pending_removal = lifecycle_world.units()[4];
    passed &= check(!pending_removal.is_alive() &&
                        pending_removal.movement_state() == MovementState::idle &&
                        pending_removal.combat_movement_state() ==
                            CombatMovementState::inactive &&
                        !pending_removal.target_id().has_value(),
                    "lethal damage immediately marks unit inactive before removal");
    passed &= check(!select_target(pending_removal, lifecycle_world.units()).has_value() &&
                        !can_fire_at(pending_removal, lifecycle_world.units()[0]),
                    "dead unit cannot target or fire before lifecycle removal");

    Simulation lifecycle_simulation{lifecycle_world};
    lifecycle_simulation.update(1.0 / 60.0);
    passed &= check(lifecycle_world.find_unit(defeated_id) == nullptr &&
                        lifecycle_world.units().size() == 11,
                    "dead gameplay unit is removed from active world units");
    passed &= check(lifecycle_world.death_events().size() == 1 &&
                        lifecycle_world.death_events().front().unit_id == defeated_id,
                    "removed unit emits exactly one death event");
    const Unit* lifecycle_observer = lifecycle_world.find_unit(observer_id);
    passed &= check(lifecycle_observer != nullptr &&
                        lifecycle_observer->target_id() == replacement_id,
                    "survivor clears removed target ID and reacquires replacement");

    lifecycle_simulation.update(1.0 / 60.0);
    passed &= check(lifecycle_world.death_events().empty(),
                    "removed unit cannot emit a duplicate death event");

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
    std::vector<Vec2> spawn_positions(world.units().size());
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
            passed &= check(x_delta > unit.move_speed() * 4.0F,
                            "team_a advances using its troop movement profile");
        } else {
            passed &= check(x_delta < -unit.move_speed() * 4.0F,
                            "team_b advances using its troop movement profile");
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
