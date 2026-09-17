#include "core/ai_commander.hpp"

#include "core/deployment.hpp"
#include "core/frontline.hpp"
#include "core/map_definition.hpp"
#include "core/perception.hpp"
#include "core/tactical_command.hpp"
#include "core/troop_definition.hpp"
#include "core/zone_capture.hpp"
#include "world/player_state.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>

namespace siege {
namespace {

std::optional<Bounds> frontmost_deployment_bounds(
    const World& world, const Team team) noexcept {
    const TeamForwardDefinition* forward =
        team_forward_definition(world.map(), team);
    if (forward == nullptr || forward->home_zone_index >= world.zones().size()) {
        return std::nullopt;
    }

    std::optional<Bounds> front = deployment_bounds(
        world, world.zones()[forward->home_zone_index], team);
    for (const std::size_t zone_index : forward->objective_order) {
        if (zone_index >= world.zones().size()) {
            break;
        }
        const auto candidate =
            deployment_bounds(world, world.zones()[zone_index], team);
        if (!candidate.has_value()) {
            break;
        }
        front = candidate;
    }
    return front;
}

int enemy_count(const Zone& zone, const Team team) noexcept {
    return team == Team::team_a ? zone.team_b_count()
                                : team == Team::team_b
                                      ? zone.team_a_count()
                                      : 0;
}

float squared_distance_to_bounds(const Vec2 position,
                                 const Bounds& bounds) noexcept {
    const float nearest_x = std::clamp(
        position.x, bounds.x, bounds.x + bounds.width);
    const float nearest_y = std::clamp(
        position.y, bounds.y, bounds.y + bounds.height);
    return length_squared(position - Vec2{nearest_x, nearest_y});
}

struct ForceAssessment {
    std::vector<Unit::Id> unit_ids;
    int friendly_strength{};
    int enemy_strength{};
    float maximum_scatter_distance{};
};

ForceAssessment assess_force(const World& world, const Team team,
                             const std::size_t objective_index,
                             const AiCommanderRules& rules) {
    ForceAssessment assessment;
    if (objective_index >= world.zones().size()) {
        return assessment;
    }
    const Bounds& bounds = world.zones()[objective_index].bounds();
    const float margin_squared =
        rules.force_selection_margin * rules.force_selection_margin;
    std::vector<std::pair<float, Unit::Id>> friendly_candidates;

    for (const Unit& unit : world.units()) {
        if (!unit.is_alive()) {
            continue;
        }
        const float distance_squared =
            squared_distance_to_bounds(unit.position(), bounds);
        if (unit.team() == team &&
            unit.mobility_mode() != MobilityMode::player_path_only) {
            friendly_candidates.emplace_back(distance_squared, unit.id());
            if (distance_squared <= margin_squared) {
                assessment.unit_ids.push_back(unit.id());
            }
        } else if (unit.team() != Team::none &&
                   distance_squared <= margin_squared) {
            ++assessment.enemy_strength;
        }
    }

    if (assessment.unit_ids.empty()) {
        std::ranges::sort(friendly_candidates);
        const std::size_t fallback_count = std::min(
            rules.fallback_force_limit, friendly_candidates.size());
        for (std::size_t index = 0; index < fallback_count; ++index) {
            assessment.unit_ids.push_back(friendly_candidates[index].second);
        }
    }
    std::ranges::sort(assessment.unit_ids);
    if (rules.maximum_local_force > 0 &&
        assessment.unit_ids.size() > rules.maximum_local_force) {
        assessment.unit_ids.resize(rules.maximum_local_force);
    }

    assessment.friendly_strength =
        static_cast<int>(assessment.unit_ids.size());
    for (const PendingDeployment& deployment : world.pending_deployments()) {
        if (deployment.team == team &&
            squared_distance_to_bounds(deployment.position, bounds) <=
                margin_squared) {
            ++assessment.friendly_strength;
        }
    }

    if (assessment.unit_ids.size() < 2) {
        return assessment;
    }
    Vec2 center{};
    for (const Unit::Id id : assessment.unit_ids) {
        const Unit* unit = world.find_unit(id);
        center = center + unit->position();
    }
    center = center * (1.0F / static_cast<float>(assessment.unit_ids.size()));
    for (const Unit::Id id : assessment.unit_ids) {
        const Unit* unit = world.find_unit(id);
        assessment.maximum_scatter_distance = std::max(
            assessment.maximum_scatter_distance,
            length(unit->position() - center));
    }
    return assessment;
}

std::optional<std::size_t> threatened_owned_objective(
    const World& world, const Team team,
    const int enemy_threshold) noexcept {
    std::optional<std::size_t> threatened;
    int greatest_enemy_presence = 0;
    for (const std::size_t index : world.map().objective_zone_indices) {
        if (index >= world.zones().size()) {
            continue;
        }
        const Zone& zone = world.zones()[index];
        const int enemies = enemy_count(zone, team);
        if (zone.owner() == team && enemies >= enemy_threshold &&
            enemies > greatest_enemy_presence) {
            threatened = index;
            greatest_enemy_presence = enemies;
        }
    }
    return threatened;
}

std::size_t troop_index(const TroopType troop) noexcept {
    switch (troop) {
    case TroopType::rifle:
        return 0;
    case TroopType::machine_gun:
        return 1;
    case TroopType::bazooka:
        return 2;
    case TroopType::medium_tank:
        return 3;
    case TroopType::anti_tank:
        return 4;
    case TroopType::mortar:
        return 5;
    }
    return 0;
}

struct PurchasePlan {
    TroopType troop{TroopType::rifle};
    AiPurchasePlanReason reason{AiPurchasePlanReason::composition};
};

std::array<int, 6> friendly_composition(const World& world,
                                        const Team team) noexcept {
    std::array<int, 6> current{};
    for (const Unit& unit : world.units()) {
        if (unit.is_alive() && unit.team() == team) {
            ++current[troop_index(unit.troop_type())];
        }
    }
    for (const PendingDeployment& pending : world.pending_deployments()) {
        if (pending.team == team) {
            ++current[troop_index(pending.troop_type)];
        }
    }
    return current;
}

struct VisibleEnemyComposition {
    int infantry{};
    int vehicles{};
};

VisibleEnemyComposition visible_enemy_composition(const World& world,
                                                   const Team team) noexcept {
    VisibleEnemyComposition result;
    for (const Unit& enemy : world.units()) {
        if (!enemy.is_alive() || enemy.team() == team ||
            enemy.team() == Team::none) {
            continue;
        }
        const bool perceived = std::ranges::any_of(
            world.units(), [&world, team, &enemy](const Unit& observer) {
                return observer.is_alive() && observer.team() == team &&
                    can_perceive(world.map(), observer, enemy);
            });
        if (!perceived) {
            continue;
        }
        if (enemy.target_category() == TargetCategory::vehicle) {
            ++result.vehicles;
        } else {
            ++result.infantry;
        }
    }
    return result;
}

PurchasePlan composition_purchase(const World& world, const Team team,
                                  const AiProfile& profile,
                                  const AiStrategy strategy,
                                  const std::optional<TroopType>
                                      last_purchased_troop) noexcept {
    const std::array<int, 6> current = friendly_composition(world, team);
    std::array<int, 6> desired{};
    for (const TroopType troop : profile.rules.troop_mix) {
        ++desired[troop_index(troop)];
    }
    for (int& weight : desired) {
        weight = std::max(1, weight);
    }

    const VisibleEnemyComposition visible =
        visible_enemy_composition(world, team);
    const bool sustained_fighting = std::ranges::any_of(
        world.units(), [team](const Unit& unit) {
            return unit.is_alive() && unit.team() == team &&
                unit.target_id().has_value();
        }) || visible.infantry + visible.vehicles >= 2;
    int total_force = 0;
    for (const int count : current) {
        total_force += count;
    }
    const int ordinary_infantry = current[troop_index(TroopType::rifle)] +
        current[troop_index(TroopType::machine_gun)] +
        current[troop_index(TroopType::bazooka)];

    constexpr std::array candidates{
        TroopType::rifle, TroopType::machine_gun, TroopType::bazooka,
        TroopType::medium_tank, TroopType::anti_tank, TroopType::mortar};
    std::array<int, candidates.size()> scores{};
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const std::size_t type_index = troop_index(candidates[index]);
        scores[index] = desired[type_index] * 100 /
            (current[type_index] + 1);
    }

