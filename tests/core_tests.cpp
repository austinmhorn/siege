#include "core/combat_behavior.hpp"
#include "client/capture_bar.hpp"
#include "client/pointer_input.hpp"
#include "client/unit_selection.hpp"
#include "core/deployment.hpp"
#include "core/economy.hpp"
#include "core/frontline.hpp"
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

} // namespace

int main() {
    using namespace siege;

    bool passed = true;

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
    selection.replace_from_rectangle(selection_world, {200.0F, 200.0F},
                                     {50.0F, 50.0F});
    passed &= check(
        selection.ids() == std::vector<Unit::Id>{101, 102},
        "reversed selection includes living Team A units and inclusive edges only");
    passed &= check(!selection.contains(103) && !selection.contains(104) &&
                        !selection.contains(105),
                    "selection ignores outside, enemy, and dead units");

    UnitSelection rmb_selection;
    PointerInputRouter rmb_input;
    const PointerDispatch rmb_press =
        rmb_input.press(PointerButton::secondary, false);
    const PointerDispatch rmb_release =
        rmb_input.release(PointerButton::secondary);
    if (rmb_press == PointerDispatch::secondary &&
        rmb_release == PointerDispatch::secondary) {
        rmb_selection.replace_from_rectangle(selection_world, {50.0F, 50.0F},
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
            selection_world, {200.0F, 200.0F}, {50.0F, 50.0F});
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

    selection.replace_from_rectangle(selection_world, {300.0F, 300.0F},
                                     {400.0F, 400.0F});
    passed &= check(selection.ids() == std::vector<Unit::Id>{103},
                    "a new rectangle replaces the previous selection");
    selection_world.units()[2].apply_damage(100.0F);
    selection.prune(selection_world);
    passed &= check(selection.ids().empty(),
                    "dead selected unit IDs are pruned safely");

    selection.replace_from_rectangle(selection_world, {90.0F, 90.0F},
                                     {110.0F, 110.0F});
    selection_world.units().erase(selection_world.units().begin());
    selection.prune(selection_world);
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
        default_economy_rules.passive_income_per_second == 100 &&
            kill_reward_for(TroopType::rifle) == 250 &&
            kill_reward_for(TroopType::machine_gun) == 400 &&
            kill_reward_for(TroopType::bazooka) == 600 &&
            default_economy_rules.objective_capture_reward == 1'000,
        "passive income and all troop/capture rewards are centralized");

    Simulation economy_simulation{economy_world};
    for (int tick = 0; tick < 60; ++tick) {
        economy_simulation.update(1.0 / 60.0);
    }
    passed &= check(economy_world.find_player(Team::team_a)->cash() == 25'100 &&
                        economy_world.find_player(Team::team_b)->cash() == 25'100,
                    "one fixed-step second awards 100 passive cash equally");

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
            render_rate_a.find_player(Team::team_a)->cash() == 25'200 &&
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
            sudden_reset_world.find_player(Team::team_a)->cash() == 25'100 &&
            sudden_reset_world.find_player(Team::team_b)->cash() == 25'100 &&
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
    bool regroup_completed = false;
    for (int tick = 0; tick < 240; ++tick) {
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
        regroup_completed &&
            length(regroup_world.units()[0].position() -
                   regroup_world.units()[1].position()) <
                initial_regroup_separation,
        "Regroup consolidates dispersed units and completes back to auto");

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
                        near(team_a_deployment->height, World::height),
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
                        near(team_b_deployment->height, World::height),
                    "Team B deployment mirrors into the right/rear 75 percent");

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
