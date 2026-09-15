#include "core/combat_behavior.hpp"
#include "client/cash_display.hpp"
#include "client/capture_bar.hpp"
#include "client/local_control.hpp"
#include "client/pointer_input.hpp"
#include "client/unit_selection.hpp"
#include "core/ai_commander.hpp"
#include "core/deployment.hpp"
#include "core/economy.hpp"
#include "core/frontline.hpp"
#include "core/map_definition.hpp"
#include "core/match.hpp"
#include "core/math.hpp"
#include "core/movement_path.hpp"
#include "core/perception.hpp"
#include "core/projectile_collision.hpp"
#include "core/scoring.hpp"
#include "core/simulation.hpp"
#include "core/support_positioning.hpp"
#include "core/tactical_command.hpp"
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

void secure_objective(siege::World& world, const std::size_t zone_index,
                      const siege::Team team) {
    siege::Zone& zone = world.zones()[zone_index];
    zone.advance_capture(team == siege::Team::team_a ? 100.0F : -100.0F);
    zone.set_owner(team);
    zone.update_security(2.0, 2.0);
}

} // namespace

int main() {
    using namespace siege;

    bool passed = true;

    const MapDefinition& battlefield = default_map_definition();
    const MapDefinition* looked_up_battlefield =
        map_definition("battlefield_01");
    passed &= check(
        looked_up_battlefield == &battlefield &&
            map_definition("missing_map") == nullptr &&
            battlefield.id == "battlefield_01",
        "battlefield_01 lookup is deterministic and rejects unknown IDs");
    passed &= check(near(battlefield.logical_width, 1'920.0F) &&
                        near(battlefield.logical_height, 1'080.0F) &&
                        battlefield.zones.size() == 5,
                    "battlefield_01 defines the exact logical dimensions and five zones");
    bool exact_zone_geometry = true;
    for (std::size_t index = 0; index < battlefield.zones.size(); ++index) {
        const ZoneDefinition& zone = battlefield.zones[index];
        exact_zone_geometry &= zone.id == index &&
                               near(zone.bounds.x,
                                    static_cast<float>(index) * 384.0F) &&
                               near(zone.bounds.y, 0.0F) &&
                               near(zone.bounds.width, 384.0F) &&
                               near(zone.bounds.height, 1'080.0F);
    }
    passed &= check(exact_zone_geometry,
                    "battlefield_01 zones are ordered with exact 384-unit bounds");
    passed &= check(
        battlefield.zones[0].type == ZoneType::home &&
            battlefield.zones[0].home_team == Team::team_a &&
            battlefield.zones[4].type == ZoneType::home &&
            battlefield.zones[4].home_team == Team::team_b &&
            battlefield.objective_zone_indices.size() == 3 &&
            battlefield.objective_zone_indices[0] == 1 &&
            battlefield.objective_zone_indices[1] == 2 &&
            battlefield.objective_zone_indices[2] == 3 &&
            battlefield.zones[1].type == ZoneType::objective &&
            battlefield.zones[2].type == ZoneType::objective &&
            battlefield.zones[3].type == ZoneType::objective &&
            battlefield.center_objective_zone_index == 2,
        "map classifies homes, objectives, and the center objective");
    passed &= check(
        battlefield.team_a_forward.home_zone_index == 0 &&
            battlefield.team_a_forward.opposing_home_zone_index == 4 &&
            battlefield.team_a_forward.x_direction == 1.0F &&
            battlefield.team_a_forward.objective_order[0] == 1 &&
            battlefield.team_a_forward.objective_order[2] == 3 &&
            battlefield.team_b_forward.home_zone_index == 4 &&
            battlefield.team_b_forward.opposing_home_zone_index == 0 &&
            battlefield.team_b_forward.x_direction == -1.0F &&
            battlefield.team_b_forward.objective_order[0] == 3 &&
            battlefield.team_b_forward.objective_order[2] == 1,
        "map defines mirrored Team A and Team B forward traversal");
    const Vec2 blue_forward = direction_from_facing(
        team_forward_facing_angle(battlefield, Team::team_a));
    const Vec2 red_forward = direction_from_facing(
        team_forward_facing_angle(battlefield, Team::team_b));
    passed &= check(near(blue_forward.x, 1.0F) &&
                        near(blue_forward.y, 0.0F) &&
                        near(red_forward.x, -1.0F) &&
                        near(red_forward.y, 0.0F),
                    "idle facing derives mirrored team-forward bearings from map traversal");
    passed &= check(
        map_zone_index_for_position(battlefield, {0.0F, 0.0F}) == 0 &&
            map_zone_index_for_position(battlefield, {383.999F, 500.0F}) == 0 &&
            map_zone_index_for_position(battlefield, {384.0F, 500.0F}) == 1 &&
            map_zone_index_for_position(battlefield, {768.0F, 500.0F}) == 2 &&
            map_zone_index_for_position(battlefield, {1'920.0F, 500.0F}) ==
                std::nullopt &&
            map_zone_index_for_position(battlefield, {500.0F, 1'080.0F}) ==
                std::nullopt,
        "map point lookup has deterministic lower-inclusive upper-exclusive boundaries");
    World map_world;
    bool world_uses_map_geometry = &map_world.map() == &battlefield &&
                                   map_world.zones().size() ==
                                       battlefield.zones.size();
    for (std::size_t index = 0; index < map_world.zones().size(); ++index) {
        const Bounds& actual = map_world.zones()[index].bounds();
        const Bounds& defined = battlefield.zones[index].bounds;
        world_uses_map_geometry &= near(actual.x, defined.x) &&
                                   near(actual.y, defined.y) &&
                                   near(actual.width, defined.width) &&
                                   near(actual.height, defined.height) &&
                                   map_world.zones()[index].type() ==
                                       battlefield.zones[index].type;
    }
    passed &= check(world_uses_map_geometry,
                    "World zone and render/deployment geometry derives from its map");

    std::array<bool, 5> terrain_types_seen{};
    bool terrain_bounds_valid = near(battlefield.terrain_tile_size, 64.0F) &&
                                !battlefield.terrain_regions.empty();
    for (const auto& region : battlefield.terrain_regions) {
        terrain_types_seen[static_cast<std::size_t>(region.terrain)] = true;
        terrain_bounds_valid &= !region.id.empty() && region.bounds.x >= 0.0F &&
                                region.bounds.y >= 0.0F &&
                                region.bounds.width > 0.0F &&
                                region.bounds.height > 0.0F &&
                                region.bounds.x + region.bounds.width <=
                                    battlefield.logical_width &&
                                region.bounds.y + region.bounds.height <=
                                    battlefield.logical_height;
    }
    passed &= check(
        terrain_bounds_valid &&
            std::ranges::all_of(terrain_types_seen, [](const bool seen) {
                return seen;
            }),
        "battlefield terrain deterministically covers all authored terrain types inside the map");

    bool environment_bounds_valid = !battlefield.environment_objects.empty();
    bool environment_ids_unique = true;
    for (std::size_t index = 0;
         index < battlefield.environment_objects.size(); ++index) {
        const auto& object = battlefield.environment_objects[index];
        environment_bounds_valid &=
            !object.id.empty() && object.position.x >= 0.0F &&
            object.position.y >= 0.0F && object.footprint.x >= 0.0F &&
            object.footprint.y >= 0.0F && object.footprint.width > 0.0F &&
            object.footprint.height > 0.0F &&
            object.footprint.x + object.footprint.width <=
                battlefield.logical_width &&
            object.footprint.y + object.footprint.height <=
                battlefield.logical_height;
        for (std::size_t other = index + 1;
             other < battlefield.environment_objects.size(); ++other) {
            environment_ids_unique &=
                object.id != battlefield.environment_objects[other].id;
        }
    }
    passed &= check(environment_bounds_valid && environment_ids_unique &&
                        battlefield.environment_objects.front().id ==
                            "blue_home_house" &&
                        battlefield.environment_objects.back().id ==
                            "east_bush",
                    "environment objects have deterministic unique IDs and in-map footprints");

    MapDefinition environment_free_map = battlefield;
    environment_free_map.environment_objects = {};
    World environment_world{default_match_rules, battlefield};
    World environment_free_world{default_match_rules, environment_free_map};
    environment_world.units().clear();
    environment_free_world.units().clear();
    const Vec2 environment_start{140.0F, 216.0F};
    environment_world.units().push_back(
        test_unit(90, Team::team_a, environment_start, 270.0F));
    environment_free_world.units().push_back(
        test_unit(90, Team::team_a, environment_start, 270.0F));
    Simulation environment_simulation{environment_world};
    Simulation environment_free_simulation{environment_free_world};
    environment_simulation.update(0.5);
    environment_free_simulation.update(0.5);
    passed &= check(
        environment_world.units()[0].position().x > environment_start.x &&
            near(environment_world.units()[0].position().x,
                 environment_free_world.units()[0].position().x) &&
            near(environment_world.units()[0].position().y,
                 environment_free_world.units()[0].position().y),
        "environment metadata is SDL-free visual data and does not block simulation movement");

    World selection_world;
    selection_world.units().clear();
    selection_world.units().push_back(
        test_unit(101, Team::team_a, {100.0F, 100.0F}, 0.0F));
    selection_world.units().push_back(
        test_unit(102, Team::team_a, {200.0F, 200.0F}, 0.0F));
    selection_world.units().push_back(
        test_unit(103, Team::team_a, {350.0F, 350.0F}, 0.0F));
    selection_world.units().push_back(
        test_unit(104, Team::team_b, {150.0F, 150.0F}, 180.0F));
    selection_world.units().push_back(
        test_unit(105, Team::team_a, {125.0F, 125.0F}, 0.0F));
    selection_world.units().back().apply_damage(100.0F);

    UnitSelection selection;
    selection.replace_from_rectangle(selection_world, Team::team_a,
                                     {200.0F, 200.0F},
                                     {50.0F, 50.0F});
    passed &= check(
        selection.ids() == std::vector<Unit::Id>{101, 102},
        "reversed selection includes living Team A units and inclusive edges only");
    passed &= check(!selection.contains(103) && !selection.contains(104) &&
                        !selection.contains(105),
                    "selection ignores outside, enemy, and dead units");

    LocalControlState local_control;
    passed &= check(local_control.team() == Team::team_a &&
                        team_color_name(local_control.team()) == "BLUE" &&
                        local_control.toggle() == Team::team_b &&
                        team_color_name(local_control.team()) == "RED" &&
                        local_control.toggle() == Team::team_a,
                    "local control defaults BLUE and toggles A to B to A");

    World cash_display_world;
    cash_display_world.find_player(Team::team_a)->reset_cash(18'450);
    cash_display_world.find_player(Team::team_b)->reset_cash(9'876'543);
    const auto blue_display_cash =
        controlled_team_cash(cash_display_world, local_control);
    (void)local_control.toggle();
    const auto red_display_cash =
        controlled_team_cash(cash_display_world, local_control);
    (void)local_control.toggle();
    cash_display_world.reset_for_sudden_death();
    const auto reset_blue_cash =
        controlled_team_cash(cash_display_world, local_control);
    (void)local_control.toggle();
    const auto reset_red_cash =
        controlled_team_cash(cash_display_world, local_control);
    (void)local_control.toggle();
    passed &= check(
        blue_display_cash == 18'450 && red_display_cash == 9'876'543 &&
            format_cash(*blue_display_cash) == "$18,450" &&
            format_cash(*red_display_cash) == "$9,876,543" &&
            reset_blue_cash == 25'000 && reset_red_cash == 25'000 &&
            format_cash(*reset_blue_cash) == "$25,000" &&
            format_cash(0) == "$0",
        "cash display selects the controlled player, formats separators, and reflects sudden-death cash reset");

    UnitSelection red_selection;
    red_selection.replace_from_rectangle(selection_world, Team::team_b,
                                         {50.0F, 50.0F},
                                         {200.0F, 200.0F});
    passed &= check(red_selection.ids() == std::vector<Unit::Id>{104},
                    "RED selection includes only living Team B units");

    UnitSelection rmb_selection;
    PointerInputRouter rmb_input;
    const PointerDispatch rmb_press =
        rmb_input.press(PointerButton::secondary, false);
    const PointerDispatch rmb_release =
        rmb_input.release(PointerButton::secondary);
    if (rmb_press == PointerDispatch::secondary &&
        rmb_release == PointerDispatch::secondary) {
        rmb_selection.replace_from_rectangle(selection_world, Team::team_a,
                                              {50.0F, 50.0F},
                                              {200.0F, 200.0F});
    }

    UnitSelection control_lmb_selection;
    PointerInputRouter control_lmb_input;
    const PointerDispatch control_lmb_press =
        control_lmb_input.press(PointerButton::primary, true);
    const PointerDispatch control_lmb_release =
        control_lmb_input.release(PointerButton::primary);
    if (control_lmb_press == PointerDispatch::secondary &&
        control_lmb_release == PointerDispatch::secondary) {
        control_lmb_selection.replace_from_rectangle(
            selection_world, Team::team_a, {200.0F, 200.0F},
            {50.0F, 50.0F});
    }
    passed &= check(
        rmb_selection.ids() == std::vector<Unit::Id>{101, 102} &&
            control_lmb_selection.ids() == rmb_selection.ids(),
        "RMB and reversed Control+LMB drags produce identical selection");

    PointerInputRouter normal_lmb_input;
    passed &= check(
        normal_lmb_input.press(PointerButton::primary, false) ==
                PointerDispatch::primary &&
            !normal_lmb_input.secondary_active() &&
            normal_lmb_input.release(PointerButton::primary) ==
                PointerDispatch::primary,
        "normal LMB remains a primary action and cannot trigger selection");

    PointerInputRouter cancelled_input;
    UnitSelection cleared_on_switch;
    cleared_on_switch.replace_from_rectangle(
        selection_world, Team::team_a, {50.0F, 50.0F}, {200.0F, 200.0F});
    (void)cancelled_input.press(PointerButton::secondary, false);
    (void)local_control.toggle();
    cleared_on_switch.clear();
    cancelled_input.cancel();
    passed &= check(cleared_on_switch.ids().empty() &&
                        !cancelled_input.secondary_active(),
                    "team switching clears selection and active pointer input");
    int normal_lmb_action_count = 0;
    PointerInputRouter exclusive_control_lmb;
    if (exclusive_control_lmb.press(PointerButton::primary, true) ==
        PointerDispatch::primary) {
        ++normal_lmb_action_count;
    }
    passed &= check(
        normal_lmb_action_count == 0 &&
            exclusive_control_lmb.secondary_active() &&
            exclusive_control_lmb.release(PointerButton::primary) ==
                PointerDispatch::secondary &&
            !exclusive_control_lmb.secondary_active(),
        "Control+LMB is exclusively secondary for its complete lifecycle");
    passed &= check(
        classify_secondary_gesture({100.0F, 100.0F},
                                   {104.0F, 104.0F}) ==
                SecondaryGesture::click &&
            classify_secondary_gesture({100.0F, 100.0F},
                                       {120.0F, 100.0F}) ==
                SecondaryGesture::drag,
        "secondary click and drag use one deterministic movement threshold");
    passed &= check(
        rmb_press == control_lmb_press && rmb_release == control_lmb_release &&
            classify_secondary_gesture({20.0F, 20.0F}, {20.0F, 20.0F}) ==
                SecondaryGesture::click,
        "RMB and Control+LMB reach the identical secondary-click menu path");

    const auto picked_team_a =
        pick_path_unit(selection_world, Team::team_a, {100.0F, 100.0F});
    passed &= check(
        picked_team_a == 101 &&
            !pick_path_unit(selection_world, Team::team_a,
                            {150.0F, 150.0F}, 20.0F)
                 .has_value() &&
            !pick_path_unit(selection_world, Team::team_b,
                            {100.0F, 100.0F}, 20.0F)
                 .has_value(),
        "path picking accepts one living Team A unit and ignores enemy, dead, missing, and empty hits");
    passed &= check(
        should_begin_individual_path(PointerDispatch::primary, false, true) &&
            !should_begin_individual_path(PointerDispatch::secondary, false,
                                          true) &&
            !should_begin_individual_path(PointerDispatch::primary, true,
                                          true) &&
            !should_begin_individual_path(PointerDispatch::primary, false,
                                          false),
        "only plain primary input on a valid unit starts a path and deployment takes precedence");

    std::vector<Vec2> sampled_path;
    passed &= check(
        !append_path_sample(sampled_path, {0.0F, 0.0F}, {10.0F, 0.0F},
                            false) &&
            append_path_sample(sampled_path, {0.0F, 0.0F}, {18.0F, 0.0F},
                               false) &&
            !append_path_sample(sampled_path, {0.0F, 0.0F}, {30.0F, 0.0F},
                                false) &&
            append_path_sample(sampled_path, {0.0F, 0.0F}, {40.0F, 10.0F},
                               false) &&
            append_path_sample(sampled_path, {0.0F, 0.0F}, {45.0F, 12.0F},
                               true) &&
            sampled_path.size() == 3 && near(sampled_path[0].x, 18.0F) &&
            near(sampled_path[0].y, 0.0F) &&
            near(sampled_path[1].x, 40.0F) &&
            near(sampled_path[1].y, 10.0F) &&
            near(sampled_path[2].x, 45.0F) &&
            near(sampled_path[2].y, 12.0F),
        "waypoint sampling is distance-limited, ordered, endpoint-preserving, and deterministic");

    selection.replace_from_rectangle(selection_world, Team::team_a,
                                     {300.0F, 300.0F},
                                     {400.0F, 400.0F});
    passed &= check(selection.ids() == std::vector<Unit::Id>{103},
                    "a new rectangle replaces the previous selection");
    selection_world.units()[2].apply_damage(100.0F);
    selection.prune(selection_world, Team::team_a);
    passed &= check(selection.ids().empty(),
                    "dead selected unit IDs are pruned safely");

    selection.replace_from_rectangle(selection_world, Team::team_a,
                                     {90.0F, 90.0F},
                                     {110.0F, 110.0F});
    selection_world.units().erase(selection_world.units().begin());
    selection.prune(selection_world, Team::team_a);
    passed &= check(selection.ids().empty(),
                    "missing selected unit IDs are pruned safely");

    World command_filter_world;
    command_filter_world.units().clear();
    command_filter_world.units().push_back(
        test_unit(201, Team::team_a, {200.0F, 200.0F}, 270.0F));
    command_filter_world.units().push_back(
        test_unit(202, Team::team_a, {250.0F, 200.0F}, 270.0F));
    command_filter_world.units().push_back(
        test_unit(203, Team::team_b, {300.0F, 200.0F}, 90.0F));
    command_filter_world.units()[1].apply_damage(100.0F);
    const std::array<Unit::Id, 5> mixed_command_ids{201, 202, 203, 999, 201};
    passed &= check(
        apply_tactical_order(command_filter_world, mixed_command_ids,
                             TacticalOrder::hold) == 1 &&
            command_filter_world.units()[0].tactical_order() ==
                TacticalOrder::hold &&
            command_filter_world.units()[0].tactical_position().has_value() &&
            near(command_filter_world.units()[0].tactical_position()->x,
                 200.0F) &&
            command_filter_world.units()[1].tactical_order() ==
                TacticalOrder::automatic &&
            command_filter_world.units()[2].tactical_order() ==
                TacticalOrder::automatic,
        "commands affect selected living friendlies and safely ignore stale, dead, enemy, and duplicate IDs");

    const std::array<Unit::Id, 2> red_command_ids{201, 203};
    passed &= check(
        apply_tactical_order(command_filter_world, red_command_ids,
                             TacticalOrder::advance, Team::team_b) == 1 &&
            command_filter_world.units()[0].tactical_order() ==
                TacticalOrder::hold &&
            command_filter_world.units()[2].tactical_order() ==
                TacticalOrder::advance,
        "RED tactical commands apply only to living Team B units");

    passed &= check(
        assign_movement_path(command_filter_world, 203,
                             {{280.0F, 220.0F}, {240.0F, 240.0F}},
                             Team::team_b) &&
            command_filter_world.units()[2].has_movement_path() &&
            !assign_movement_path(command_filter_world, 201,
                                  {{180.0F, 220.0F}}, Team::team_b),
        "RED individual paths apply only to living Team B units");

    World red_deployment_world;
    red_deployment_world.units().clear();
    const Money red_cash_before =
        red_deployment_world.find_player(Team::team_b)->cash();
    passed &= check(
        request_deployment(red_deployment_world, Team::team_b,
                           TroopType::rifle, {1800.0F, 500.0F}) ==
                DeploymentResult::accepted &&
            red_deployment_world.find_player(Team::team_b)->cash() ==
                red_cash_before - rifle_definition.purchase_cost &&
            red_deployment_world.pending_deployments().size() == 1 &&
            red_deployment_world.pending_deployments().front().team ==
                Team::team_b,
        "RED deployment charges Team B and creates a Team B pending unit");

    red_deployment_world.units().push_back(
        test_unit(204, Team::team_a, {300.0F, 300.0F}, 270.0F));
    const Team existing_blue_team =
        red_deployment_world.units().front().team();
    const Team home_a_owner = red_deployment_world.zones()[0].owner();
    const Team home_b_owner = red_deployment_world.zones()[4].owner();
    const Score blue_score =
        red_deployment_world.find_player(Team::team_a)->score();
    const Score red_score =
        red_deployment_world.find_player(Team::team_b)->score();
    LocalControlState isolated_control;
    (void)isolated_control.toggle();
    passed &= check(
        existing_blue_team == Team::team_a &&
            red_deployment_world.units().front().team() == existing_blue_team &&
            red_deployment_world.zones()[0].owner() == home_a_owner &&
            red_deployment_world.zones()[4].owner() == home_b_owner &&
            red_deployment_world.find_player(Team::team_a)->score() ==
                blue_score &&
            red_deployment_world.find_player(Team::team_b)->score() ==
                red_score,
        "switching local control does not alter units, ownership, or scores");

    World ai_purchase_world;
    ai_purchase_world.units().clear();
    AiCommander red_commander{Team::team_b};
    const Money ai_starting_cash =
        ai_purchase_world.find_player(Team::team_b)->cash();
    red_commander.update(ai_purchase_world, Team::team_a);
    const auto first_ai_position = red_commander.last_deployment_position();
    passed &= check(
        red_commander.status() == AiCommanderStatus::enabled &&
            red_commander.last_result() == AiDecisionResult::purchased &&
            red_commander.last_troop_choice() == TroopType::rifle &&
            first_ai_position.has_value() &&
            is_valid_deployment_location(ai_purchase_world, Team::team_b,
                                         *first_ai_position) &&
            ai_purchase_world.find_player(Team::team_b)->cash() ==
                ai_starting_cash - rifle_definition.purchase_cost &&
            ai_purchase_world.pending_deployments().size() == 1 &&
            ai_purchase_world.pending_deployments().front().team ==
                Team::team_b &&
            ai_purchase_world.pending_deployments().front().troop_type ==
                TroopType::rifle &&
            near(static_cast<float>(
                     ai_purchase_world.pending_deployments().front()
                         .remaining_seconds),
                 static_cast<float>(rifle_definition.deployment_seconds)),
        "RED AI uses normal deployment validation, cost, and pending timer");

    update_pending_deployments(ai_purchase_world, 0.74);
    passed &= check(ai_purchase_world.units().empty() &&
                        ai_purchase_world.pending_deployments().size() == 1,
                    "AI deployment does not bypass the normal pending timer");
    update_pending_deployments(ai_purchase_world, 0.01);
    passed &= check(
        ai_purchase_world.pending_deployments().empty() &&
            ai_purchase_world.units().size() == 1 &&
            ai_purchase_world.units().front().team() == Team::team_b &&
            ai_purchase_world.units().front().troop_type() == TroopType::rifle,
        "AI pending deployment spawns the purchased RED troop normally");

    World unaffordable_ai_world;
    unaffordable_ai_world.units().clear();
    unaffordable_ai_world.find_player(Team::team_b)->reset_cash(
        rifle_definition.purchase_cost - 1);
    AiCommander saving_commander{Team::team_b};
    saving_commander.update(unaffordable_ai_world, Team::team_a);
    passed &= check(
        unaffordable_ai_world.pending_deployments().empty() &&
            unaffordable_ai_world.find_player(Team::team_b)->cash() ==
                rifle_definition.purchase_cost - 1 &&
            saving_commander.last_result() ==
                AiDecisionResult::no_affordable_troop,
        "AI cannot overspend or create a free troop when none is affordable");

    World ai_mix_world;
    ai_mix_world.units().clear();
    AiCommander mix_commander{Team::team_b};
    mix_commander.update(ai_mix_world, Team::team_a, 361);
    const std::array<TroopType, 4> expected_ai_mix{
        TroopType::rifle, TroopType::machine_gun, TroopType::rifle,
        TroopType::bazooka};
    bool mixed_pending = ai_mix_world.pending_deployments().size() ==
                         expected_ai_mix.size();
    for (std::size_t index = 0;
         mixed_pending && index < expected_ai_mix.size(); ++index) {
        mixed_pending &= ai_mix_world.pending_deployments()[index].troop_type ==
                         expected_ai_mix[index];
    }
    const bool mixed_y_positions =
        mixed_pending &&
        !near(ai_mix_world.pending_deployments()[0].position.y,
              ai_mix_world.pending_deployments()[1].position.y) &&
        !near(ai_mix_world.pending_deployments()[1].position.y,
              ai_mix_world.pending_deployments()[2].position.y);
    passed &= check(
        mixed_pending && mixed_y_positions &&
            mix_commander.successful_deployments() == 4 &&
            ai_mix_world.find_player(Team::team_b)->cash() ==
                default_economy_rules.starting_cash -
                    rifle_definition.purchase_cost * 2 -
                    machine_gun_definition.purchase_cost -
                    bazooka_definition.purchase_cost,
        "AI follows the centralized deterministic weighted troop mix");

    World ai_front_world;
    ai_front_world.units().clear();
    secure_objective(ai_front_world, 3, Team::team_b);
    secure_objective(ai_front_world, 2, Team::team_b);
    AiCommander front_commander{Team::team_b};
    front_commander.update(ai_front_world, Team::team_a);
    const auto front_position = front_commander.last_deployment_position();
    passed &= check(
        front_position.has_value() &&
            zone_index_for_position(ai_front_world, *front_position) == 2 &&
            is_valid_deployment_location(ai_front_world, Team::team_b,
                                         *front_position),
        "RED AI prefers the map-defined frontmost contiguous deployment zone");

    World ai_disconnected_world;
    ai_disconnected_world.units().clear();
    secure_objective(ai_disconnected_world, 2, Team::team_b);
    AiCommander disconnected_commander{Team::team_b};
    disconnected_commander.update(ai_disconnected_world, Team::team_a);
    const auto disconnected_position =
        disconnected_commander.last_deployment_position();
    passed &= check(
        disconnected_position.has_value() &&
            zone_index_for_position(ai_disconnected_world,
                                    *disconnected_position) == 4,
        "AI cannot bypass a disconnected secured objective deployment gap");

    World ai_sudden_world{MatchRules{60, 0}};
    ai_sudden_world.units().clear();
    secure_objective(ai_sudden_world, 3, Team::team_b);
    AiCommander sudden_commander{Team::team_b};
    sudden_commander.update(ai_sudden_world, Team::team_a);
    const auto sudden_position = sudden_commander.last_deployment_position();
    passed &= check(
        ai_sudden_world.match_state().phase() == MatchPhase::sudden_death &&
            sudden_position.has_value() &&
            zone_index_for_position(ai_sudden_world, *sudden_position) == 4 &&
            is_valid_deployment_location(ai_sudden_world, Team::team_b,
                                         *sudden_position),
        "AI respects sudden-death home-only deployment");

    World ai_finished_world;
    ai_finished_world.units().clear();
    (void)ai_finished_world.match_state().advance(
        default_match_rules.duration_ticks(), 1, 0);
    AiCommander finished_commander{Team::team_b};
    const Money finished_cash =
        ai_finished_world.find_player(Team::team_b)->cash();
    finished_commander.update(ai_finished_world, Team::team_a, 600);
    passed &= check(
        finished_commander.status() ==
                AiCommanderStatus::stopped_match_finished &&
            ai_finished_world.pending_deployments().empty() &&
            ai_finished_world.find_player(Team::team_b)->cash() ==
                finished_cash,
        "finished matches stop AI purchasing and deployment actions");

    World ai_control_world;
    ai_control_world.units().clear();
    LocalControlState ai_local_control;
    AiCommander control_commander{Team::team_b};
    control_commander.update(ai_control_world, ai_local_control.team());
    const std::size_t before_pause =
        ai_control_world.pending_deployments().size();
    const std::uint64_t before_pause_ticks =
        control_commander.ticks_until_next_decision();
    const std::uint64_t before_pause_strategy_ticks =
        control_commander.ticks_until_next_strategy_evaluation();
    const std::uint64_t before_pause_strategy_evaluations =
        control_commander.strategy_evaluation_count();
    (void)ai_local_control.toggle();
    control_commander.update(ai_control_world, ai_local_control.team(), 600);
    const bool paused_cleanly =
        control_commander.status() ==
            AiCommanderStatus::paused_local_control &&
        ai_control_world.pending_deployments().size() == before_pause &&
        control_commander.ticks_until_next_decision() == before_pause_ticks &&
        control_commander.ticks_until_next_strategy_evaluation() ==
            before_pause_strategy_ticks &&
        control_commander.strategy_evaluation_count() ==
            before_pause_strategy_evaluations;
    (void)ai_local_control.toggle();
    control_commander.update(ai_control_world, ai_local_control.team(),
                             before_pause_ticks);
    passed &= check(
        paused_cleanly &&
            control_commander.status() == AiCommanderStatus::enabled &&
            ai_control_world.pending_deployments().size() == before_pause + 1 &&
            control_commander.strategy_evaluation_count() >
                before_pause_strategy_evaluations,
        "F4 RED control pauses purchasing and strategy clocks, then BLUE control resumes both");

    World deterministic_ai_world_a;
    World deterministic_ai_world_b;
    deterministic_ai_world_a.units().clear();
    deterministic_ai_world_b.units().clear();
    AiCommander deterministic_commander_a{Team::team_b};
    AiCommander deterministic_commander_b{Team::team_b};
    Simulation deterministic_ai_simulation_a{deterministic_ai_world_a};
    Simulation deterministic_ai_simulation_b{deterministic_ai_world_b};
    for (int tick = 0; tick < 480; ++tick) {
        deterministic_ai_simulation_a.update(1.0 / 60.0);
        deterministic_commander_a.update(deterministic_ai_world_a,
                                         Team::team_a);
        deterministic_ai_simulation_b.update(1.0 / 60.0);
        deterministic_commander_b.update(deterministic_ai_world_b,
                                         Team::team_a);
    }
    bool deterministic_ai =
        deterministic_ai_world_a.find_player(Team::team_b)->cash() ==
            deterministic_ai_world_b.find_player(Team::team_b)->cash() &&
        deterministic_ai_world_a.units().size() ==
            deterministic_ai_world_b.units().size() &&
        deterministic_ai_world_a.pending_deployments().size() ==
            deterministic_ai_world_b.pending_deployments().size() &&
        deterministic_commander_a.successful_deployments() ==
            deterministic_commander_b.successful_deployments() &&
        deterministic_commander_a.strategy() ==
            deterministic_commander_b.strategy() &&
        deterministic_commander_a.target_objective() ==
            deterministic_commander_b.target_objective() &&
        deterministic_commander_a.last_tactical_command() ==
            deterministic_commander_b.last_tactical_command() &&
        std::ranges::equal(
            deterministic_commander_a.last_commanded_unit_ids(),
            deterministic_commander_b.last_commanded_unit_ids()) &&
        deterministic_commander_a.tactical_command_issue_count() ==
            deterministic_commander_b.tactical_command_issue_count();
    for (std::size_t index = 0;
         deterministic_ai && index < deterministic_ai_world_a.units().size();
         ++index) {
        const Unit& left = deterministic_ai_world_a.units()[index];
        const Unit& right = deterministic_ai_world_b.units()[index];
        deterministic_ai &= left.id() == right.id() &&
                            left.team() == right.team() &&
                            left.troop_type() == right.troop_type() &&
                            left.tactical_order() == right.tactical_order() &&
                            near(left.position().x, right.position().x) &&
                            near(left.position().y, right.position().y);
    }
    for (std::size_t index = 0;
         deterministic_ai &&
         index < deterministic_ai_world_a.pending_deployments().size();
         ++index) {
        const PendingDeployment& left =
            deterministic_ai_world_a.pending_deployments()[index];
        const PendingDeployment& right =
            deterministic_ai_world_b.pending_deployments()[index];
        deterministic_ai &= left.id == right.id && left.team == right.team &&
                            left.troop_type == right.troop_type &&
                            near(left.position.x, right.position.x) &&
                            near(left.position.y, right.position.y) &&
                            std::abs(left.remaining_seconds -
                                     right.remaining_seconds) < 1.0e-9;
    }
    passed &= check(deterministic_ai,
                    "repeated fixed-timestep simulations produce identical AI decisions");

    World ai_attack_world;
    ai_attack_world.units().clear();
    ai_attack_world.find_player(Team::team_b)->reset_cash(0);
    ai_attack_world.units().push_back(
        test_unit(520, Team::team_b, {1'440.0F, 300.0F}, 90.0F));
    ai_attack_world.units().push_back(
        unit_from_definition(510, Team::team_b, {1'620.0F, 700.0F}, 90.0F,
                             machine_gun_definition));
    ai_attack_world.units().push_back(
        test_unit(530, Team::team_b, {1'820.0F, 500.0F}, 90.0F));
    ai_attack_world.units().push_back(
        test_unit(540, Team::team_a, {300.0F, 500.0F}, 270.0F));
    ai_attack_world.units().push_back(
        test_unit(550, Team::team_b, {1'430.0F, 500.0F}, 90.0F));
    ai_attack_world.units().back().apply_damage(
        ai_attack_world.units().back().max_health());
    update_zone_capture(ai_attack_world, 0.0);
    AiCommander attack_commander{Team::team_b};
    attack_commander.update(ai_attack_world, Team::team_a);
    const std::array<Unit::Id, 2> expected_local_attack_ids{510, 520};
    passed &= check(
        attack_commander.strategy() == AiStrategy::attack &&
            attack_commander.target_objective() == 3 &&
            attack_commander.last_tactical_command() ==
                TacticalOrder::advance &&
            std::ranges::equal(attack_commander.last_commanded_unit_ids(),
                               expected_local_attack_ids) &&
            ai_attack_world.find_unit(510)->tactical_order() ==
                TacticalOrder::advance &&
            ai_attack_world.find_unit(520)->tactical_order() ==
                TacticalOrder::advance &&
            ai_attack_world.find_unit(530)->tactical_order() ==
                TacticalOrder::automatic &&
            ai_attack_world.find_unit(540)->tactical_order() ==
                TacticalOrder::automatic &&
            ai_attack_world.find_unit(550)->tactical_order() ==
                TacticalOrder::automatic,
        "unthreatened RED frontline deterministically advances only its local living force");

    const std::uint64_t attack_issue_count =
        attack_commander.tactical_command_issue_count();
    attack_commander.update(ai_attack_world, Team::team_a, 59);
    passed &= check(
        attack_commander.strategy_evaluation_count() == 2 &&
            attack_commander.tactical_command_issue_count() ==
                attack_issue_count,
        "repeated strategy evaluation does not reissue an identical order");

    World ai_defend_world;
    ai_defend_world.units().clear();
    ai_defend_world.find_player(Team::team_b)->reset_cash(0);
    secure_objective(ai_defend_world, 3, Team::team_b);
    ai_defend_world.units().push_back(
        test_unit(610, Team::team_b, {1'620.0F, 260.0F}, 90.0F));
    ai_defend_world.units().push_back(
        unit_from_definition(620, Team::team_b, {1'420.0F, 760.0F}, 90.0F,
                             bazooka_definition));
    ai_defend_world.units().push_back(
        test_unit(630, Team::team_a, {1'360.0F, 260.0F}, 270.0F));
    ai_defend_world.units().push_back(
        test_unit(640, Team::team_a, {300.0F, 760.0F}, 270.0F));
    update_zone_capture(ai_defend_world, 0.0);
    AiCommander defend_commander{Team::team_b};
    defend_commander.update(ai_defend_world, Team::team_a);
    const Bounds defended_bounds = ai_defend_world.zones()[3].bounds();
    const Unit* outside_defender = ai_defend_world.find_unit(610);
    const Unit* inside_defender = ai_defend_world.find_unit(620);
    const auto anchor_inside = [&defended_bounds](const Unit* unit) {
        return unit != nullptr && unit->tactical_position().has_value() &&
               unit->tactical_position()->x >= defended_bounds.x &&
               unit->tactical_position()->x <
                   defended_bounds.x + defended_bounds.width &&
               unit->tactical_position()->y >= defended_bounds.y &&
               unit->tactical_position()->y <
                   defended_bounds.y + defended_bounds.height;
    };
    passed &= check(
        defend_commander.strategy() == AiStrategy::defend &&
            defend_commander.target_objective() == 3 &&
            defend_commander.relevant_enemy_strength() == 1 &&
            outside_defender->tactical_order() == TacticalOrder::hold &&
            inside_defender->tactical_order() == TacticalOrder::hold &&
            anchor_inside(outside_defender) && anchor_inside(inside_defender) &&
            ai_defend_world.find_unit(630)->tactical_order() ==
                TacticalOrder::automatic &&
            ai_defend_world.find_unit(640)->tactical_order() ==
                TacticalOrder::automatic,
        "an actively threatened owned objective takes priority and holds RED anchors inside its bounds");

    World ai_regroup_world;
    ai_regroup_world.units().clear();
    ai_regroup_world.find_player(Team::team_b)->reset_cash(0);
    ai_regroup_world.units().push_back(
        test_unit(710, Team::team_b, {950.0F, 300.0F}, 90.0F));
    ai_regroup_world.units().push_back(
        test_unit(720, Team::team_b, {1'500.0F, 700.0F}, 90.0F));
    ai_regroup_world.units().push_back(
        test_unit(730, Team::team_a, {1'180.0F, 200.0F}, 270.0F));
    ai_regroup_world.units().push_back(
        test_unit(740, Team::team_a, {1'280.0F, 400.0F}, 270.0F));
    ai_regroup_world.units().push_back(
        test_unit(750, Team::team_a, {1'380.0F, 600.0F}, 270.0F));
    ai_regroup_world.units().push_back(
        test_unit(760, Team::team_a, {1'480.0F, 800.0F}, 270.0F));
    update_zone_capture(ai_regroup_world, 0.0);
    AiCommander regroup_commander{Team::team_b};
    regroup_commander.update(ai_regroup_world, Team::team_a);
    const auto regroup_target =
        ai_regroup_world.find_unit(710)->tactical_position();
    const auto second_regroup_target =
        ai_regroup_world.find_unit(720)->tactical_position();
    passed &= check(
        regroup_commander.strategy() == AiStrategy::regroup &&
            regroup_commander.relevant_friendly_strength() == 2 &&
            regroup_commander.relevant_enemy_strength() == 4 &&
            regroup_commander.last_tactical_command() ==
                TacticalOrder::regroup &&
            regroup_target.has_value() && second_regroup_target.has_value() &&
            near(second_regroup_target->x, regroup_target->x) &&
            near(second_regroup_target->y, regroup_target->y) &&
            near(regroup_target->x, 1'225.0F) &&
            near(regroup_target->y, 500.0F),
        "scattered and substantially outnumbered RED forces regroup around one deterministic center");
    const std::uint64_t regroup_issue_count =
        regroup_commander.tactical_command_issue_count();
    regroup_commander.update(ai_regroup_world, Team::team_a, 59);
    passed &= check(
        regroup_commander.tactical_command_issue_count() ==
            regroup_issue_count,
        "active regroup orders continue without strategy-tick command spam");

    std::erase_if(ai_regroup_world.units(), [](const Unit& unit) {
        return unit.team() == Team::team_a;
    });
    Simulation regroup_completion_simulation{ai_regroup_world};
    for (int tick = 0; tick < 900; ++tick) {
        regroup_completion_simulation.update(1.0 / 60.0);
    }
    passed &= check(
        ai_regroup_world.find_unit(710)->tactical_order() ==
                TacticalOrder::automatic &&
            ai_regroup_world.find_unit(720)->tactical_order() ==
                TacticalOrder::automatic,
        "AI-issued Regroup uses normal consolidation and automatic completion");
    regroup_commander.update(ai_regroup_world, Team::team_a, 60);
    passed &= check(
        regroup_commander.strategy() == AiStrategy::attack &&
            ai_regroup_world.find_unit(710)->tactical_order() ==
                TacticalOrder::advance &&
            ai_regroup_world.find_unit(720)->tactical_order() ==
                TacticalOrder::advance,
        "a consolidated AI regroup resumes the normal frontline attack");

    World ai_frontline_limit_world;
    ai_frontline_limit_world.units().clear();
    ai_frontline_limit_world.find_player(Team::team_b)->reset_cash(0);
    ai_frontline_limit_world.units().push_back(
        test_unit(810, Team::team_b, {1'300.0F, 500.0F}, 90.0F));
    update_zone_capture(ai_frontline_limit_world, 0.0);
    AiCommander frontline_limit_commander{Team::team_b};
    frontline_limit_commander.update(ai_frontline_limit_world, Team::team_a);
    Simulation frontline_limit_simulation{ai_frontline_limit_world};
    for (int tick = 0; tick < 240; ++tick) {
        frontline_limit_simulation.update(1.0 / 60.0);
    }
    passed &= check(
        ai_frontline_limit_world.find_unit(810)->position().x >= 1'151.99F &&
            ai_frontline_limit_world.zones()[3].owner() == Team::none,
        "AI Advance remains constrained by the uncaptured RED frontline");

    passed &= check(
        sudden_commander.strategy() == AiStrategy::attack &&
            sudden_commander.target_objective() ==
                battlefield.center_objective_zone_index,
        "sudden-death strategy explicitly prioritizes center progression");

    World economy_world;
    economy_world.units().clear();
    economy_world.projectiles().clear();
    const PlayerState* initial_team_a = economy_world.find_player(Team::team_a);
    const PlayerState* initial_team_b = economy_world.find_player(Team::team_b);
    passed &= check(initial_team_a != nullptr && initial_team_b != nullptr &&
                        initial_team_a->cash() == 25'000 &&
                        initial_team_b->cash() == 25'000,
                    "both players start with 25000 cash");
    passed &= check(
        default_economy_rules.passive_income_per_second == 200 &&
            kill_reward_for(TroopType::rifle) == 250 &&
            kill_reward_for(TroopType::machine_gun) == 400 &&
            kill_reward_for(TroopType::bazooka) == 600 &&
            default_economy_rules.objective_capture_reward == 1'000,
        "passive income and all troop/capture rewards are centralized");

    Simulation economy_simulation{economy_world};
    for (int tick = 0; tick < 60; ++tick) {
        economy_simulation.update(1.0 / 60.0);
    }
    passed &= check(economy_world.find_player(Team::team_a)->cash() == 25'200 &&
                        economy_world.find_player(Team::team_b)->cash() == 25'200,
                    "one fixed-step second awards 200 passive cash equally");

    World batched_economy_world;
    batched_economy_world.units().clear();
    update_passive_income(batched_economy_world, 60);
    passed &= check(
        batched_economy_world.find_player(Team::team_a)->cash() ==
                economy_world.find_player(Team::team_a)->cash() &&
            batched_economy_world.find_player(Team::team_b)->cash() ==
                economy_world.find_player(Team::team_b)->cash(),
        "batched and individual fixed ticks produce deterministic income");

    World render_rate_a;
    World render_rate_b;
    render_rate_a.units().clear();
    render_rate_b.units().clear();
    Simulation render_rate_a_simulation{render_rate_a};
    Simulation render_rate_b_simulation{render_rate_b};
    Money render_snapshot_checksum = 0;
    for (int tick = 0; tick < 120; ++tick) {
        render_rate_a_simulation.update(1.0 / 60.0);
        for (int render = 0; render <= tick % 7; ++render) {
            render_snapshot_checksum +=
                render_rate_b.find_player(Team::team_a)->cash();
        }
        render_rate_b_simulation.update(1.0 / 60.0);
    }
    passed &= check(
        render_snapshot_checksum > 0 &&
            render_rate_a.find_player(Team::team_a)->cash() == 25'400 &&
            render_rate_a.find_player(Team::team_a)->cash() ==
                render_rate_b.find_player(Team::team_a)->cash() &&
            render_rate_a.find_player(Team::team_b)->cash() ==
                render_rate_b.find_player(Team::team_b)->cash(),
        "different render-read rates cannot affect fixed-step cash");

    World team_a_capture_reward_world;
    team_a_capture_reward_world.units().clear();
    const Money team_a_capture_cash =
        team_a_capture_reward_world.find_player(Team::team_a)->cash();
    team_a_capture_reward_world.zones()[1].advance_capture(100.0F);
    update_zone_capture(team_a_capture_reward_world, 0.0);
    award_zone_capture_rewards(team_a_capture_reward_world);
    passed &= check(
        team_a_capture_reward_world.find_player(Team::team_a)->cash() ==
            team_a_capture_cash + 1'000,
        "neutral objective capture rewards Team A exactly 1000");
    award_zone_capture_rewards(team_a_capture_reward_world);
    passed &= check(
        team_a_capture_reward_world.find_player(Team::team_a)->cash() ==
            team_a_capture_cash + 1'000,
        "reprocessing the same capture event gives no duplicate reward");
    team_a_capture_reward_world.zones()[1].advance_capture(-100.0F);
    update_zone_capture(team_a_capture_reward_world, 0.0);
    award_zone_capture_rewards(team_a_capture_reward_world);
    passed &= check(
        team_a_capture_reward_world.find_player(Team::team_a)->cash() ==
            team_a_capture_cash + 1'000,
        "objective neutralization gives no cash reward");

    World team_b_capture_reward_world;
    team_b_capture_reward_world.units().clear();
    const Money team_b_capture_cash =
        team_b_capture_reward_world.find_player(Team::team_b)->cash();
    team_b_capture_reward_world.zones()[3].advance_capture(-100.0F);
    update_zone_capture(team_b_capture_reward_world, 0.0);
    award_zone_capture_rewards(team_b_capture_reward_world);
    passed &= check(
        team_b_capture_reward_world.find_player(Team::team_b)->cash() ==
            team_b_capture_cash + 1'000,
        "neutral objective capture rewards Team B exactly once");

    MatchState default_match;
    passed &= check(
        default_match.active() &&
            default_match.phase() == MatchPhase::regulation &&
            default_match.result() == MatchResult::none &&
            default_match.remaining_ticks() == 18'000 &&
            default_match.remaining_display_seconds() == 300 &&
            default_match.rules().duration_seconds == 300,
        "match starts active at exactly 300 seconds and 18000 fixed ticks");

    constexpr MatchRules one_second_match{
        .fixed_ticks_per_second = 60,
        .duration_seconds = 1,
    };
    MatchState exact_countdown{one_second_match};
    passed &= check(exact_countdown.advance(59, 0, 0) ==
                            MatchTransition::none &&
                        exact_countdown.active() &&
                        exact_countdown.remaining_ticks() == 1 &&
                        exact_countdown.remaining_display_seconds() == 1,
                    "match countdown cannot expire before its exact final tick");
    passed &= check(exact_countdown.advance(1, 0, 0) ==
                            MatchTransition::sudden_death &&
                        exact_countdown.active() &&
                        exact_countdown.phase() == MatchPhase::sudden_death &&
                        exact_countdown.remaining_ticks() == 0 &&
                        exact_countdown.remaining_display_seconds() == 0 &&
                        exact_countdown.result() == MatchResult::none,
                    "tied regulation enters sudden death exactly at zero");
    passed &= check(exact_countdown.advance(10'000, 50, 0) ==
                            MatchTransition::none &&
                        exact_countdown.remaining_ticks() == 0 &&
                        exact_countdown.phase() == MatchPhase::sudden_death &&
                        exact_countdown.result() == MatchResult::none,
                    "sudden death transition occurs only once and has no timer");

    MatchState team_a_win{one_second_match};
    MatchState team_b_win{one_second_match};
    (void)team_a_win.advance(60, 4, 3);
    (void)team_b_win.advance(60, 8, 9);
    passed &= check(team_a_win.phase() == MatchPhase::finished &&
                        team_b_win.phase() == MatchPhase::finished &&
                        team_a_win.result() == MatchResult::team_a &&
                        team_b_win.result() == MatchResult::team_b,
                    "non-tied regulation finishes with the higher-score winner");

    World sudden_reset_world{one_second_match};
    sudden_reset_world.units().clear();
    sudden_reset_world.units().push_back(
        test_unit(1220, Team::team_a, {200.0F, 300.0F}, 270.0F));
    sudden_reset_world.units().push_back(
        test_unit(1221, Team::team_b, {1700.0F, 300.0F}, 90.0F));
    sudden_reset_world.spawn_projectile(
        WeaponType::rifle, Team::team_a, 1220, {500.0F, 900.0F},
        {100.0F, 0.0F}, 10'000.0F, 10.0F);
    sudden_reset_world.queue_deployment(Team::team_a, TroopType::rifle,
                                        {100.0F, 500.0F}, 10.0);
    sudden_reset_world.zones()[1].advance_capture(100.0F);
    sudden_reset_world.zones()[1].set_owner(Team::team_a);
    sudden_reset_world.zones()[1].update_security(2.0, 2.0);
    (void)sudden_reset_world.find_player(Team::team_a)->try_spend(4'000);
    sudden_reset_world.find_player(Team::team_b)->credit(2'000);
    sudden_reset_world.find_player(Team::team_a)->add_score(3);
    sudden_reset_world.find_player(Team::team_b)->add_score(3);
    Simulation sudden_reset_simulation{sudden_reset_world};
    for (int tick = 0; tick < 60; ++tick) {
        sudden_reset_simulation.update(1.0 / 60.0);
    }
    bool objectives_reset = true;
    for (std::size_t index = 1; index <= 3; ++index) {
        const Zone& zone = sudden_reset_world.zones()[index];
        objectives_reset &= zone.owner() == Team::none &&
                            near(zone.capture_value(), 0.0F) &&
                            zone.team_a_count() == 0 &&
                            zone.team_b_count() == 0 &&
                            !zone.secured() &&
                            near(static_cast<float>(zone.secure_timer_seconds()),
                                 0.0F);
    }
    passed &= check(
        sudden_reset_world.match_state().phase() ==
                MatchPhase::sudden_death &&
            sudden_reset_world.match_state().result() == MatchResult::none &&
            sudden_reset_world.units().empty() &&
            sudden_reset_world.projectiles().empty() &&
            sudden_reset_world.pending_deployments().empty() &&
            sudden_reset_world.death_events().empty() && objectives_reset &&
            sudden_reset_world.find_player(Team::team_a)->cash() == 25'000 &&
            sudden_reset_world.find_player(Team::team_b)->cash() == 25'000 &&
            sudden_reset_world.find_player(Team::team_a)->score() == 3 &&
            sudden_reset_world.find_player(Team::team_b)->score() == 3,
        "tied regulation performs one clean sudden-death reset while preserving scores");

    for (int tick = 0; tick < 60; ++tick) {
        sudden_reset_simulation.update(1.0 / 60.0);
    }
    passed &= check(
        sudden_reset_world.match_state().phase() ==
                MatchPhase::sudden_death &&
            sudden_reset_world.find_player(Team::team_a)->cash() == 25'200 &&
            sudden_reset_world.find_player(Team::team_b)->cash() == 25'200 &&
            sudden_reset_world.find_player(Team::team_a)->score() == 3 &&
            sudden_reset_world.find_player(Team::team_b)->score() == 3,
        "sudden death has no timer, keeps passive income, and freezes regulation scores");

    sudden_reset_world.zones()[1].advance_capture(100.0F);
    sudden_reset_world.zones()[1].set_owner(Team::team_a);
    sudden_reset_world.zones()[1].update_security(2.0, 2.0);
    passed &= check(
        request_deployment(sudden_reset_world, Team::team_a,
                           TroopType::rifle, {500.0F, 500.0F}) ==
                DeploymentResult::invalid_location &&
            request_deployment(sudden_reset_world, Team::team_a,
                               TroopType::rifle, {100.0F, 500.0F}) ==
                DeploymentResult::accepted,
        "sudden death permits home deployment but rejects secured objective origins");
    sudden_reset_world.units().push_back(
        test_unit(1222, Team::team_a, {500.0F, 300.0F}, 270.0F));
    update_zone_capture(sudden_reset_world, 0.0);
    update_objective_scoring(sudden_reset_world, 60);
    passed &= check(
        sudden_reset_world.find_player(Team::team_a)->score() == 3 &&
            !is_zone_deployable(sudden_reset_world,
                                sudden_reset_world.zones()[1],
                                Team::team_a),
        "captured sudden-death objectives neither score nor become deployable");

    constexpr MatchRules immediate_sudden_death{
        .fixed_ticks_per_second = 60,
        .duration_seconds = 0,
    };
    MapDefinition alternate_center_map = battlefield;
    alternate_center_map.center_objective_zone_index = 1;
    World map_center_world{immediate_sudden_death, alternate_center_map};
    map_center_world.reset_for_sudden_death();
    map_center_world.emit_zone_ownership_event(
        1, Team::none, Team::team_a, ZoneTransitionType::captured);
    passed &= check(
        resolve_sudden_death_center_capture(map_center_world) &&
            map_center_world.match_state().result() == MatchResult::team_a,
        "sudden-death victory resolves from the map-defined center objective");

    World partial_center_world{immediate_sudden_death};
    partial_center_world.reset_for_sudden_death();
    partial_center_world.zones()[2].advance_capture(50.0F);
    passed &= check(
        !resolve_sudden_death_center_capture(partial_center_world) &&
            partial_center_world.match_state().phase() ==
                MatchPhase::sudden_death,
        "partial center capture does not win sudden death");

    World neutralized_center_world{immediate_sudden_death};
    neutralized_center_world.reset_for_sudden_death();
    neutralized_center_world.zones()[2].advance_capture(100.0F);
    neutralized_center_world.zones()[2].set_owner(Team::team_a);
    neutralized_center_world.zones()[2].advance_capture(-100.0F);
    update_zone_capture(neutralized_center_world, 0.0);
    passed &= check(
        neutralized_center_world.zones()[2].owner() == Team::none &&
            !resolve_sudden_death_center_capture(neutralized_center_world) &&
            neutralized_center_world.match_state().phase() ==
                MatchPhase::sudden_death,
        "center neutralization alone does not win sudden death");

    World team_a_center_world{immediate_sudden_death};
    team_a_center_world.reset_for_sudden_death();
    team_a_center_world.zones()[2].advance_capture(95.0F);
    team_a_center_world.units().push_back(
        test_unit(1223, Team::team_a, {900.0F, 300.0F}, 270.0F));
    update_zone_capture(team_a_center_world, 1.0);
    passed &= check(
        resolve_sudden_death_center_capture(team_a_center_world) &&
            team_a_center_world.match_state().phase() == MatchPhase::finished &&
            team_a_center_world.match_state().result() == MatchResult::team_a &&
            !team_a_center_world.zones()[2].secured() &&
            !team_a_center_world.match_state().resolve_sudden_death(
                Team::team_b) &&
            team_a_center_world.match_state().result() == MatchResult::team_a,
        "first full Team A center capture wins once without requiring security");

    World team_b_center_world{immediate_sudden_death};
    team_b_center_world.reset_for_sudden_death();
    team_b_center_world.zones()[2].advance_capture(-95.0F);
    team_b_center_world.units().push_back(
        test_unit(1224, Team::team_b, {900.0F, 300.0F}, 90.0F));
    update_zone_capture(team_b_center_world, 1.0);
    passed &= check(
        resolve_sudden_death_center_capture(team_b_center_world) &&
            team_b_center_world.match_state().phase() == MatchPhase::finished &&
            team_b_center_world.match_state().result() == MatchResult::team_b,
        "first full Team B center capture wins sudden death");

    World simulated_center_win_world{immediate_sudden_death};
    simulated_center_win_world.reset_for_sudden_death();
    simulated_center_win_world.zones()[2].advance_capture(99.95F);
    simulated_center_win_world.units().push_back(
        test_unit(1225, Team::team_a, {900.0F, 300.0F}, 270.0F));
    Simulation simulated_center_win{simulated_center_win_world};
    simulated_center_win.update(1.0 / 60.0);
    const Vec2 center_winner_position =
        simulated_center_win_world.units().front().position();
    simulated_center_win.update(1.0 / 60.0);
    passed &= check(
        simulated_center_win_world.match_state().phase() ==
                MatchPhase::finished &&
            simulated_center_win_world.match_state().result() ==
                MatchResult::team_a &&
            near(simulated_center_win_world.units().front().position().x,
                 center_winner_position.x) &&
            near(simulated_center_win_world.units().front().position().y,
                 center_winner_position.y),
        "simulation resolves full center capture immediately and freezes afterward");

    World deterministic_reset_world{one_second_match};
    deterministic_reset_world.units().clear();
    deterministic_reset_world.find_player(Team::team_a)->add_score(3);
    deterministic_reset_world.find_player(Team::team_b)->add_score(3);
    deterministic_reset_world.find_player(Team::team_a)->credit(500);
    deterministic_reset_world.zones()[3].advance_capture(-75.0F);
    deterministic_reset_world.queue_deployment(
        Team::team_b, TroopType::bazooka, {1800.0F, 500.0F}, 5.0);
    Simulation deterministic_reset_simulation{deterministic_reset_world};
    for (int tick = 0; tick < 60; ++tick) {
        deterministic_reset_simulation.update(1.0 / 60.0);
    }
    passed &= check(
        deterministic_reset_world.match_state().phase() ==
                sudden_reset_world.match_state().phase() &&
            deterministic_reset_world.find_player(Team::team_a)->cash() ==
                25'000 &&
            deterministic_reset_world.find_player(Team::team_b)->cash() ==
                25'000 &&
            deterministic_reset_world.find_player(Team::team_a)->score() == 3 &&
            deterministic_reset_world.find_player(Team::team_b)->score() == 3 &&
            deterministic_reset_world.units().empty() &&
            deterministic_reset_world.pending_deployments().empty() &&
            near(deterministic_reset_world.zones()[3].capture_value(), 0.0F),
        "sudden-death reset is deterministic across different regulation state");

    World scoring_cadence_world;
    scoring_cadence_world.units().clear();
    scoring_cadence_world.zones()[1].advance_capture(100.0F);
    scoring_cadence_world.zones()[1].set_owner(Team::team_a);
    scoring_cadence_world.units().push_back(
        test_unit(1200, Team::team_a, {500.0F, 200.0F}, 0.0F));
    update_zone_capture(scoring_cadence_world, 0.0);
    update_objective_scoring(scoring_cadence_world, 59);
    passed &= check(
        scoring_cadence_world.find_player(Team::team_a)->score() == 0 &&
            scoring_cadence_world.scoring_tick_progress() == 59,
        "owned and occupied objective waits for the exact one-second score tick");
    update_objective_scoring(scoring_cadence_world, 1);
    passed &= check(
        scoring_cadence_world.find_player(Team::team_a)->score() == 1 &&
            scoring_cadence_world.scoring_tick_progress() == 0,
        "fully owned objective with a living owner occupant scores at one second");
    update_objective_scoring(scoring_cadence_world, 59);
    passed &= check(
        scoring_cadence_world.find_player(Team::team_a)->score() == 1,
        "objective cannot award a duplicate point within one scoring interval");
    scoring_cadence_world.units().push_back(
        test_unit(1207, Team::team_b, {550.0F, 200.0F}, 0.0F));
    update_zone_capture(scoring_cadence_world, 0.0);
    update_objective_scoring(scoring_cadence_world, 1);
    passed &= check(
        scoring_cadence_world.zones()[1].contested() &&
            scoring_cadence_world.find_player(Team::team_a)->score() == 2,
        "contested fully owned objective still scores when its owner is present");

    World partial_owned_scoring_world;
    partial_owned_scoring_world.units().clear();
    partial_owned_scoring_world.zones()[1].advance_capture(50.0F);
    partial_owned_scoring_world.zones()[1].set_owner(Team::team_a);
    partial_owned_scoring_world.units().push_back(
        test_unit(1208, Team::team_a, {500.0F, 200.0F}, 0.0F));
    update_zone_capture(partial_owned_scoring_world, 0.0);
    update_objective_scoring(partial_owned_scoring_world, 60);
    passed &= check(
        partial_owned_scoring_world.find_player(Team::team_a)->score() == 0,
        "owned objective below full capture does not score");

    World empty_scoring_world;
    empty_scoring_world.units().clear();
    empty_scoring_world.zones()[1].advance_capture(100.0F);
    empty_scoring_world.zones()[1].set_owner(Team::team_a);
    update_zone_capture(empty_scoring_world, 0.0);
    update_objective_scoring(empty_scoring_world, 60);
    passed &= check(empty_scoring_world.find_player(Team::team_a)->score() == 0,
                    "owned but empty objective does not score");
    empty_scoring_world.units().push_back(
        test_unit(1209, Team::team_a, {500.0F, 200.0F}, 0.0F));
    update_zone_capture(empty_scoring_world, 0.0);
    update_objective_scoring(empty_scoring_world, 60);
    passed &= check(empty_scoring_world.find_player(Team::team_a)->score() == 1,
                    "owner re-entering an owned objective resumes scoring");

    World enemy_only_scoring_world;
    enemy_only_scoring_world.units().clear();
    enemy_only_scoring_world.zones()[1].advance_capture(100.0F);
    enemy_only_scoring_world.zones()[1].set_owner(Team::team_a);
    enemy_only_scoring_world.units().push_back(
        test_unit(1201, Team::team_b, {500.0F, 200.0F}, 0.0F));
    update_zone_capture(enemy_only_scoring_world, 0.0);
    update_objective_scoring(enemy_only_scoring_world, 60);
    passed &= check(
        enemy_only_scoring_world.find_player(Team::team_a)->score() == 0 &&
            enemy_only_scoring_world.find_player(Team::team_b)->score() == 0,
        "enemy-only occupation of an owned objective does not score");

    World neutral_scoring_world;
    neutral_scoring_world.units().clear();
    neutral_scoring_world.units().push_back(
        test_unit(1202, Team::team_a, {500.0F, 200.0F}, 0.0F));
    update_zone_capture(neutral_scoring_world, 0.0);
    update_objective_scoring(neutral_scoring_world, 60);
    passed &= check(neutral_scoring_world.find_player(Team::team_a)->score() == 0,
                    "neutral objective does not score despite occupation");

    World multi_objective_scoring_world;
    multi_objective_scoring_world.units().clear();
    multi_objective_scoring_world.zones()[1].advance_capture(100.0F);
    multi_objective_scoring_world.zones()[1].set_owner(Team::team_a);
    multi_objective_scoring_world.zones()[2].advance_capture(-100.0F);
    multi_objective_scoring_world.zones()[2].set_owner(Team::team_b);
    multi_objective_scoring_world.zones()[3].advance_capture(100.0F);
    multi_objective_scoring_world.zones()[3].set_owner(Team::team_a);
    multi_objective_scoring_world.units().push_back(
        test_unit(1203, Team::team_a, {500.0F, 200.0F}, 0.0F));
    multi_objective_scoring_world.units().push_back(
        test_unit(1204, Team::team_b, {900.0F, 200.0F}, 0.0F));
    multi_objective_scoring_world.units().push_back(
        test_unit(1205, Team::team_a, {1300.0F, 200.0F}, 0.0F));
    update_zone_capture(multi_objective_scoring_world, 0.0);
    update_objective_scoring(multi_objective_scoring_world, 60);
    passed &= check(
        multi_objective_scoring_world.find_player(Team::team_a)->score() == 2 &&
            multi_objective_scoring_world.find_player(Team::team_b)->score() == 1,
        "all three objectives score independently and can award on the same tick");

    World home_scoring_world;
    home_scoring_world.units().clear();
    home_scoring_world.units().push_back(
        test_unit(1206, Team::team_a, {100.0F, 200.0F}, 0.0F));
    update_zone_capture(home_scoring_world, 0.0);
    update_objective_scoring(home_scoring_world, 60);
    passed &= check(home_scoring_world.find_player(Team::team_a)->score() == 0,
                    "permanently owned home zones never score");

    World finished_scoring_world{one_second_match};
    finished_scoring_world.units().clear();
    finished_scoring_world.zones()[1].advance_capture(100.0F);
    finished_scoring_world.zones()[1].set_owner(Team::team_a);
    finished_scoring_world.units().push_back(
        test_unit(1210, Team::team_a, {500.0F, 300.0F}, 0.0F));
    update_zone_capture(finished_scoring_world, 0.0);
    Simulation finished_scoring_simulation{finished_scoring_world};
    for (int tick = 0; tick < 60; ++tick) {
        finished_scoring_simulation.update(1.0 / 60.0);
    }
    const Score final_scoring_score =
        finished_scoring_world.find_player(Team::team_a)->score();
    update_objective_scoring(finished_scoring_world, 60);
    passed &= check(
        final_scoring_score == 1 &&
            finished_scoring_world.find_player(Team::team_a)->score() == 1 &&
            finished_scoring_world.match_state().result() == MatchResult::team_a,
        "final active second scores before result resolution and cannot score afterward");

    World frozen_world{one_second_match};
    frozen_world.units().clear();
    frozen_world.zones()[1].advance_capture(50.0F);
    frozen_world.zones()[1].set_owner(Team::team_a);
    frozen_world.find_player(Team::team_a)->add_score(1);
    frozen_world.units().push_back(
        test_unit(1211, Team::team_a, {500.0F, 400.0F}, 270.0F));
    const Unit::Id frozen_unit_id = frozen_world.units().front().id();
    frozen_world.queue_deployment(Team::team_a, TroopType::rifle,
                                  {100.0F, 500.0F}, 10.0);
    frozen_world.spawn_projectile(WeaponType::rifle, Team::team_a,
                                  frozen_unit_id, {100.0F, 900.0F},
                                  {100.0F, 0.0F}, 10'000.0F, 10.0F);
    Simulation frozen_simulation{frozen_world};
    for (int tick = 0; tick < 60; ++tick) {
        frozen_simulation.update(1.0 / 60.0);
    }
    const Vec2 frozen_position = frozen_world.units().front().position();
    const Vec2 frozen_projectile_position =
        frozen_world.projectiles().front().position();
    const float frozen_capture = frozen_world.zones()[1].capture_value();
    const Money frozen_cash =
        frozen_world.find_player(Team::team_a)->cash();
    const Score frozen_score =
        frozen_world.find_player(Team::team_a)->score();
    const double frozen_deployment_time =
        frozen_world.pending_deployments().front().remaining_seconds;
    const std::size_t frozen_projectile_count =
        frozen_world.projectiles().size();
    const std::array<Unit::Id, 1> frozen_ids{frozen_unit_id};
    passed &= check(
        apply_tactical_order(frozen_world, frozen_ids,
                             TacticalOrder::hold) == 0 &&
            !assign_movement_path(frozen_world, frozen_unit_id,
                                  {{700.0F, 400.0F}}) &&
            request_deployment(frozen_world, Team::team_a, TroopType::rifle,
                               {100.0F, 500.0F}) ==
                DeploymentResult::match_finished,
        "deployment, tactical commands, and paths are rejected after finish");
    frozen_world.units().push_back(test_unit(
        1212, Team::team_b, frozen_position + Vec2{40.0F, 0.0F}, 90.0F));
    const float frozen_enemy_health = frozen_world.units().back().health();
    update_zone_capture(frozen_world, 10.0);
    update_passive_income(frozen_world, 60);
    update_pending_deployments(frozen_world, 20.0);
    update_objective_scoring(frozen_world, 60);
    for (int tick = 0; tick < 120; ++tick) {
        frozen_simulation.update(1.0 / 60.0);
    }
    passed &= check(
        frozen_simulation.tick_count() == 60 &&
            near(frozen_world.units().front().position().x,
                 frozen_position.x) &&
            near(frozen_world.units().front().position().y,
                 frozen_position.y) &&
            near(frozen_world.projectiles().front().position().x,
                 frozen_projectile_position.x) &&
            near(frozen_world.projectiles().front().position().y,
                 frozen_projectile_position.y) &&
            near(frozen_world.zones()[1].capture_value(), frozen_capture) &&
            frozen_world.find_player(Team::team_a)->cash() == frozen_cash &&
            frozen_world.find_player(Team::team_a)->score() == frozen_score &&
            frozen_world.projectiles().size() == frozen_projectile_count &&
            std::abs(frozen_world.pending_deployments().front()
                         .remaining_seconds -
                     frozen_deployment_time) < 1.0e-9 &&
            !frozen_world.units().front().target_id().has_value() &&
            !frozen_world.units().front().has_movement_path() &&
            near(frozen_world.units().back().health(), frozen_enemy_health),
        "finished simulation freezes movement, combat, projectiles, capture, economy, scoring, and pending deployment");

    World purchase_world;
    purchase_world.units().clear();
    const Money purchase_start =
        purchase_world.find_player(Team::team_a)->cash();
    passed &= check(
        request_deployment(purchase_world, Team::team_a, TroopType::rifle,
                           {100.0F, 420.0F}) == DeploymentResult::accepted &&
            purchase_world.find_player(Team::team_a)->cash() ==
                purchase_start - 2'500 &&
            purchase_world.pending_deployments().size() == 1,
        "valid home-zone rifle purchase deducts cash once and queues deployment");

    const Money after_purchase =
        purchase_world.find_player(Team::team_a)->cash();
    passed &= check(
        request_deployment(purchase_world, Team::team_a,
                           TroopType::machine_gun, {800.0F, 420.0F}) ==
                DeploymentResult::invalid_location &&
            purchase_world.find_player(Team::team_a)->cash() == after_purchase &&
            purchase_world.pending_deployments().size() == 1,
        "neutral-zone placement is rejected without charging");
    passed &= check(
        request_deployment(purchase_world, Team::team_a, TroopType::bazooka,
                           {1800.0F, 420.0F}) ==
                DeploymentResult::invalid_location &&
            purchase_world.find_player(Team::team_a)->cash() == after_purchase,
        "enemy home-zone placement is rejected without charging");

    World unsecured_world;
    unsecured_world.units().clear();
    unsecured_world.zones()[1].set_owner(Team::team_a);
    passed &= check(
        request_deployment(unsecured_world, Team::team_a, TroopType::rifle,
                           {500.0F, 420.0F}) ==
                DeploymentResult::invalid_location &&
            unsecured_world.find_player(Team::team_a)->cash() == 25'000,
        "owned but unsecured objective rejects placement without charging");

    World secured_world;
    secured_world.units().clear();
    secured_world.zones()[1].advance_capture(100.0F);
    update_zone_capture(secured_world, 0.0);
    update_zone_capture(secured_world, 2.0);
    passed &= check(
        request_deployment(secured_world, Team::team_a,
                           TroopType::machine_gun, {500.0F, 420.0F}) ==
                DeploymentResult::accepted &&
            secured_world.find_player(Team::team_a)->cash() == 21'000,
        "secured forward objective accepts owner placement and charges cost");
    const Money secured_cash =
        secured_world.find_player(Team::team_a)->cash();
    passed &= check(
        request_deployment(secured_world, Team::team_a, TroopType::rifle,
                           {700.0F, 420.0F}) ==
                DeploymentResult::invalid_location &&
            secured_world.find_player(Team::team_a)->cash() == secured_cash,
        "secured objective front 25 percent remains invalid without charging");

    World insufficient_world;
    insufficient_world.units().clear();
    PlayerState* poor_player = insufficient_world.find_player(Team::team_a);
    const bool drained_cash = poor_player->try_spend(25'000);
    passed &= check(
        drained_cash &&
            request_deployment(insufficient_world, Team::team_a,
                               TroopType::bazooka, {100.0F, 420.0F}) ==
                DeploymentResult::insufficient_cash &&
            poor_player->cash() == 0 &&
            insufficient_world.pending_deployments().empty(),
        "insufficient cash rejects purchase without queueing or overdrawing");

    World countdown_world;
    countdown_world.units().clear();
    passed &= check(
        request_deployment(countdown_world, Team::team_a, TroopType::bazooka,
                           {120.0F, 321.0F}) == DeploymentResult::accepted,
        "bazooka deployment request is accepted in Team A home zone");
    update_pending_deployments(countdown_world, 1.74);
    passed &= check(countdown_world.units().empty() &&
                        countdown_world.pending_deployments().size() == 1 &&
                        std::abs(countdown_world.pending_deployments().front()
                                     .remaining_seconds -
                                 0.01) < 1.0e-8,
                    "unit does not spawn before its deterministic timer expires");
    update_pending_deployments(countdown_world, 0.01);
    passed &= check(countdown_world.pending_deployments().empty() &&
                        countdown_world.units().size() == 1 &&
                        countdown_world.units().front().troop_type() ==
                            TroopType::bazooka &&
                        near(countdown_world.units().front().position().y,
                             321.0F) &&
                        near(countdown_world.units().front().preferred_y(),
                             321.0F),
                    "timer completion spawns the requested troop at placement preferred_y");

    World partitioned_timer_world;
    World single_step_timer_world;
    partitioned_timer_world.units().clear();
    single_step_timer_world.units().clear();
    const auto partitioned_request = request_deployment(
        partitioned_timer_world, Team::team_a, TroopType::rifle,
        {100.0F, 300.0F});
    const auto single_step_request = request_deployment(
        single_step_timer_world, Team::team_a, TroopType::rifle,
        {100.0F, 300.0F});
    for (int tick = 0; tick < 45; ++tick) {
        update_pending_deployments(partitioned_timer_world, 1.0 / 60.0);
    }
    update_pending_deployments(single_step_timer_world, 0.75);
    passed &= check(partitioned_request == DeploymentResult::accepted &&
                        single_step_request == DeploymentResult::accepted &&
                        partitioned_timer_world.units().size() == 1 &&
                        single_step_timer_world.units().size() == 1 &&
                        partitioned_timer_world.pending_deployments().empty() &&
                        single_step_timer_world.pending_deployments().empty(),
                    "deployment countdown is deterministic across timestep partitions");

    passed &= check(
        rifle_definition.purchase_cost == 2'500 &&
            std::abs(rifle_definition.deployment_seconds - 0.75) < 1.0e-9 &&
            machine_gun_definition.purchase_cost == 4'000 &&
            std::abs(machine_gun_definition.deployment_seconds - 1.25) <
                1.0e-9 &&
            bazooka_definition.purchase_cost == 6'000 &&
            std::abs(bazooka_definition.deployment_seconds - 1.75) < 1.0e-9,
        "each troop exposes its centralized purchase cost and deployment time");
    passed &= check(
        troop_display_name(TroopType::rifle) == "Rifleman" &&
            troop_display_name(TroopType::machine_gun) == "Machine Gun" &&
            troop_display_name(TroopType::bazooka) == "Bazooka" &&
            to_string(TroopType::rifle) == "rifle" &&
            to_string(TroopType::machine_gun) == "machine_gun" &&
            to_string(TroopType::bazooka) == "bazooka",
        "display names are user-facing while internal troop IDs stay unchanged");

    World frontline_world;
    frontline_world.units().clear();
    const auto initial_a_frontline =
        frontline_objective(frontline_world, Team::team_a);
    const auto initial_b_frontline =
        frontline_objective(frontline_world, Team::team_b);
    const float initial_a_hold_x =
        initial_a_frontline.has_value() ? initial_a_frontline->hold_x : 652.8F;
    passed &= check(initial_a_frontline.has_value() &&
                        initial_a_frontline->zone_index == 1 &&
                        near(initial_a_frontline->forward_boundary_x, 768.0F) &&
                        initial_b_frontline.has_value() &&
                        initial_b_frontline->zone_index == 3 &&
                        near(initial_b_frontline->forward_boundary_x, 1152.0F),
                    "frontline objective selection is mirrored by team");

    World team_a_gate_world;
    team_a_gate_world.units().clear();
    team_a_gate_world.units().push_back(
        test_unit(5000, Team::team_a, {767.5F, 300.0F}, 270.0F));
    team_a_gate_world.units().push_back(
        test_unit(5001, Team::team_b, {1100.0F, 300.0F}, 90.0F));
    Simulation team_a_gate_simulation{team_a_gate_world};
    team_a_gate_simulation.update(1.0);
    passed &= check(team_a_gate_world.units()[0].position().x < 768.0F,
                    "Team A combat pursuit cannot cross uncaptured frontline boundary");

    World team_b_gate_world;
    team_b_gate_world.units().clear();
    team_b_gate_world.units().push_back(
        test_unit(5010, Team::team_b, {1152.5F, 300.0F}, 90.0F));
    team_b_gate_world.units().push_back(
        test_unit(5011, Team::team_a, {820.0F, 300.0F}, 270.0F));
    Simulation team_b_gate_simulation{team_b_gate_world};
    team_b_gate_simulation.update(1.0);
    passed &= check(team_b_gate_world.units()[0].position().x > 1152.0F,
                    "Team B combat pursuit obeys mirrored frontline boundary");

    World frontline_entry_world;
    frontline_entry_world.units().clear();
    frontline_entry_world.units().push_back(
        test_unit(5020, Team::team_a, {383.5F, 300.0F}, 270.0F));
    Simulation frontline_entry_simulation{frontline_entry_world};
    frontline_entry_simulation.update(1.0 / 60.0);
    passed &= check(frontline_entry_world.units()[0].position().x > 384.0F,
                    "units may enter their current frontline objective");

    World frontline_hold_world;
    frontline_hold_world.units().clear();
    frontline_hold_world.units().push_back(
        test_unit(5021, Team::team_a,
                  {initial_a_hold_x, 300.0F}, 270.0F));
    Simulation frontline_hold_simulation{frontline_hold_world};
    for (int tick = 0; tick < 120; ++tick) {
        frontline_hold_simulation.update(1.0 / 60.0);
    }
    passed &= check(
        std::abs(frontline_hold_world.units()[0].position().x -
                 initial_a_hold_x) < 1.0F &&
            frontline_hold_world.units()[0].position().x < 700.0F,
        "targetless unit settles at interior frontline hold instead of boundary");

    World unlocked_frontline_world;
    unlocked_frontline_world.units().clear();
    unlocked_frontline_world.zones()[1].advance_capture(100.0F);
    update_zone_capture(unlocked_frontline_world, 0.0);
    const auto advanced_frontline =
        frontline_objective(unlocked_frontline_world, Team::team_a);
    unlocked_frontline_world.units().push_back(
        test_unit(5030, Team::team_a, {767.5F, 300.0F}, 270.0F));
    Simulation unlocked_frontline_simulation{unlocked_frontline_world};
    unlocked_frontline_simulation.update(0.1);
    passed &= check(advanced_frontline.has_value() &&
                        advanced_frontline->zone_index == 2 &&
                        near(advanced_frontline->forward_boundary_x, 1152.0F) &&
                        unlocked_frontline_world.units()[0].position().x >
                            768.0F,
                    "full ownership unlocks advancement and moves limit to next objective");

    unlocked_frontline_world.units().clear();
    unlocked_frontline_world.units().push_back(
        test_unit(5031, Team::team_a, {1151.5F, 300.0F}, 270.0F));
    unlocked_frontline_world.units().push_back(
        test_unit(5032, Team::team_b, {1490.0F, 300.0F}, 90.0F));
    Simulation next_gate_simulation{unlocked_frontline_world};
    next_gate_simulation.update(1.0);
    passed &= check(unlocked_frontline_world.units()[0].position().x < 1152.0F,
                    "new frontline objective becomes the next movement limit");

    World frontline_combat_world;
    frontline_combat_world.units().clear();
    frontline_combat_world.units().push_back(
        test_unit(5040, Team::team_a, {600.0F, 300.0F}, 270.0F));
    frontline_combat_world.units().push_back(
        test_unit(5041, Team::team_b, {620.0F, 300.0F}, 90.0F));
    Simulation frontline_combat_simulation{frontline_combat_world};
    frontline_combat_simulation.update(0.1);
    passed &= check(frontline_combat_world.units()[0].position().x < 600.0F &&
                        frontline_combat_world.units()[1].position().x > 620.0F &&
                        frontline_combat_world.units()[0]
                                .combat_movement_state() ==
                            CombatMovementState::retreating,
                    "close-range retreat remains active inside the frontline");

    World frontline_closing_world;
    frontline_closing_world.units().clear();
    frontline_closing_world.units().push_back(
        test_unit(5050, Team::team_a, {420.0F, 300.0F}, 270.0F));
    frontline_closing_world.units().push_back(
        test_unit(5051, Team::team_b, {740.0F, 300.0F}, 90.0F));
    Simulation frontline_closing_simulation{frontline_closing_world};
    frontline_closing_simulation.update(0.1);
    passed &= check(frontline_closing_world.units()[0].position().x > 420.0F &&
                        frontline_closing_world.units()[0]
                                .combat_movement_state() ==
                            CombatMovementState::closing,
                    "combat closing remains active within the frontline objective");

    World advance_order_world;
    advance_order_world.units().clear();
    advance_order_world.units().push_back(
        test_unit(5060, Team::team_a, {767.5F, 300.0F}, 270.0F));
    const std::array<Unit::Id, 1> advance_ids{5060};
    (void)apply_tactical_order(advance_order_world, advance_ids,
                               TacticalOrder::advance);
    Simulation advance_order_simulation{advance_order_world};
    advance_order_simulation.update(1.0);
    passed &= check(
        advance_order_world.units()[0].position().x > 767.5F &&
            advance_order_world.units()[0].position().x < 768.0F &&
            advance_order_world.units()[0].tactical_order() ==
                TacticalOrder::advance,
        "Advance pushes deliberately but cannot cross the uncaptured frontline");

    World hold_world;
    hold_world.units().clear();
    hold_world.units().push_back(
        test_unit(5070, Team::team_a, {500.0F, 200.0F}, 0.0F));
    hold_world.units().push_back(
        test_unit(5071, Team::team_b, {500.0F, 650.0F}, 0.0F));
    const std::array<Unit::Id, 1> hold_ids{5070};
    (void)apply_tactical_order(hold_world, hold_ids, TacticalOrder::hold);
    const Vec2 hold_anchor = *hold_world.units()[0].tactical_position();
    Simulation hold_simulation{hold_world};
    for (int tick = 0; tick < 120; ++tick) {
        hold_simulation.update(1.0 / 60.0);
    }
    passed &= check(
        near(hold_anchor.x, 500.0F) && near(hold_anchor.y, 200.0F) &&
            length(hold_world.units()[0].position() - hold_anchor) <=
                default_tactical_rules.hold_leash_radius + 0.01F,
        "Hold stores the issue position and prevents long-distance pursuit");

    World idle_hold_world;
    idle_hold_world.units().clear();
    idle_hold_world.units().push_back(
        test_unit(5072, Team::team_a, {500.0F, 200.0F}, 0.0F));
    const std::array<Unit::Id, 1> idle_hold_ids{5072};
    (void)apply_tactical_order(idle_hold_world, idle_hold_ids,
                               TacticalOrder::hold);
    Simulation idle_hold_simulation{idle_hold_world};
    idle_hold_simulation.update(1.0 / 60.0);
    passed &= check(
        near(idle_hold_world.units()[0].desired_facing_angle(), 270.0F) &&
            near(idle_hold_world.units()[0].facing_angle(), 358.5F),
        "an idle Hold unit gradually turns toward its map-defined forward direction");

    World hold_combat_world;
    hold_combat_world.units().clear();
    hold_combat_world.units().push_back(
        test_unit(5080, Team::team_a, {500.0F, 200.0F}, 0.0F));
    hold_combat_world.units().push_back(
        test_unit(5081, Team::team_b, {500.0F, 480.0F}, 180.0F));
    const std::array<Unit::Id, 1> hold_combat_ids{5080};
    (void)apply_tactical_order(hold_combat_world, hold_combat_ids,
                               TacticalOrder::hold);
    Simulation hold_combat_simulation{hold_combat_world};
    hold_combat_simulation.update(1.0 / 60.0);
    passed &= check(
        hold_combat_world.units()[0].target_id() == 5081 &&
            !hold_combat_world.projectiles().empty() &&
            hold_combat_world.units()[0].tactical_order() ==
                TacticalOrder::hold,
        "Hold preserves target acquisition, tracking, and firing");

    World regroup_world;
    regroup_world.units().clear();
    regroup_world.units().push_back(
        test_unit(5092, Team::team_a, {650.0F, 500.0F}, 270.0F));
    regroup_world.units().push_back(
        test_unit(5091, Team::team_a, {450.0F, 500.0F}, 270.0F));
    const std::array<Unit::Id, 2> regroup_ids{5092, 5091};
    passed &= check(
        apply_tactical_order(regroup_world, regroup_ids,
                             TacticalOrder::regroup) == 2 &&
            regroup_world.units()[0].tactical_position().has_value() &&
            regroup_world.units()[1].tactical_position().has_value() &&
            near(regroup_world.units()[0].tactical_position()->x, 550.0F) &&
            near(regroup_world.units()[1].tactical_position()->x, 550.0F),
        "Regroup assigns one deterministic center to the selected living units");
    const float initial_regroup_separation =
        length(regroup_world.units()[0].position() -
               regroup_world.units()[1].position());
    Simulation regroup_simulation{regroup_world};
    regroup_simulation.update(1.0 / 60.0);
    const bool regroup_uses_movement_heading =
        near(regroup_world.units()[0].desired_facing_angle(), 90.0F) &&
        near(regroup_world.units()[1].desired_facing_angle(), 270.0F);
    bool regroup_completed = false;
    for (int tick = 1; tick < 240; ++tick) {
        regroup_simulation.update(1.0 / 60.0);
        if (regroup_world.units()[0].tactical_order() ==
                TacticalOrder::automatic &&
            regroup_world.units()[1].tactical_order() ==
                TacticalOrder::automatic) {
            regroup_completed = true;
            break;
        }
    }
    passed &= check(
        regroup_uses_movement_heading && regroup_completed &&
            length(regroup_world.units()[0].position() -
                   regroup_world.units()[1].position()) <
                initial_regroup_separation,
        "Regroup faces its movement direction, consolidates, and completes back to auto");

    World deterministic_regroup_world;
    deterministic_regroup_world.units().clear();
    deterministic_regroup_world.units().push_back(
        test_unit(5101, Team::team_a, {100.0F, 300.0F}, 270.0F));
    deterministic_regroup_world.units().push_back(
        test_unit(5102, Team::team_a, {500.0F, 700.0F}, 270.0F));
    const std::array<Unit::Id, 2> forward_ids{5101, 5102};
    const std::array<Unit::Id, 2> reverse_ids{5102, 5101};
    (void)apply_tactical_order(deterministic_regroup_world, forward_ids,
                               TacticalOrder::regroup);
    const Vec2 first_center =
        *deterministic_regroup_world.units()[0].tactical_position();
    (void)apply_tactical_order(deterministic_regroup_world, reverse_ids,
                               TacticalOrder::regroup);
    const Vec2 second_center =
        *deterministic_regroup_world.units()[0].tactical_position();
    (void)apply_tactical_order(deterministic_regroup_world, forward_ids,
                               TacticalOrder::automatic);
    passed &= check(
        near(first_center.x, second_center.x) &&
            near(first_center.y, second_center.y) &&
            deterministic_regroup_world.units()[0].tactical_order() ==
                TacticalOrder::automatic &&
            !deterministic_regroup_world.units()[0]
                 .tactical_position()
                 .has_value(),
        "Regroup is ID-order deterministic and Resume Auto clears manual intent");

    World path_world;
    path_world.units().clear();
    path_world.units().push_back(
        test_unit(5110, Team::team_a, {400.0F, 400.0F}, 270.0F));
    path_world.units()[0].set_tactical_order(TacticalOrder::hold,
                                              Vec2{400.0F, 400.0F});
    path_world.units()[0].replace_movement_path(
        {{470.0F, 400.0F}, {470.0F, 520.0F}});
    passed &= check(
        path_world.units()[0].tactical_order() == TacticalOrder::automatic &&
            path_world.units()[0].remaining_waypoint_count() == 2,
        "a new individual path replaces the previous tactical order");
    Simulation path_simulation{path_world};
    bool reached_second_waypoint = false;
    bool followed_first_segment = false;
    bool path_completed = false;
    for (int tick = 0; tick < 480; ++tick) {
        path_simulation.update(1.0 / 60.0);
        const Unit& path_unit = path_world.units()[0];
        if (!reached_second_waypoint &&
            path_unit.remaining_waypoint_count() == 1) {
            reached_second_waypoint = true;
            followed_first_segment =
                path_unit.position().x >= 456.0F &&
                std::abs(path_unit.position().y - 400.0F) < 2.0F;
        }
        if (!path_unit.has_movement_path()) {
            path_completed = true;
            break;
        }
    }
    passed &= check(
        reached_second_waypoint && followed_first_segment && path_completed &&
            near(path_world.units()[0].preferred_y(), 520.0F) &&
            path_world.units()[0].tactical_order() == TacticalOrder::automatic,
        "troop follows waypoints in sequence, clears the final path, adopts final Y, and returns to auto");

    path_world.units()[0].replace_movement_path({{600.0F, 520.0F}});
    path_world.units()[0].replace_movement_path(
        {{430.0F, 430.0F}, {450.0F, 450.0F}});
    passed &= check(
        path_world.units()[0].remaining_waypoint_count() == 2 &&
            near(path_world.units()[0].current_waypoint()->x, 430.0F),
        "replacement path completely replaces older waypoints");
    const std::array<Unit::Id, 1> path_command_ids{5110};
    (void)apply_tactical_order(path_world, path_command_ids,
                               TacticalOrder::advance);
    passed &= check(!path_world.units()[0].has_movement_path() &&
                        path_world.units()[0].tactical_order() ==
                            TacticalOrder::advance,
                    "a later group command cancels the affected individual path");

    World path_combat_world;
    path_combat_world.units().clear();
    path_combat_world.units().push_back(
        test_unit(5120, Team::team_a, {500.0F, 300.0F}, 270.0F));
    path_combat_world.units().push_back(
        test_unit(5121, Team::team_b, {520.0F, 300.0F}, 90.0F));
    path_combat_world.units()[0].replace_movement_path({{700.0F, 300.0F}});
    Simulation path_combat_simulation{path_combat_world};
    path_combat_simulation.update(0.1);
    const float danger_position_x = path_combat_world.units()[0].position().x;
    const bool path_survived_combat =
        path_combat_world.units()[0].has_movement_path() &&
        path_combat_world.units()[0].combat_movement_state() ==
            CombatMovementState::retreating &&
        danger_position_x < 500.0F;
    path_combat_world.units().erase(path_combat_world.units().begin() + 1);
    path_combat_simulation.update(0.1);
    passed &= check(
        path_survived_combat &&
            path_combat_world.units()[0].has_movement_path() &&
            path_combat_world.units()[0].position().x > danger_position_x,
        "immediate combat retreat preserves the path and movement resumes when pressure clears");

    World path_frontline_world;
    path_frontline_world.units().clear();
    path_frontline_world.units().push_back(
        test_unit(5130, Team::team_a, {767.5F, 300.0F}, 270.0F));
    path_frontline_world.units()[0].replace_movement_path({{900.0F, 300.0F}});
    Simulation path_frontline_simulation{path_frontline_world};
    path_frontline_simulation.update(1.0);
    passed &= check(
        path_frontline_world.units()[0].position().x < 768.0F &&
            path_frontline_world.units()[0].has_movement_path() &&
            near(path_frontline_world.units()[0].current_waypoint()->x,
                 900.0F),
        "individual paths cannot bypass the uncaptured frontline and retain the blocked waypoint");

    World terminal_a_world;
    terminal_a_world.units().clear();
    for (std::size_t index = 1; index <= 3; ++index) {
        terminal_a_world.zones()[index].advance_capture(100.0F);
    }
    update_zone_capture(terminal_a_world, 0.0);
    const auto terminal_a_frontline =
        frontline_objective(terminal_a_world, Team::team_a);
    terminal_a_world.units().push_back(
        test_unit(5140, Team::team_a, {1535.5F, 300.0F}, 270.0F));
    Simulation terminal_a_simulation{terminal_a_world};
    terminal_a_simulation.update(1.0);
    passed &= check(
        terminal_a_frontline.has_value() &&
            terminal_a_frontline->zone_index == 3 &&
            near(terminal_a_frontline->forward_boundary_x, 1536.0F) &&
            terminal_a_world.units()[0].position().x < 1536.0F,
        "Team A terminal frontline prevents autonomous entry into Team B home");

    terminal_a_world.units()[0].set_position({1535.5F, 300.0F});
    const std::array<Unit::Id, 1> terminal_advance_ids{5140};
    (void)apply_tactical_order(terminal_a_world, terminal_advance_ids,
                               TacticalOrder::advance);
    terminal_a_simulation.update(1.0);
    const bool terminal_advance_blocked =
        terminal_a_world.units()[0].position().x < 1536.0F;
    terminal_a_world.units()[0].set_position({1535.5F, 300.0F});
    terminal_a_world.units()[0].replace_movement_path({{1800.0F, 300.0F}});
    terminal_a_simulation.update(1.0);
    passed &= check(
        terminal_advance_blocked &&
            terminal_a_world.units()[0].position().x < 1536.0F &&
            terminal_a_world.units()[0].has_movement_path(),
        "Tactical Advance and individual paths share Team A terminal home boundary");

    terminal_a_world.units()[0].clear_movement_path();
    terminal_a_world.units()[0].set_tactical_order(TacticalOrder::automatic);
    terminal_a_world.units()[0].set_position(
        {terminal_a_frontline->hold_x, 300.0F});
    for (int tick = 0; tick < 120; ++tick) {
        terminal_a_simulation.update(1.0 / 60.0);
    }
    passed &= check(
        std::abs(terminal_a_world.units()[0].position().x -
                 terminal_a_frontline->hold_x) < 1.0F &&
            terminal_a_world.units()[0].position().x < 1536.0F,
        "owning every objective holds targetless Team A troops inside zone 3");

    terminal_a_world.units().clear();
    terminal_a_world.units().push_back(test_unit(
        5141, Team::team_a, {terminal_a_frontline->hold_x, 300.0F}, 0.0F));
    terminal_a_simulation.update(1.0 / 60.0);
    const bool terminal_blue_faces_forward_gradually =
        terminal_a_world.units()[0].movement_state() == MovementState::idle &&
        near(terminal_a_world.units()[0].desired_facing_angle(), 270.0F) &&
        near(terminal_a_world.units()[0].facing_angle(), 358.5F);
    terminal_a_world.units()[0].set_tactical_order(
        TacticalOrder::regroup, terminal_a_world.units()[0].position());
    terminal_a_simulation.update(1.0 / 60.0);
    passed &= check(
        terminal_blue_faces_forward_gradually &&
            terminal_a_world.units()[0].tactical_order() ==
                TacticalOrder::automatic &&
            near(terminal_a_world.units()[0].desired_facing_angle(), 270.0F),
        "terminal Team A idle and completed Regroup both return to forward facing without snapping");

    World idle_target_facing_world;
    idle_target_facing_world.units().clear();
    for (std::size_t index = 1; index <= 3; ++index) {
        idle_target_facing_world.zones()[index].advance_capture(100.0F);
    }
    update_zone_capture(idle_target_facing_world, 0.0);
    const auto idle_target_frontline =
        frontline_objective(idle_target_facing_world, Team::team_a);
    idle_target_facing_world.units().push_back(test_unit(
        5142, Team::team_a, {idle_target_frontline->hold_x, 300.0F}, 270.0F));
    idle_target_facing_world.units().push_back(test_unit(
        5143, Team::team_b, {idle_target_frontline->hold_x, 380.0F}, 90.0F));
    Simulation idle_target_facing_simulation{idle_target_facing_world};
    idle_target_facing_simulation.update(1.0 / 60.0);
    const bool target_overrode_idle_facing =
        idle_target_facing_world.units()[0].target_id() == 5143 &&
        near(idle_target_facing_world.units()[0].desired_facing_angle(), 0.0F);
    idle_target_facing_world.units().erase(
        idle_target_facing_world.units().begin() + 1);
    for (int tick = 0; tick < 300; ++tick) {
        idle_target_facing_simulation.update(1.0 / 60.0);
    }
    passed &= check(
        target_overrode_idle_facing &&
            !idle_target_facing_world.units()[0].target_id().has_value() &&
            near(idle_target_facing_world.units()[0].desired_facing_angle(),
                 270.0F),
        "combat targeting overrides idle facing and target loss returns a settled unit forward");

    World terminal_b_world;
    terminal_b_world.units().clear();
    for (std::size_t index = 1; index <= 3; ++index) {
        terminal_b_world.zones()[index].advance_capture(-100.0F);
    }
    update_zone_capture(terminal_b_world, 0.0);
    const auto terminal_b_frontline =
        frontline_objective(terminal_b_world, Team::team_b);
    terminal_b_world.units().push_back(
        test_unit(5150, Team::team_b, {384.5F, 300.0F}, 90.0F));
    Simulation terminal_b_simulation{terminal_b_world};
    terminal_b_simulation.update(1.0);
    passed &= check(
        terminal_b_frontline.has_value() &&
            terminal_b_frontline->zone_index == 1 &&
            near(terminal_b_frontline->forward_boundary_x, 384.0F) &&
            terminal_b_world.units()[0].position().x > 384.0F,
        "Team B terminal frontline mirrors the opposing-home restriction");

    terminal_b_world.units().clear();
    terminal_b_world.units().push_back(test_unit(
        5151, Team::team_b, {terminal_b_frontline->hold_x, 300.0F}, 0.0F));
    terminal_b_simulation.update(1.0 / 60.0);
    passed &= check(
        terminal_b_world.units()[0].movement_state() == MovementState::idle &&
            near(terminal_b_world.units()[0].desired_facing_angle(), 90.0F) &&
            near(terminal_b_world.units()[0].facing_angle(), 1.5F),
        "terminal Team B idle facing mirrors gradually toward Team A");

    passed &= check(near(capture_bar_fraction(100.0F), 0.0F) &&
                        near(capture_bar_fraction(0.0F), 0.5F) &&
                        near(capture_bar_fraction(-100.0F), 1.0F),
                    "capture bar maps Team A left, neutral center, Team B right");

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

    World occupation_world;
    occupation_world.units().clear();
    occupation_world.zones()[1].advance_capture(100.0F);
    occupation_world.units().push_back(
        test_unit(1100, Team::team_a, {500.0F, 200.0F}, 0.0F));
    update_zone_capture(occupation_world, 0.0);
    passed &= check(occupation_world.zones()[1].owner() == Team::team_a &&
                        occupation_world.zones()[1].occupied(),
                    "owned objective with owner presence is occupied");

    occupation_world.units().clear();
    update_zone_capture(occupation_world, 0.5);
    passed &= check(occupation_world.zones()[1].owner() == Team::team_a &&
                        !occupation_world.zones()[1].occupied(),
                    "empty owned objective is unoccupied without losing ownership");

    World neutral_security_world;
    neutral_security_world.units().clear();
    update_zone_capture(neutral_security_world, 10.0);
    passed &= check(!neutral_security_world.zones()[2].secured() &&
                        near(static_cast<float>(neutral_security_world.zones()[2]
                                                    .secure_timer_seconds()),
                             0.0F),
                    "neutral objective never becomes secured");

    occupation_world.units().push_back(
        test_unit(1101, Team::team_b, {500.0F, 300.0F}, 0.0F));
    update_zone_capture(occupation_world, 0.0);
    passed &= check(occupation_world.zones()[1].contested() &&
                        !occupation_world.zones()[1].secured() &&
                        near(static_cast<float>(occupation_world.zones()[1]
                                                    .secure_timer_seconds()),
                             0.0F),
                    "enemy presence contests an owned objective and resets securing");

    occupation_world.units().clear();
    update_zone_capture(occupation_world, 1.0);
    passed &= check(!occupation_world.zones()[1].secured() &&
                        near(static_cast<float>(occupation_world.zones()[1]
                                                    .secure_timer_seconds()),
                             1.0F),
                    "objective remains unsecured before two uncontested seconds");
    update_zone_capture(occupation_world, 1.0);
    passed &= check(occupation_world.zones()[1].secured() &&
                        near(static_cast<float>(occupation_world.zones()[1]
                                                    .secure_timer_seconds()),
                             2.0F),
                    "two continuous uncontested seconds secure an objective");

    occupation_world.units().push_back(
        test_unit(1102, Team::team_b, {500.0F, 400.0F}, 0.0F));
    update_zone_capture(occupation_world, 0.0);
    passed &= check(!occupation_world.zones()[1].secured() &&
                        !is_zone_deployable(occupation_world.zones()[1],
                                            Team::team_a),
                    "enemy entry immediately removes security and deployment");
    occupation_world.units().clear();
    update_zone_capture(occupation_world, 1.0);
    passed &= check(!occupation_world.zones()[1].secured(),
                    "clearing an enemy does not restore security early");
    update_zone_capture(occupation_world, 1.0);
    passed &= check(occupation_world.zones()[1].secured(),
                    "cleared objective requires another full two-second timer");

    passed &= check(is_zone_deployable(home_world.zones()[0], Team::team_a) &&
                        is_zone_deployable(home_world.zones()[4], Team::team_b) &&
                        !is_zone_deployable(home_world.zones()[0], Team::team_b) &&
                        home_world.zones()[0].secured() &&
                        home_world.zones()[4].secured(),
                    "home zones are permanently secured and owner-deployable");
    passed &= check(
        is_zone_deployable(occupation_world.zones()[1], Team::team_a) &&
            !is_zone_deployable(occupation_world.zones()[1], Team::team_b),
        "secured objective is deployable only by its owner");

    const auto team_a_deployment =
        deployment_bounds(occupation_world.zones()[1], Team::team_a);
    passed &= check(team_a_deployment.has_value() &&
                        near(team_a_deployment->x, 384.0F) &&
                        near(team_a_deployment->width, 288.0F) &&
                        near(team_a_deployment->height,
                             occupation_world.map().logical_height),
                    "Team A deployment uses the left/rear 75 percent");

    World team_b_deployment_world;
    team_b_deployment_world.units().clear();
    team_b_deployment_world.zones()[3].advance_capture(-100.0F);
    update_zone_capture(team_b_deployment_world, 0.0);
    update_zone_capture(team_b_deployment_world, 2.0);
    const auto team_b_deployment = deployment_bounds(
        team_b_deployment_world.zones()[3], Team::team_b);
    passed &= check(team_b_deployment.has_value() &&
                        near(team_b_deployment->x, 1248.0F) &&
                        near(team_b_deployment->width, 288.0F) &&
                        near(team_b_deployment->height,
                             team_b_deployment_world.map().logical_height),
                    "Team B deployment mirrors into the right/rear 75 percent");

    World contiguous_a_world;
    contiguous_a_world.units().clear();
    const auto home_only_a = deployment_bounds(
        contiguous_a_world, contiguous_a_world.zones()[0], Team::team_a);
    const auto home_only_b = deployment_bounds(
        contiguous_a_world, contiguous_a_world.zones()[4], Team::team_b);
    passed &= check(
        home_only_a.has_value() && near(home_only_a->x, 0.0F) &&
            near(home_only_a->width, 288.0F) && home_only_b.has_value() &&
            near(home_only_b->x, 1632.0F) &&
            near(home_only_b->width, 288.0F),
        "home-only deployment keeps the mirrored rear-75-percent safety restriction");

    contiguous_a_world.zones()[1].advance_capture(100.0F);
    update_zone_capture(contiguous_a_world, 0.0);
    update_zone_capture(contiguous_a_world, 2.0);
    const auto full_a_home = deployment_bounds(
        contiguous_a_world, contiguous_a_world.zones()[0], Team::team_a);
    const auto restricted_a_zone_1 = deployment_bounds(
        contiguous_a_world, contiguous_a_world.zones()[1], Team::team_a);
    passed &= check(
        full_a_home.has_value() && near(full_a_home->width, 384.0F) &&
            restricted_a_zone_1.has_value() &&
            near(restricted_a_zone_1->x, 384.0F) &&
            near(restricted_a_zone_1->width, 288.0F) &&
            is_valid_deployment_location(contiguous_a_world, Team::team_a,
                                         {350.0F, 400.0F}) &&
            !is_valid_deployment_location(contiguous_a_world, Team::team_a,
                                          {700.0F, 400.0F}),
        "one secured Team A objective fills the old home gap and retains its front buffer");

    contiguous_a_world.zones()[2].advance_capture(100.0F);
    update_zone_capture(contiguous_a_world, 0.0);
    update_zone_capture(contiguous_a_world, 2.0);
    const auto full_a_zone_1 = deployment_bounds(
        contiguous_a_world, contiguous_a_world.zones()[1], Team::team_a);
    const auto restricted_a_zone_2 = deployment_bounds(
        contiguous_a_world, contiguous_a_world.zones()[2], Team::team_a);
    passed &= check(
        full_a_zone_1.has_value() && near(full_a_zone_1->width, 384.0F) &&
            restricted_a_zone_2.has_value() &&
            near(restricted_a_zone_2->x, 768.0F) &&
            near(restricted_a_zone_2->width, 288.0F) &&
            is_valid_deployment_location(contiguous_a_world, Team::team_a,
                                         {750.0F, 400.0F}) &&
            !is_valid_deployment_location(contiguous_a_world, Team::team_a,
                                          {1100.0F, 400.0F}),
        "advancing Team A deployment makes each previous connected zone fully deployable");

    contiguous_a_world.zones()[3].advance_capture(100.0F);
    update_zone_capture(contiguous_a_world, 0.0);
    update_zone_capture(contiguous_a_world, 2.0);
    const auto full_a_zone_2 = deployment_bounds(
        contiguous_a_world, contiguous_a_world.zones()[2], Team::team_a);
    const auto restricted_a_zone_3 = deployment_bounds(
        contiguous_a_world, contiguous_a_world.zones()[3], Team::team_a);
    passed &= check(
        full_a_zone_2.has_value() && near(full_a_zone_2->width, 384.0F) &&
            restricted_a_zone_3.has_value() &&
            near(restricted_a_zone_3->x, 1152.0F) &&
            near(restricted_a_zone_3->width, 288.0F),
        "frontmost Team A secured objective alone retains the forward 25-percent buffer");

    World contiguous_b_world;
    contiguous_b_world.units().clear();
    contiguous_b_world.zones()[3].advance_capture(-100.0F);
    update_zone_capture(contiguous_b_world, 0.0);
    update_zone_capture(contiguous_b_world, 2.0);
    const auto full_b_home = deployment_bounds(
        contiguous_b_world, contiguous_b_world.zones()[4], Team::team_b);
    const auto restricted_b_zone_3 = deployment_bounds(
        contiguous_b_world, contiguous_b_world.zones()[3], Team::team_b);
    passed &= check(
        full_b_home.has_value() && near(full_b_home->x, 1536.0F) &&
            near(full_b_home->width, 384.0F) &&
            restricted_b_zone_3.has_value() &&
            near(restricted_b_zone_3->x, 1248.0F) &&
            near(restricted_b_zone_3->width, 288.0F) &&
            is_valid_deployment_location(contiguous_b_world, Team::team_b,
                                         {1570.0F, 400.0F}) &&
            !is_valid_deployment_location(contiguous_b_world, Team::team_b,
                                          {1200.0F, 400.0F}),
        "Team B contiguous deployment and front safety buffer mirror Team A");

    World disconnected_deployment_world;
    disconnected_deployment_world.units().clear();
    disconnected_deployment_world.zones()[2].advance_capture(100.0F);
    update_zone_capture(disconnected_deployment_world, 0.0);
    update_zone_capture(disconnected_deployment_world, 2.0);
    passed &= check(
        !deployment_bounds(disconnected_deployment_world,
                           disconnected_deployment_world.zones()[2],
                           Team::team_a)
             .has_value() &&
            !is_valid_deployment_location(disconnected_deployment_world,
                                          Team::team_a,
                                          {800.0F, 400.0F}),
        "disconnected secured ownership cannot bridge an unsecured objective gap");

    World sudden_deployment_corridor_world{MatchRules{60, 1}};
    sudden_deployment_corridor_world.units().clear();
    (void)sudden_deployment_corridor_world.match_state().advance(60, 0, 0);
    sudden_deployment_corridor_world.reset_for_sudden_death();
    sudden_deployment_corridor_world.zones()[1].advance_capture(100.0F);
    update_zone_capture(sudden_deployment_corridor_world, 0.0);
    update_zone_capture(sudden_deployment_corridor_world, 2.0);
    const auto sudden_home = deployment_bounds(
        sudden_deployment_corridor_world,
        sudden_deployment_corridor_world.zones()[0], Team::team_a);
    passed &= check(
        sudden_home.has_value() && near(sudden_home->width, 288.0F) &&
            !deployment_bounds(sudden_deployment_corridor_world,
                               sudden_deployment_corridor_world.zones()[1],
                               Team::team_a)
                 .has_value() &&
            !is_valid_deployment_location(sudden_deployment_corridor_world,
                                          Team::team_a,
                                          {500.0F, 400.0F}),
        "sudden death remains home-only with its home safety buffer");

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

    const auto simulated_kill_reward = [](const std::size_t victim_index,
                                          const float damage) {
        World reward_world;
        isolate_collision_units(reward_world);
        reward_world.units()[victim_index].set_position({200.0F, 100.0F});
        const Money difference_before =
            reward_world.find_player(Team::team_a)->cash() -
            reward_world.find_player(Team::team_b)->cash();
        reward_world.spawn_projectile(
            WeaponType::rifle, Team::team_a, reward_world.units()[0].id(),
            {100.0F, 100.0F}, {12000.0F, 0.0F}, 1000.0F, damage);
        Simulation reward_simulation{reward_world};
        reward_simulation.update(1.0 / 60.0);
        return reward_world.find_player(Team::team_a)->cash() -
               reward_world.find_player(Team::team_b)->cash() -
               difference_before;
    };
    passed &= check(simulated_kill_reward(4, 1'000.0F) == 250,
                    "killing a rifle awards 250 to the killer team");
    passed &= check(simulated_kill_reward(6, 1'000.0F) == 400,
                    "killing a machine_gun awards 400 to the killer team");
    passed &= check(simulated_kill_reward(10, 1'000.0F) == 600,
                    "killing a bazooka awards 600 to the killer team");
    passed &= check(simulated_kill_reward(4, 25.0F) == 0,
                    "nonlethal projectile damage gives no kill reward");

    World single_reward_world;
    isolate_collision_units(single_reward_world);
    single_reward_world.units()[4].set_position({200.0F, 100.0F});
    single_reward_world.spawn_projectile(
        WeaponType::rifle, Team::team_a, single_reward_world.units()[0].id(),
        {100.0F, 100.0F}, {12000.0F, 0.0F}, 1000.0F, 1'000.0F);
    Simulation single_reward_simulation{single_reward_world};
    single_reward_simulation.update(1.0 / 60.0);
    const Money difference_after_lethal =
        single_reward_world.find_player(Team::team_a)->cash() -
        single_reward_world.find_player(Team::team_b)->cash();
    single_reward_simulation.update(1.0 / 60.0);
    passed &= check(
        single_reward_world.find_player(Team::team_a)->cash() -
                single_reward_world.find_player(Team::team_b)->cash() ==
            difference_after_lethal,
        "a lethal hit rewards exactly once after the victim is removed");

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