    scores[troop_index(TroopType::rifle)] +=
        ordinary_infantry * 2 < std::max(2, total_force) ? 90 : 0;
    scores[troop_index(TroopType::machine_gun)] += visible.infantry * 14;
    scores[troop_index(TroopType::bazooka)] +=
        (visible.infantry + visible.vehicles) * 7;
    scores[troop_index(TroopType::medium_tank)] +=
        strategy == AiStrategy::attack ? 35 : 5;
    scores[troop_index(TroopType::anti_tank)] +=
        visible.vehicles * 170 -
        current[troop_index(TroopType::anti_tank)] * 90;
    scores[troop_index(TroopType::mortar)] +=
        sustained_fighting ? 55 : -15;

    switch (profile.playstyle) {
    case AiPlaystyle::balanced:
        break;
    case AiPlaystyle::aggressive:
        scores[troop_index(TroopType::medium_tank)] += 65;
        scores[troop_index(TroopType::rifle)] += 20;
        scores[troop_index(TroopType::mortar)] -= 40;
        break;
    case AiPlaystyle::defensive:
        scores[troop_index(TroopType::machine_gun)] += 35;
        scores[troop_index(TroopType::mortar)] += 70;
        scores[troop_index(TroopType::medium_tank)] -= 10;
        break;
    }
    switch (profile.difficulty) {
    case AiDifficulty::easy:
        scores[troop_index(TroopType::medium_tank)] -= 15;
        scores[troop_index(TroopType::mortar)] -= 10;
        break;
    case AiDifficulty::medium:
        break;
    case AiDifficulty::hard:
        scores[troop_index(TroopType::medium_tank)] += 15;
        scores[troop_index(TroopType::mortar)] += 15;
        scores[troop_index(TroopType::anti_tank)] += visible.vehicles * 30;
        break;
    }
    if (last_purchased_troop == TroopType::medium_tank) {
        scores[troop_index(TroopType::medium_tank)] -= 180;
    }
    if (last_purchased_troop == TroopType::mortar) {
        scores[troop_index(TroopType::mortar)] -= 220;
    }
    scores[troop_index(TroopType::medium_tank)] -=
        current[troop_index(TroopType::medium_tank)] * 20;
    scores[troop_index(TroopType::mortar)] -=
        current[troop_index(TroopType::mortar)] * 45;

    std::size_t best = 0;
    for (std::size_t index = 1; index < candidates.size(); ++index) {
        if (scores[index] > scores[best]) {
            best = index;
        }
    }
    const TroopType selected = candidates[best];
    const AiPurchasePlanReason reason = selected == TroopType::medium_tank
        ? AiPurchasePlanReason::frontline_anchor
        : selected == TroopType::mortar
            ? AiPurchasePlanReason::artillery_support
            : selected == TroopType::anti_tank && visible.vehicles > 0
                ? AiPurchasePlanReason::vehicle_counter
                : AiPurchasePlanReason::composition;
    return {selected, reason};
}

std::size_t current_or_pending_count(const World& world, const Team team,
                                     const TroopType troop_type) noexcept {
    return static_cast<std::size_t>(std::ranges::count_if(
               world.units(), [team, troop_type](const Unit& unit) {
                   return unit.is_alive() && unit.team() == team &&
                       unit.troop_type() == troop_type;
               })) +
        static_cast<std::size_t>(std::ranges::count_if(
            world.pending_deployments(),
            [team, troop_type](const PendingDeployment& pending) {
                return pending.team == team && pending.troop_type == troop_type;
            }));
}

bool immediate_defensive_emergency(const World& world,
                                   const Team team) noexcept {
    const TeamForwardDefinition* forward =
        team_forward_definition(world.map(), team);
    const bool home_threatened = forward != nullptr &&
        std::ranges::any_of(world.units(), [&world, team, forward](
                                                  const Unit& unit) {
            return unit.is_alive() && unit.team() != Team::none &&
                unit.team() != team &&
                map_zone_index_for_position(world.map(), unit.position()) ==
                    forward->home_zone_index;
        });
    std::size_t combat_force = 0;
    for (const Unit& unit : world.units()) {
        if (unit.is_alive() && unit.team() == team) {
            ++combat_force;
        }
    }
    combat_force += static_cast<std::size_t>(std::ranges::count_if(
        world.pending_deployments(), [team](const PendingDeployment& pending) {
            return pending.team == team;
        }));
    return home_threatened || combat_force <= 1;
}

std::optional<Bounds> deployment_bounds_for_troop(
    const World& world, const Team team, const TroopType troop) noexcept {
    const TeamForwardDefinition* forward =
        team_forward_definition(world.map(), team);
    if (forward == nullptr || forward->home_zone_index >= world.zones().size()) {
        return std::nullopt;
    }
    if (troop == TroopType::mortar) {
        return deployment_bounds(
            world, world.zones()[forward->home_zone_index], team);
    }
    return frontmost_deployment_bounds(world, team);
}

} // namespace

AiProfile make_ai_profile(const AiDifficulty difficulty,
                          const AiPlaystyle playstyle) noexcept {
    AiProfile profile{difficulty, playstyle, default_ai_commander_rules};
    AiCommanderRules& rules = profile.rules;
    switch (difficulty) {
    case AiDifficulty::easy:
        rules.decision_interval_seconds = 4.0;
        rules.strategy_interval_seconds = 3.0;
        rules.deployment_y_fractions = {0.20F, 0.80F, 0.50F,
                                        0.35F, 0.65F, 0.50F, 0.30F};
        rules.force_selection_margin = 170.0F;
        rules.fallback_force_limit = 4;
        rules.maximum_local_force = 4;
        rules.regroup_outnumber_ratio = 1.75F;
        rules.regroup_minimum_enemy_advantage = 3;
        rules.regroup_scatter_distance = 320.0F;
        rules.regroup_severe_scatter_distance = 500.0F;
        break;
    case AiDifficulty::medium:
        break;
    case AiDifficulty::hard:
        rules.decision_interval_seconds = 1.0;
        rules.strategy_interval_seconds = 0.5;
        rules.deployment_y_fractions = {0.50F, 0.35F, 0.65F,
                                        0.25F, 0.75F, 0.50F, 0.20F};
        rules.force_selection_margin = 300.0F;
        rules.fallback_force_limit = 8;
        rules.regroup_outnumber_ratio = 1.35F;
        rules.regroup_minimum_enemy_advantage = 1;
        rules.regroup_scatter_distance = 220.0F;
        rules.regroup_severe_scatter_distance = 360.0F;
        break;
    }

    switch (playstyle) {
    case AiPlaystyle::balanced:
        break;
    case AiPlaystyle::aggressive:
        rules.troop_mix = {TroopType::rifle, TroopType::machine_gun,
                           TroopType::rifle, TroopType::medium_tank,
                           TroopType::machine_gun, TroopType::anti_tank,
                           TroopType::mortar};
        rules.forward_position_fraction = 0.90F;
        rules.defense_enemy_threshold = 2;
        rules.regroup_outnumber_ratio += 0.35F;
        rules.regroup_minimum_enemy_advantage =
            std::max(2, rules.regroup_minimum_enemy_advantage);
        rules.regroup_scatter_distance += 40.0F;
        rules.regroup_severe_scatter_distance += 80.0F;
        break;
    case AiPlaystyle::defensive:
        rules.troop_mix = {TroopType::rifle, TroopType::machine_gun,
                           TroopType::machine_gun, TroopType::bazooka,
                           TroopType::medium_tank, TroopType::anti_tank,
                           TroopType::mortar};
        rules.forward_position_fraction = 0.65F;
        rules.force_selection_margin += 80.0F;
        rules.fallback_force_limit += 2;
        rules.strategy_interval_seconds *= 0.75;
        rules.defense_release_evaluations = 2;
        rules.regroup_outnumber_ratio =
            std::max(1.0F, rules.regroup_outnumber_ratio - 0.20F);
        rules.regroup_minimum_enemy_advantage = 1;
        rules.regroup_scatter_distance =
            std::max(160.0F, rules.regroup_scatter_distance - 40.0F);
        rules.regroup_severe_scatter_distance = std::max(
            rules.regroup_scatter_distance,
            rules.regroup_severe_scatter_distance - 80.0F);
        break;
    }
    return profile;
}

AiCommander::AiCommander(const Team team, const AiProfile profile) noexcept
    : team_{team}, profile_{profile}, rules_{profile.rules} {
    rules_.decision_interval_seconds =
        std::max(1.0 / 60.0, rules_.decision_interval_seconds);
    rules_.strategy_interval_seconds =
        std::max(1.0 / 60.0, rules_.strategy_interval_seconds);
    rules_.forward_position_fraction =
        std::clamp(rules_.forward_position_fraction, 0.0F, 1.0F);
    for (float& fraction : rules_.deployment_y_fractions) {
        fraction = std::clamp(fraction, 0.0F, 1.0F);
    }
    rules_.force_selection_margin =
        std::max(0.0F, rules_.force_selection_margin);
    rules_.fallback_force_limit =
        std::max<std::size_t>(1, rules_.fallback_force_limit);
    rules_.defense_enemy_threshold =
        std::max(1, rules_.defense_enemy_threshold);
    rules_.regroup_outnumber_ratio =
        std::max(1.0F, rules_.regroup_outnumber_ratio);
    rules_.regroup_minimum_enemy_advantage =
        std::max(1, rules_.regroup_minimum_enemy_advantage);
    rules_.regroup_scatter_distance =
        std::max(0.0F, rules_.regroup_scatter_distance);
    rules_.regroup_severe_scatter_distance = std::max(
        rules_.regroup_scatter_distance,
        rules_.regroup_severe_scatter_distance);
}

void AiCommander::update(World& world, const Team locally_controlled_team,
                         std::uint64_t fixed_tick_count) {
    if (!world.match_state().active()) {
        status_ = AiCommanderStatus::stopped_match_finished;
        return;
    }
    if (locally_controlled_team == team_) {
        status_ = AiCommanderStatus::paused_local_control;
        return;
    }
    status_ = AiCommanderStatus::enabled;
    if (fixed_tick_count == 0 || team_ == Team::none) {
        return;
    }

    const auto interval_ticks = [&world](const double seconds) {
        return std::max<std::uint64_t>(
            1, static_cast<std::uint64_t>(std::llround(
                   seconds * world.match_state().rules().fixed_ticks_per_second)));
    };
    const std::uint64_t purchase_interval_ticks =
        interval_ticks(rules_.decision_interval_seconds);
    const std::uint64_t strategy_interval_ticks =
        interval_ticks(rules_.strategy_interval_seconds);
    const auto advance_clock = [fixed_tick_count](
                                   std::uint64_t& remaining,
                                   const std::uint64_t interval,
                                   auto&& decision) {
        std::uint64_t ticks = fixed_tick_count;
        if (remaining == 0) {
            decision();
            remaining = interval;
        }
        while (ticks >= remaining) {
            ticks -= remaining;
            decision();
            remaining = interval;
        }
        remaining -= ticks;
    };
    advance_clock(ticks_until_next_decision_, purchase_interval_ticks,
                  [this, &world] { make_purchase_decision(world); });
    advance_clock(ticks_until_next_strategy_evaluation_,
                  strategy_interval_ticks,
                  [this, &world] { make_strategy_decision(world); });
}

void AiCommander::make_purchase_decision(World& world) {
    last_troop_choice_.reset();
    last_deployment_position_.reset();
    emergency_override_active_ = false;
    PlayerState* player = world.find_player(team_);
    if (player == nullptr) {
        last_result_ = AiDecisionResult::rejected;
        return;
    }

    const VisibleEnemyComposition visible =
        visible_enemy_composition(world, team_);
    const std::size_t anti_tank_count = current_or_pending_count(
        world, team_, TroopType::anti_tank);
    const bool needs_vehicle_counter =
        static_cast<std::size_t>(visible.vehicles) > anti_tank_count;
    if (planned_purchase_reason_ == AiPurchasePlanReason::vehicle_counter &&
        !needs_vehicle_counter) {
        planned_purchase_.reset();
        planned_purchase_reason_.reset();
        emergency_spent_for_plan_ = false;
    }
    if (needs_vehicle_counter &&
        planned_purchase_ != TroopType::anti_tank) {
        planned_purchase_ = TroopType::anti_tank;
        planned_purchase_reason_ = AiPurchasePlanReason::vehicle_counter;
        deployment_failure_evaluations_ = 0;
        emergency_spent_for_plan_ = false;
    } else if (!planned_purchase_.has_value()) {
        const PurchasePlan plan = composition_purchase(
            world, team_, profile_, strategy_, last_purchased_troop_);
        planned_purchase_ = plan.troop;
        planned_purchase_reason_ = plan.reason;
        deployment_failure_evaluations_ = 0;
        emergency_spent_for_plan_ = false;
    }

    const TroopDefinition* planned_definition =
        planned_purchase_.has_value()
        ? troop_definition_for(*planned_purchase_)
        : nullptr;
    if (planned_definition == nullptr) {
        planned_purchase_.reset();
        planned_purchase_reason_.reset();
        last_result_ = AiDecisionResult::rejected;
        return;
    }

    TroopType troop_type = *planned_purchase_;
    bool purchasing_plan = true;
    if (!player->can_afford(planned_definition->purchase_cost)) {
        const TroopDefinition* fallback =
            troop_definition_for(TroopType::rifle);
        if (!emergency_spent_for_plan_ &&
            immediate_defensive_emergency(world, team_) &&
            fallback != nullptr &&
            player->can_afford(fallback->purchase_cost)) {
            troop_type = TroopType::rifle;
            purchasing_plan = false;
            emergency_override_active_ = true;
        } else {
            last_result_ = AiDecisionResult::no_affordable_troop;
            return;
        }
    }

    last_troop_choice_ = troop_type;
    const auto bounds = deployment_bounds_for_troop(world, team_, troop_type);
    const TeamForwardDefinition* forward =
        team_forward_definition(world.map(), team_);
    if (!bounds.has_value() || forward == nullptr) {
        last_result_ = AiDecisionResult::no_valid_deployment;
        if (++deployment_failure_evaluations_ >= 3) {
            planned_purchase_.reset();
            planned_purchase_reason_.reset();
            emergency_spent_for_plan_ = false;
            deployment_failure_evaluations_ = 0;
        }
        return;
    }

    const float placement_fraction = troop_type == TroopType::mortar
        ? 0.35F
        : troop_type == TroopType::anti_tank
            ? 0.55F
            : troop_type == TroopType::medium_tank
                ? std::max(0.72F, rules_.forward_position_fraction)
                : rules_.forward_position_fraction;
    const float forward_fraction = forward->x_direction > 0.0F
        ? placement_fraction
        : 1.0F - placement_fraction;
    std::optional<Vec2> position;
    std::size_t selected_placement_offset = 0;
    for (std::size_t offset = 0;
         offset < rules_.deployment_y_fractions.size(); ++offset) {
        const float y_fraction = rules_.deployment_y_fractions[
            (placement_cursor_ + offset) %
            rules_.deployment_y_fractions.size()];
        const Vec2 candidate{
            bounds->x + bounds->width * forward_fraction,
            bounds->y + bounds->height * y_fraction,
        };
        if (is_valid_deployment_location(world, team_, troop_type,
                                         candidate)) {
            position = candidate;
            selected_placement_offset = offset;
            break;
        }
    }
    if (!position.has_value()) {
        last_result_ = AiDecisionResult::no_valid_deployment;
        if (++deployment_failure_evaluations_ >= 3) {
            planned_purchase_.reset();
            planned_purchase_reason_.reset();
            emergency_spent_for_plan_ = false;
            deployment_failure_evaluations_ = 0;
        }
        return;
    }
    const DeploymentResult result =
        request_deployment(world, team_, troop_type, *position);
    if (result != DeploymentResult::accepted) {
        last_result_ = AiDecisionResult::rejected;
        return;
    }

    last_result_ = AiDecisionResult::purchased;
    last_deployment_position_ = *position;
    deployment_failure_evaluations_ = 0;
    if (purchasing_plan) {
        last_purchased_troop_ = troop_type;
        planned_purchase_.reset();
        planned_purchase_reason_.reset();
        emergency_spent_for_plan_ = false;
    } else {
        emergency_spent_for_plan_ = true;
    }
    placement_cursor_ += selected_placement_offset + 1;
    ++successful_deployments_;
}

void AiCommander::make_strategy_decision(World& world) {
    ++strategy_evaluation_count_;
    auto threat = threatened_owned_objective(
        world, team_, rules_.defense_enemy_threshold);
    const auto frontline = frontline_objective(world, team_);
    if (threat.has_value()) {
        defense_clear_evaluations_ = 0;
    } else if (strategy_ == AiStrategy::defend &&
               target_objective_.has_value() &&
               *target_objective_ < world.zones().size() &&
               world.zones()[*target_objective_].owner() == team_ &&
               defense_clear_evaluations_ <
                   rules_.defense_release_evaluations) {
        threat = target_objective_;
        ++defense_clear_evaluations_;
    } else {
        defense_clear_evaluations_ = 0;
    }
    if (!threat.has_value() && !frontline.has_value()) {
        target_objective_.reset();
        relevant_friendly_strength_ = 0;
        relevant_enemy_strength_ = 0;
        issue_tactical_command(world, TacticalOrder::automatic, 0, {});
        return;
    }

    if (threat.has_value()) {
        strategy_ = AiStrategy::defend;
        target_objective_ = *threat;
        ForceAssessment force =
            assess_force(world, team_, *threat, rules_);
        relevant_friendly_strength_ = force.friendly_strength;
        relevant_enemy_strength_ = force.enemy_strength;
        issue_tactical_command(world, TacticalOrder::hold, *threat,
                               std::move(force.unit_ids));
        return;
    }

    const std::size_t operational_objective = frontline->zone_index;
    target_objective_ =
        world.match_state().phase() == MatchPhase::sudden_death
            ? std::optional<std::size_t>{
                  world.map().center_objective_zone_index}
            : std::optional<std::size_t>{operational_objective};
    ForceAssessment force =
        assess_force(world, team_, operational_objective, rules_);
    relevant_friendly_strength_ = force.friendly_strength;
    relevant_enemy_strength_ = force.enemy_strength;

    const bool regroup_in_progress =
        strategy_ == AiStrategy::regroup &&
        std::ranges::any_of(last_commanded_unit_ids_, [this, &world](
                                                         const Unit::Id id) {
            const Unit* unit = world.find_unit(id);
            return unit != nullptr && unit->is_alive() &&
                   unit->team() == team_ &&
                   unit->tactical_order() == TacticalOrder::regroup;
        });
    const bool regroup_completed =
        strategy_ == AiStrategy::regroup &&
        last_tactical_command_ == TacticalOrder::regroup &&
        !last_commanded_unit_ids_.empty() && !regroup_in_progress;
    const int proportional_threshold = static_cast<int>(std::ceil(
        static_cast<float>(std::max(1, force.friendly_strength)) *
        rules_.regroup_outnumber_ratio));
    const bool substantially_outnumbered =
        force.enemy_strength >= proportional_threshold &&
        force.enemy_strength - force.friendly_strength >=
            rules_.regroup_minimum_enemy_advantage;
    const bool scattered = force.maximum_scatter_distance >=
                           rules_.regroup_scatter_distance;
    const bool severely_scattered = force.maximum_scatter_distance >=
                                    rules_.regroup_severe_scatter_distance;
    const bool should_regroup =
        force.unit_ids.size() >= 2 && scattered &&
        (substantially_outnumbered || severely_scattered);

    if (regroup_in_progress) {
        strategy_ = AiStrategy::regroup;
        return;
    }
    if (!regroup_completed && should_regroup) {
        strategy_ = AiStrategy::regroup;
        issue_tactical_command(world, TacticalOrder::regroup,
                               *target_objective_,
                               std::move(force.unit_ids));
        return;
    }

    strategy_ = AiStrategy::attack;
    issue_tactical_command(world, TacticalOrder::advance,
                           *target_objective_, std::move(force.unit_ids));
}

void AiCommander::issue_tactical_command(
    World& world, const TacticalOrder order,
    const std::size_t objective_index, std::vector<Unit::Id> unit_ids) {
    std::ranges::sort(unit_ids);
    unit_ids.erase(std::ranges::unique(unit_ids).begin(), unit_ids.end());

    if (unit_ids.empty()) {
        const std::size_t released = apply_tactical_order(
            world, last_commanded_unit_ids_, TacticalOrder::automatic, team_);
        if (released > 0) {
            last_tactical_command_ = TacticalOrder::automatic;
            last_commanded_unit_count_ = released;
            ++tactical_command_issue_count_;
        } else {
            last_tactical_command_.reset();
            last_commanded_unit_count_ = 0;
        }
        last_commanded_unit_ids_.clear();
        last_command_objective_.reset();
        return;
    }

    const bool same_signature = last_tactical_command_ == order &&
                                last_command_objective_ == objective_index &&
                                last_commanded_unit_ids_ == unit_ids;
    const bool order_still_applied =
        std::ranges::all_of(unit_ids, [this, &world, order](const Unit::Id id) {
            const Unit* unit = world.find_unit(id);
            return unit != nullptr && unit->is_alive() &&
                   unit->team() == team_ && unit->tactical_order() == order;
        });
    if (same_signature && order_still_applied) {
        return;
    }

    std::vector<Unit::Id> stale_ids;
    for (const Unit::Id id : last_commanded_unit_ids_) {
        if (!std::ranges::binary_search(unit_ids, id)) {
            stale_ids.push_back(id);
        }
    }
    (void)apply_tactical_order(world, stale_ids, TacticalOrder::automatic,
                               team_);

    const std::optional<Bounds> hold_bounds =
        order == TacticalOrder::hold && objective_index < world.zones().size()
            ? std::optional<Bounds>{world.zones()[objective_index].bounds()}
            : std::nullopt;
    const std::size_t commanded = apply_tactical_order(
        world, unit_ids, order, team_, hold_bounds);
    if (commanded == 0) {
        return;
    }
    last_tactical_command_ = order;
    last_commanded_unit_count_ = commanded;
    last_command_objective_ = objective_index;
    last_commanded_unit_ids_ = std::move(unit_ids);
    ++tactical_command_issue_count_;
}

Team AiCommander::team() const noexcept { return team_; }

AiCommanderStatus AiCommander::status() const noexcept { return status_; }

AiDecisionResult AiCommander::last_result() const noexcept {
    return last_result_;
}

std::optional<TroopType> AiCommander::last_troop_choice() const noexcept {
    return last_troop_choice_;
}

std::optional<TroopType> AiCommander::planned_purchase() const noexcept {
    return planned_purchase_;
}

Money AiCommander::planned_purchase_cost() const noexcept {
    const TroopDefinition* definition = planned_purchase_.has_value()
        ? troop_definition_for(*planned_purchase_)
        : nullptr;
    return definition == nullptr ? 0 : definition->purchase_cost;
}

std::optional<AiPurchasePlanReason> AiCommander::planned_purchase_reason()
    const noexcept {
    return planned_purchase_reason_;
}

bool AiCommander::emergency_override_active() const noexcept {
    return emergency_override_active_;
}

std::optional<Vec2> AiCommander::last_deployment_position() const noexcept {
    return last_deployment_position_;
}

std::uint64_t AiCommander::ticks_until_next_decision() const noexcept {
    return ticks_until_next_decision_;
}

std::uint64_t AiCommander::successful_deployments() const noexcept {
    return successful_deployments_;
}

AiStrategy AiCommander::strategy() const noexcept { return strategy_; }

std::optional<std::size_t> AiCommander::target_objective() const noexcept {
    return target_objective_;
}

int AiCommander::relevant_friendly_strength() const noexcept {
    return relevant_friendly_strength_;
}

int AiCommander::relevant_enemy_strength() const noexcept {
    return relevant_enemy_strength_;
}

std::optional<TacticalOrder> AiCommander::last_tactical_command()
    const noexcept {
    return last_tactical_command_;
}

std::size_t AiCommander::last_commanded_unit_count() const noexcept {
    return last_commanded_unit_count_;
}

std::span<const Unit::Id> AiCommander::last_commanded_unit_ids()
    const noexcept {
    return last_commanded_unit_ids_;
}

std::uint64_t AiCommander::ticks_until_next_strategy_evaluation()
    const noexcept {
    return ticks_until_next_strategy_evaluation_;
}

std::uint64_t AiCommander::strategy_evaluation_count() const noexcept {
    return strategy_evaluation_count_;
}

std::uint64_t AiCommander::tactical_command_issue_count() const noexcept {
    return tactical_command_issue_count_;
}

const AiCommanderRules& AiCommander::rules() const noexcept { return rules_; }

const AiProfile& AiCommander::profile() const noexcept { return profile_; }

std::string_view to_string(const AiCommanderStatus status) noexcept {
    switch (status) {
    case AiCommanderStatus::enabled:
        return "enabled";
    case AiCommanderStatus::paused_local_control:
        return "paused";
    case AiCommanderStatus::stopped_match_finished:
        return "stopped";
    }
    return "unknown";
}

std::string_view to_string(const AiDecisionResult result) noexcept {
    switch (result) {
    case AiDecisionResult::none:
        return "none";
    case AiDecisionResult::purchased:
        return "purchased";
    case AiDecisionResult::no_affordable_troop:
        return "saving";
    case AiDecisionResult::no_valid_deployment:
        return "no deployment";
    case AiDecisionResult::rejected:
        return "rejected";
    }
    return "unknown";
}

std::string_view to_string(const AiStrategy strategy) noexcept {
    switch (strategy) {
    case AiStrategy::attack:
        return "attack";
    case AiStrategy::defend:
        return "defend";
    case AiStrategy::regroup:
        return "regroup";
    }
    return "unknown";
}

std::string_view to_string(const AiDifficulty difficulty) noexcept {
    switch (difficulty) {
    case AiDifficulty::easy:
        return "easy";
    case AiDifficulty::medium:
        return "medium";
    case AiDifficulty::hard:
        return "hard";
    }
    return "unknown";
}

std::string_view to_string(const AiPlaystyle playstyle) noexcept {
    switch (playstyle) {
    case AiPlaystyle::balanced:
        return "balanced";
    case AiPlaystyle::aggressive:
        return "aggressive";
    case AiPlaystyle::defensive:
        return "defensive";
    }
    return "unknown";
}

std::string_view to_string(const AiPurchasePlanReason reason) noexcept {
    switch (reason) {
    case AiPurchasePlanReason::composition:
        return "composition";
    case AiPurchasePlanReason::vehicle_counter:
        return "vehicle counter";
    case AiPurchasePlanReason::frontline_anchor:
        return "frontline anchor";
    case AiPurchasePlanReason::artillery_support:
        return "artillery support";
    }
    return "unknown";
}

} // namespace siege
