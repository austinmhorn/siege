#include "core/ai_commander.hpp"

#include "core/ai_coordinated_push.hpp"
#include "core/ai_objective_occupancy.hpp"
#include "core/deployment.hpp"
#include "core/frontline.hpp"
#include "core/map_definition.hpp"
#include "core/perception.hpp"
#include "core/tactical_command.hpp"
#include "core/tactical_escort.hpp"
#include "core/tactical_group.hpp"
#include "core/troop_definition.hpp"
#include "core/zone_capture.hpp"
#include "world/player_state.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <tuple>
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
            unit.mobility_mode() != MobilityMode::player_path_only &&
            !unit.ai_objective_zone().has_value() &&
            !unit.ai_push_id().has_value()) {
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
    case TroopType::light_tank:
        return 3;
    case TroopType::medium_tank:
        return 4;
    case TroopType::heavy_tank:
        return 5;
    case TroopType::anti_tank:
        return 6;
    case TroopType::mortar:
        return 7;
    }
    return 0;
}

struct PurchasePlan {
    TroopType troop{TroopType::rifle};
    AiPurchasePlanReason reason{AiPurchasePlanReason::composition};
};

std::array<int, 8> friendly_composition(const World& world,
                                        const Team team) noexcept {
    std::array<int, 8> current{};
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
    const std::array<int, 8> current = friendly_composition(world, team);
    std::array<int, 8> desired{};
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
        TroopType::light_tank, TroopType::medium_tank, TroopType::heavy_tank,
        TroopType::anti_tank, TroopType::mortar};
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
    scores[troop_index(TroopType::light_tank)] +=
        strategy == AiStrategy::attack ? 35 : 10;
    scores[troop_index(TroopType::medium_tank)] +=
        strategy == AiStrategy::attack ? 55 : 5;
    scores[troop_index(TroopType::heavy_tank)] +=
        total_force >= 5 && ordinary_infantry >= 3 &&
                (sustained_fighting || total_force >= 10)
            ? 32
            : -130;
    scores[troop_index(TroopType::anti_tank)] +=
        visible.vehicles * 170 -
        current[troop_index(TroopType::anti_tank)] * 90;
    scores[troop_index(TroopType::mortar)] +=
        sustained_fighting ? 55 : -15;

    switch (profile.playstyle) {
    case AiPlaystyle::balanced:
        break;
    case AiPlaystyle::aggressive:
        scores[troop_index(TroopType::light_tank)] += 75;
        scores[troop_index(TroopType::medium_tank)] += 65;
        scores[troop_index(TroopType::heavy_tank)] += 20;
        scores[troop_index(TroopType::rifle)] += 20;
        scores[troop_index(TroopType::mortar)] -= 40;
        break;
    case AiPlaystyle::defensive:
        scores[troop_index(TroopType::machine_gun)] += 35;
        scores[troop_index(TroopType::mortar)] += 70;
        scores[troop_index(TroopType::medium_tank)] -= 10;
        scores[troop_index(TroopType::heavy_tank)] += 55;
        break;
    }
    switch (profile.difficulty) {
    case AiDifficulty::easy:
        scores[troop_index(TroopType::heavy_tank)] -= 25;
        scores[troop_index(TroopType::medium_tank)] -= 15;
        scores[troop_index(TroopType::mortar)] -= 10;
        break;
    case AiDifficulty::medium:
        break;
    case AiDifficulty::hard:
        scores[troop_index(TroopType::light_tank)] += 10;
        scores[troop_index(TroopType::medium_tank)] += 15;
        scores[troop_index(TroopType::heavy_tank)] += 20;
        scores[troop_index(TroopType::mortar)] += 15;
        scores[troop_index(TroopType::anti_tank)] += visible.vehicles * 30;
        break;
    }
    if (last_purchased_troop == TroopType::medium_tank) {
        scores[troop_index(TroopType::medium_tank)] -= 180;
    }
    if (last_purchased_troop == TroopType::light_tank) {
        scores[troop_index(TroopType::light_tank)] -= 150;
    }
    if (last_purchased_troop == TroopType::heavy_tank) {
        scores[troop_index(TroopType::heavy_tank)] -= 420;
    }
    if (last_purchased_troop == TroopType::mortar) {
        scores[troop_index(TroopType::mortar)] -= 220;
    }
    scores[troop_index(TroopType::light_tank)] -=
        current[troop_index(TroopType::light_tank)] * 28;
    scores[troop_index(TroopType::medium_tank)] -=
        current[troop_index(TroopType::medium_tank)] * 20;
    scores[troop_index(TroopType::heavy_tank)] -=
        current[troop_index(TroopType::heavy_tank)] * 120;
    scores[troop_index(TroopType::mortar)] -=
        current[troop_index(TroopType::mortar)] * 45;

    std::size_t best = 0;
    for (std::size_t index = 1; index < candidates.size(); ++index) {
        if (scores[index] > scores[best]) {
            best = index;
        }
    }
    const TroopType selected = candidates[best];
    const AiPurchasePlanReason reason =
        (selected == TroopType::light_tank ||
         selected == TroopType::medium_tank ||
         selected == TroopType::heavy_tank)
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
        rules.deployment_y_fractions = {0.20F, 0.80F, 0.50F, 0.35F,
                                        0.65F, 0.50F, 0.30F, 0.70F};
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
        rules.deployment_y_fractions = {0.50F, 0.35F, 0.65F, 0.25F,
                                        0.75F, 0.50F, 0.20F, 0.80F};
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
                           TroopType::light_tank, TroopType::rifle,
                           TroopType::medium_tank, TroopType::heavy_tank,
                           TroopType::anti_tank, TroopType::mortar};
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
                           TroopType::medium_tank, TroopType::heavy_tank,
                           TroopType::anti_tank, TroopType::mortar};
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
        set_objective_holder_movement_active(world, false);
        end_coordinated_push(world, false);
        return;
    }
    if (locally_controlled_team == team_) {
        status_ = AiCommanderStatus::paused_local_control;
        set_objective_holder_movement_active(world, false);
        if (push_state_ != AiPushState::idle) {
            end_coordinated_push(world, true);
        }
        return;
    }
    status_ = AiCommanderStatus::enabled;
    set_objective_holder_movement_active(world, true);
    if (fixed_tick_count == 0 || team_ == Team::none) {
        return;
    }

    update_objective_occupancy(world, fixed_tick_count);
    update_coordinated_push(world, fixed_tick_count);

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
    bool occupancy_fallback = false;
    const TroopDefinition* fallback = troop_definition_for(TroopType::rifle);
    if (objective_fallback_request_.has_value() && fallback != nullptr &&
        player->can_afford(fallback->purchase_cost)) {
        troop_type = TroopType::rifle;
        purchasing_plan = false;
        occupancy_fallback = true;
        emergency_override_active_ = true;
    } else if (!player->can_afford(planned_definition->purchase_cost)) {
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
        if (purchasing_plan && ++deployment_failure_evaluations_ >= 3) {
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
        if (purchasing_plan && ++deployment_failure_evaluations_ >= 3) {
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
    if (occupancy_fallback && objective_fallback_request_.has_value() &&
        !world.pending_deployments().empty()) {
        world.pending_deployments().back().ai_objective_zone =
            *objective_fallback_request_;
        objective_fallback_request_.reset();
    }
    if (purchasing_plan) {
        last_purchased_troop_ = troop_type;
        planned_purchase_.reset();
        planned_purchase_reason_.reset();
        emergency_spent_for_plan_ = false;
    } else if (!occupancy_fallback) {
        emergency_spent_for_plan_ = true;
    }
    placement_cursor_ += selected_placement_offset + 1;
    ++successful_deployments_;
}

void AiCommander::set_objective_holder_movement_active(
    World& world, const bool active) const noexcept {
    for (Unit& unit : world.units()) {
        if (unit.team() == team_ && unit.ai_objective_zone().has_value()) {
            unit.set_ai_objective_assignment_active(active);
        }
    }
}

void AiCommander::update_objective_occupancy(
    World& world, const std::uint64_t fixed_tick_count) {
    const std::uint64_t grace_ticks = std::max<std::uint64_t>(
        1, static_cast<std::uint64_t>(std::llround(
               ai_objective_natural_occupancy_grace_seconds *
               world.match_state().rules().fixed_ticks_per_second)));

    const auto previous_status = [this](const std::size_t zone_index) {
        return std::ranges::find_if(
            objective_occupancy_, [zone_index](const auto& status) {
                return status.zone_index == zone_index;
            });
    };
    std::vector<AiObjectiveOccupancyStatus> next;
    next.reserve(world.map().objective_zone_indices.size());
    objective_fallback_request_.reset();

    const auto unit_zone = [&world](const Unit& unit) {
        return zone_index_for_position(world, unit.position());
    };
    const auto sole_occupant_of_other_owned_objective =
        [&world, this, &unit_zone](const Unit& unit,
                                  const std::size_t destination_zone) {
            const auto current_zone = unit_zone(unit);
            if (!current_zone.has_value() ||
                *current_zone == destination_zone ||
                *current_zone >= world.zones().size()) {
                return false;
            }
            const Zone& zone = world.zones()[*current_zone];
            if (zone.type() != ZoneType::objective || zone.owner() != team_) {
                return false;
            }
            return std::ranges::count_if(
                       world.units(), [this, &unit_zone, current_zone](
                                          const Unit& other) {
                           return other.is_alive() && other.team() == team_ &&
                               unit_zone(other) == current_zone;
                       }) == 1;
        };

    for (const std::size_t zone_index : world.map().objective_zone_indices) {
        AiObjectiveOccupancyStatus status{.zone_index = zone_index};
        const auto old = previous_status(zone_index);
        if (old != objective_occupancy_.end()) {
            status.natural_occupancy_ticks = old->natural_occupancy_ticks;
        }
        if (zone_index >= world.zones().size() ||
            world.zones()[zone_index].owner() != team_) {
            for (Unit& unit : world.units()) {
                if (unit.team() == team_ &&
                    unit.ai_objective_zone() == zone_index) {
                    unit.clear_ai_objective_assignment();
                }
            }
            for (PendingDeployment& pending : world.pending_deployments()) {
                if (pending.team == team_ &&
                    pending.ai_objective_zone == zone_index) {
                    pending.ai_objective_zone.reset();
                }
            }
            status.coverage = AiObjectiveCoverage::not_owned;
            status.natural_occupancy_ticks = 0;
            next.push_back(status);
            continue;
        }

        Unit* holder = nullptr;
        for (Unit& unit : world.units()) {
            if (!unit.is_alive() || unit.team() != team_ ||
                unit.ai_objective_zone() != zone_index) {
                continue;
            }
            const bool assignment_valid =
                unit.troop_type() != TroopType::mortar &&
                !unit.has_movement_path() &&
                unit.tactical_order() != TacticalOrder::hold &&
                unit.tactical_order() != TacticalOrder::regroup;
            if (!assignment_valid || holder != nullptr) {
                unit.clear_ai_objective_assignment();
                continue;
            }
            holder = &unit;
            holder->set_ai_objective_assignment_active(true);
        }

        const bool naturally_occupied = std::ranges::any_of(
            world.units(), [this, zone_index, holder, &unit_zone](
                               const Unit& unit) {
                return unit.is_alive() && unit.team() == team_ &&
                    (&unit != holder) && unit_zone(unit) == zone_index;
            });
        if (naturally_occupied) {
            if (holder != nullptr) {
                status.natural_occupancy_ticks = std::min(
                    grace_ticks,
                    status.natural_occupancy_ticks + fixed_tick_count);
                if (status.natural_occupancy_ticks >= grace_ticks) {
                    holder->clear_ai_objective_assignment();
                    holder = nullptr;
                }
            } else {
                status.natural_occupancy_ticks = grace_ticks;
            }
            status.coverage = holder == nullptr
                ? AiObjectiveCoverage::naturally_occupied
                : AiObjectiveCoverage::assigned;
            if (holder != nullptr) {
                status.holder_id = holder->id();
            }
            next.push_back(status);
            continue;
        }
        status.natural_occupancy_ticks = 0;

        if (holder == nullptr) {
            std::vector<Unit*> candidates;
            for (Unit& unit : world.units()) {
                if (!unit.is_alive() || unit.team() != team_ ||
                    unit.troop_type() == TroopType::mortar ||
                    unit.mobility_mode() == MobilityMode::player_path_only ||
                    unit.has_movement_path() ||
                    unit.tactical_order() == TacticalOrder::hold ||
                    unit.tactical_order() == TacticalOrder::regroup ||
                    unit.ai_objective_zone().has_value() ||
                    unit.ai_push_id().has_value() ||
                    sole_occupant_of_other_owned_objective(unit, zone_index)) {
                    continue;
                }
                candidates.push_back(&unit);
            }
            const Bounds& bounds = world.zones()[zone_index].bounds();
            std::ranges::sort(candidates, [&bounds](const Unit* left,
                                                    const Unit* right) {
                const auto key = [&bounds](const Unit* unit) {
                    const TroopDefinition* definition =
                        troop_definition_for(unit->troop_type());
                    const Money cost = definition == nullptr
                        ? Money{} : definition->purchase_cost;
                    return std::tuple{
                        unit->target_id().has_value(),
                        unit->target_category() == TargetCategory::vehicle,
                        cost,
                        squared_distance_to_bounds(unit->position(), bounds),
                        unit->id()};
                };
                return key(left) < key(right);
            });
            if (!candidates.empty()) {
                holder = candidates.front();
                holder->set_ai_objective_assignment(
                    zone_index,
                    ai_objective_hold_position(
                        world, world.zones()[zone_index], team_,
                        holder->preferred_y(), holder->hit_radius()));
                holder->set_ai_objective_assignment_active(true);
            }
        }

        if (holder != nullptr) {
            status.coverage = AiObjectiveCoverage::assigned;
            status.holder_id = holder->id();
        } else {
            const bool pending = std::ranges::any_of(
                world.pending_deployments(), [this, zone_index](
                                                 const PendingDeployment& item) {
                    return item.team == team_ &&
                        item.ai_objective_zone == zone_index;
                });
            status.coverage = pending
                ? AiObjectiveCoverage::pending_reinforcement
                : AiObjectiveCoverage::uncovered;
            if (!pending && !objective_fallback_request_.has_value()) {
                objective_fallback_request_ = zone_index;
            }
        }
        next.push_back(status);
    }
    objective_occupancy_ = std::move(next);
}

void AiCommander::set_push_staging_active(World& world,
                                          const bool active) const noexcept {
    for (const Unit::Id id : push_member_ids_) {
        Unit* unit = world.find_unit(id);
        if (unit != nullptr && unit->team() == team_ &&
            unit->ai_push_id() == push_id_) {
            unit->set_ai_push_staging_active(
                active && push_state_ == AiPushState::staging);
        }
    }
}

void AiCommander::update_push_role_positions(World& world) {
    if (!push_id_.has_value() || push_member_ids_.empty()) {
        return;
    }
    const TeamForwardDefinition* forward =
        team_forward_definition(world.map(), team_);
    if (forward == nullptr) {
        return;
    }

    std::vector<Unit*> members;
    for (const Unit::Id id : push_member_ids_) {
        Unit* unit = world.find_unit(id);
        if (unit != nullptr && unit->is_alive() && unit->team() == team_ &&
            unit->ai_push_id() == push_id_) {
            members.push_back(unit);
        }
    }
    std::ranges::sort(members, {}, &Unit::id);
    if (members.empty()) {
        return;
    }

    Unit* anchor = nullptr;
    for (Unit* unit : members) {
        if (is_tank_escort_anchor(unit->troop_type())) {
            anchor = unit;
            break;
        }
    }
    if (anchor == nullptr) {
        const auto rifle = std::ranges::find_if(members, [](const Unit* unit) {
            return unit->troop_type() == TroopType::rifle;
        });
        anchor = rifle != members.end() ? *rifle : members.front();
    }

    struct RoleMember {
        Unit* unit;
        AiPushRole role;
        std::optional<Vec2> escort_position;
        Unit::Id role_anchor_id;
    };
    std::vector<RoleMember> role_members;
    role_members.reserve(members.size());
    for (Unit* unit : members) {
        AiPushRole role = AiPushRole::support;
        std::optional<Vec2> escort_position;
        Unit::Id role_anchor_id = anchor->id();
        if (unit == anchor) {
            role = AiPushRole::front_anchor;
        } else if (is_tank_escort_anchor(unit->troop_type()) ||
                   unit->troop_type() == TroopType::rifle) {
            role = AiPushRole::frontline;
        } else if (unit->troop_type() == TroopType::anti_tank) {
            const auto escort = anti_tank_escort_target(*unit, world.units());
            if (escort.has_value()) {
                role = AiPushRole::escort;
                escort_position = escort->desired_position;
                role_anchor_id = escort->anchor_id;
            }
        }
        role_members.push_back({unit, role, escort_position, role_anchor_id});
    }

    const Vec2 anchor_position = push_state_ == AiPushState::staging &&
            push_staging_point_.has_value()
        ? *push_staging_point_
        : anchor->position();
    const auto role_count = [&role_members](const AiPushRole role) {
        return static_cast<std::size_t>(std::ranges::count_if(
            role_members, [role](const RoleMember& member) {
                return member.role == role;
            }));
    };
    const std::size_t frontline_count = role_count(AiPushRole::frontline);
    const std::size_t support_count = role_count(AiPushRole::support);
    std::size_t frontline_index = 0;
    std::size_t support_index = 0;
    for (RoleMember& member : role_members) {
        std::size_t role_index = 0;
        std::size_t count = 1;
        if (member.role == AiPushRole::frontline) {
            role_index = frontline_index++;
            count = frontline_count;
        } else if (member.role == AiPushRole::support) {
            role_index = support_index++;
            count = support_count;
        }
        Vec2 desired = member.escort_position.value_or(ai_push_role_position(
            anchor_position, forward->x_direction, member.role,
            role_index, count));
        const float inset = std::max(32.0F, member.unit->hit_radius());
        desired.x = std::clamp(desired.x, inset,
                               world.map().logical_width - inset);
        desired.y = std::clamp(desired.y, inset,
                               world.map().logical_height - inset);
        member.unit->set_ai_push_formation(
            member.role, member.role_anchor_id, desired);
    }
}

bool AiCommander::start_coordinated_push(World& world) {
    const auto frontline = frontline_objective(world, team_);
    if (!frontline.has_value() ||
        frontline->zone_index >= world.zones().size() ||
        world.zones()[frontline->zone_index].owner() == team_) {
        return false;
    }

    std::vector<Unit*> eligible;
    for (Unit& unit : world.units()) {
        if (!unit.is_alive() || unit.team() != team_ ||
            unit.troop_type() == TroopType::mortar ||
            unit.mobility_mode() != MobilityMode::autonomous ||
            unit.has_movement_path() ||
            unit.tactical_order() == TacticalOrder::hold ||
            unit.tactical_order() == TacticalOrder::regroup ||
            unit.ai_objective_zone().has_value() ||
            unit.ai_push_id().has_value() || unit.group_id().has_value()) {
            continue;
        }
        eligible.push_back(&unit);
    }
    std::ranges::sort(eligible, {}, &Unit::id);

    const std::size_t minimum_members =
        profile_.playstyle == AiPlaystyle::aggressive ? 2U
        : profile_.playstyle == AiPlaystyle::defensive ? 4U
                                                       : 3U;
    const std::size_t maximum_members =
        profile_.playstyle == AiPlaystyle::aggressive ? 4U
        : profile_.playstyle == AiPlaystyle::defensive ? 8U
                                                       : 6U;
    if (eligible.size() < minimum_members) {
        return false;
    }

    const auto tank_rank = [this](const TroopType type) {
        if (profile_.playstyle == AiPlaystyle::aggressive) {
            return type == TroopType::light_tank ? 0
                : type == TroopType::medium_tank ? 1 : 2;
        }
        if (profile_.playstyle == AiPlaystyle::defensive) {
            return type == TroopType::heavy_tank ? 0
                : type == TroopType::medium_tank ? 1 : 2;
        }
        return type == TroopType::medium_tank ? 0
            : type == TroopType::light_tank ? 1 : 2;
    };
    std::vector<Unit*> tanks;
    for (Unit* unit : eligible) {
        if (is_tank_escort_anchor(unit->troop_type())) {
            tanks.push_back(unit);
        }
    }
    std::ranges::sort(tanks, [&tank_rank](const Unit* left,
                                         const Unit* right) {
        return std::pair{tank_rank(left->troop_type()), left->id()} <
               std::pair{tank_rank(right->troop_type()), right->id()};
    });

    std::vector<Unit*> selected;
    selected.reserve(maximum_members);
    const auto add_first = [&selected, maximum_members, &eligible](
                               const TroopType type,
                               const std::size_t maximum_count) {
        std::size_t added = 0;
        for (Unit* unit : eligible) {
            if (selected.size() >= maximum_members ||
                added >= maximum_count) {
                break;
            }
            if (unit->troop_type() == type &&
                !std::ranges::contains(selected, unit)) {
                selected.push_back(unit);
                ++added;
            }
        }
    };
    if (!tanks.empty()) {
        selected.push_back(tanks.front());
        add_first(TroopType::anti_tank, 1);
    }
    const std::size_t desired_rifles =
        profile_.playstyle == AiPlaystyle::aggressive ? 2U
        : profile_.playstyle == AiPlaystyle::defensive ? 4U
                                                       : 3U;
    add_first(TroopType::rifle, desired_rifles);
    add_first(TroopType::machine_gun, 1);
    add_first(TroopType::bazooka, 1);
    if (tanks.empty()) {
        add_first(TroopType::anti_tank, 1);
    }
    for (Unit* unit : eligible) {
        if (selected.size() >= maximum_members) {
            break;
        }
        if (!is_tank_escort_anchor(unit->troop_type()) &&
            !std::ranges::contains(selected, unit)) {
            selected.push_back(unit);
        }
    }
    if (selected.size() < minimum_members) {
        return false;
    }
    std::ranges::sort(selected, {}, &Unit::id);

    double y_sum = 0.0;
    for (const Unit* unit : selected) {
        y_sum += unit->preferred_y();
    }
    const auto staging = ai_push_staging_point(
        world, team_, static_cast<float>(y_sum / selected.size()));
    if (!staging.has_value()) {
        return false;
    }

    push_state_ = AiPushState::staging;
    push_id_ = next_push_id_++;
    push_staging_point_ = staging;
    push_starting_frontline_zone_ = frontline->zone_index;
    push_starting_frontline_owner_ =
        world.zones()[frontline->zone_index].owner();
    push_elapsed_ticks_ = 0;
    push_ready_member_count_ = 0;
    push_member_ids_.clear();
    for (std::size_t index = 0; index < selected.size(); ++index) {
        Unit* unit = selected[index];
        unit->set_ai_push_assignment(*push_id_, *staging);
        unit->set_ai_push_staging_active(true);
        push_member_ids_.push_back(unit->id());
    }

    Unit* tank = nullptr;
    std::vector<Unit*> anti_tank_members;
    for (Unit* unit : selected) {
        if (tank == nullptr && is_tank_escort_anchor(unit->troop_type())) {
            tank = unit;
        } else if (unit->troop_type() == TroopType::anti_tank) {
            anti_tank_members.push_back(unit);
        }
    }
    if (tank != nullptr && !anti_tank_members.empty()) {
        push_temporary_group_id_ = world.allocate_tactical_group_id();
        tank->set_group_id(push_temporary_group_id_);
        for (Unit* escort : anti_tank_members) {
            escort->set_group_id(push_temporary_group_id_);
        }
    }
    update_push_role_positions(world);
    return true;
}

void AiCommander::release_coordinated_push(World& world) {
    if (push_state_ != AiPushState::staging) {
        return;
    }
    std::vector<Unit::Id> living;
    for (const Unit::Id id : push_member_ids_) {
        Unit* unit = world.find_unit(id);
        if (unit == nullptr || !unit->is_alive() || unit->team() != team_ ||
            unit->ai_push_id() != push_id_) {
            continue;
        }
        unit->set_ai_push_staging_active(false);
        living.push_back(id);
    }
    push_member_ids_ = living;
    (void)apply_tactical_order(world, push_member_ids_,
                               TacticalOrder::advance, team_);
    push_state_ = AiPushState::advancing;
    push_elapsed_ticks_ = 0;
    update_push_role_positions(world);
}

void AiCommander::end_coordinated_push(World& world,
                                       const bool start_cooldown) {
    for (const Unit::Id id : push_member_ids_) {
        Unit* unit = world.find_unit(id);
        if (unit == nullptr || unit->team() != team_ ||
            unit->ai_push_id() != push_id_) {
            continue;
        }
        if (unit->group_id() == push_temporary_group_id_) {
            unit->clear_group_id();
        }
        unit->clear_ai_push_assignment();
        if (unit->tactical_order() == TacticalOrder::advance) {
            unit->set_tactical_order(TacticalOrder::automatic);
        }
    }
    cleanup_tactical_groups(world);
    push_state_ = AiPushState::idle;
    push_id_.reset();
    push_member_ids_.clear();
    push_staging_point_.reset();
    push_starting_frontline_zone_.reset();
    push_starting_frontline_owner_ = Team::none;
    push_temporary_group_id_.reset();
    push_ready_member_count_ = 0;
    push_elapsed_ticks_ = 0;
    if (start_cooldown && profile_.difficulty != AiDifficulty::easy) {
        double seconds = profile_.difficulty == AiDifficulty::hard
            ? default_ai_coordinated_push_rules.hard_cooldown_seconds
            : default_ai_coordinated_push_rules.medium_cooldown_seconds;
        if (profile_.playstyle == AiPlaystyle::aggressive) {
            seconds *= 0.75;
        } else if (profile_.playstyle == AiPlaystyle::defensive) {
            seconds *= 1.50;
        }
        push_cooldown_ticks_ = static_cast<std::uint64_t>(std::llround(
            seconds * world.match_state().rules().fixed_ticks_per_second));
    }
}

void AiCommander::update_coordinated_push(
    World& world, const std::uint64_t fixed_tick_count) {
    if (profile_.difficulty == AiDifficulty::easy) {
        push_cooldown_initialized_ = true;
        return;
    }
    const auto ticks_for = [&world](const double seconds) {
        return std::max<std::uint64_t>(
            1, static_cast<std::uint64_t>(std::llround(
                   seconds *
                   world.match_state().rules().fixed_ticks_per_second)));
    };
    if (!push_cooldown_initialized_) {
        double seconds = profile_.difficulty == AiDifficulty::hard
            ? default_ai_coordinated_push_rules.hard_cooldown_seconds
            : default_ai_coordinated_push_rules.medium_cooldown_seconds;
        if (profile_.playstyle == AiPlaystyle::aggressive) {
            seconds *= 0.75;
        } else if (profile_.playstyle == AiPlaystyle::defensive) {
            seconds *= 1.50;
        }
        push_cooldown_ticks_ = ticks_for(seconds);
        push_cooldown_initialized_ = true;
    }

    std::uint64_t remaining = fixed_tick_count;
    while (remaining > 0) {
        if (push_state_ == AiPushState::idle) {
            if (push_cooldown_ticks_ > remaining) {
                push_cooldown_ticks_ -= remaining;
                return;
            }
            remaining -= push_cooldown_ticks_;
            push_cooldown_ticks_ = 0;
            if (!start_coordinated_push(world)) {
                push_cooldown_ticks_ = ticks_for(1.0);
                return;
            }
            if (remaining == 0) {
                return;
            }
        }

        const auto invalid_push_member = [this, &world](const Unit::Id id) {
            const Unit* unit = world.find_unit(id);
            return unit == nullptr || !unit->is_alive() ||
                unit->team() != team_ || unit->ai_push_id() != push_id_ ||
                unit->troop_type() == TroopType::mortar ||
                unit->ai_objective_zone().has_value() ||
                unit->has_movement_path() ||
                unit->tactical_order() == TacticalOrder::hold ||
                unit->tactical_order() == TacticalOrder::regroup;
        };
        for (const Unit::Id id : push_member_ids_) {
            Unit* unit = world.find_unit(id);
            if (!invalid_push_member(id) || unit == nullptr ||
                unit->ai_push_id() != push_id_) {
                continue;
            }
            if (unit->group_id() == push_temporary_group_id_) {
                unit->clear_group_id();
            }
            unit->clear_ai_push_assignment();
        }
        std::erase_if(push_member_ids_, invalid_push_member);
        if (push_member_ids_.size() <= 1) {
            end_coordinated_push(world, true);
            continue;
        }
        if (push_temporary_group_id_.has_value() &&
            tactical_group_members(world, *push_temporary_group_id_).size() < 2) {
            for (Unit& unit : world.units()) {
                if (unit.group_id() == push_temporary_group_id_) {
                    unit.clear_group_id();
                }
            }
            push_temporary_group_id_.reset();
        }
        update_push_role_positions(world);

        if (push_state_ == AiPushState::staging) {
            push_ready_member_count_ = static_cast<std::size_t>(
                std::ranges::count_if(
                    push_member_ids_, [&world](const Unit::Id id) {
                        const Unit* unit = world.find_unit(id);
                        return unit != nullptr &&
                            unit->ai_push_desired_position().has_value() &&
                            length(unit->position() -
                                   *unit->ai_push_desired_position()) <=
                                default_ai_coordinated_push_rules
                                    .readiness_radius;
                    }));
            const std::size_t required = static_cast<std::size_t>(std::ceil(
                default_ai_coordinated_push_rules.readiness_fraction *
                static_cast<float>(push_member_ids_.size())));
            if (push_ready_member_count_ >= required) {
                release_coordinated_push(world);
                continue;
            }
            const std::uint64_t timeout = ticks_for(
                default_ai_coordinated_push_rules.staging_timeout_seconds);
            const std::uint64_t until_timeout = timeout > push_elapsed_ticks_
                ? timeout - push_elapsed_ticks_ : 0;
            if (remaining < until_timeout) {
                push_elapsed_ticks_ += remaining;
                return;
            }
            remaining -= until_timeout;
            push_elapsed_ticks_ = timeout;
            release_coordinated_push(world);
            continue;
        }

        const auto frontline = frontline_objective(world, team_);
        const bool frontline_advanced =
            !frontline.has_value() ||
            frontline->zone_index != push_starting_frontline_zone_ ||
            (push_starting_frontline_zone_.has_value() &&
             *push_starting_frontline_zone_ < world.zones().size() &&
             world.zones()[*push_starting_frontline_zone_].owner() !=
                 push_starting_frontline_owner_);
        if (frontline_advanced) {
            end_coordinated_push(world, true);
            continue;
        }
        const std::uint64_t timeout = ticks_for(
            default_ai_coordinated_push_rules.advance_timeout_seconds);
        const std::uint64_t until_timeout = timeout > push_elapsed_ticks_
            ? timeout - push_elapsed_ticks_ : 0;
        if (remaining < until_timeout) {
            push_elapsed_ticks_ += remaining;
            return;
        }
        remaining -= until_timeout;
        push_elapsed_ticks_ = timeout;
        end_coordinated_push(world, true);
    }
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

std::span<const AiObjectiveOccupancyStatus>
AiCommander::objective_occupancy() const noexcept {
    return objective_occupancy_;
}

bool AiCommander::is_objective_holder(const Unit::Id unit_id) const noexcept {
    return std::ranges::any_of(
        objective_occupancy_, [unit_id](const auto& status) {
            return status.holder_id == unit_id;
        });
}

AiPushState AiCommander::push_state() const noexcept { return push_state_; }

std::optional<std::uint32_t> AiCommander::push_id() const noexcept {
    return push_id_;
}

std::span<const Unit::Id> AiCommander::push_members() const noexcept {
    return push_member_ids_;
}

std::optional<Vec2> AiCommander::push_staging_point() const noexcept {
    return push_staging_point_;
}

std::size_t AiCommander::push_ready_member_count() const noexcept {
    return push_ready_member_count_;
}

std::uint64_t AiCommander::push_elapsed_ticks() const noexcept {
    return push_elapsed_ticks_;
}

std::uint64_t AiCommander::push_cooldown_ticks() const noexcept {
    return push_cooldown_ticks_;
}

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

std::string_view to_string(const AiObjectiveCoverage coverage) noexcept {
    switch (coverage) {
    case AiObjectiveCoverage::not_owned:
        return "not owned";
    case AiObjectiveCoverage::naturally_occupied:
        return "naturally occupied";
    case AiObjectiveCoverage::assigned:
        return "holder";
    case AiObjectiveCoverage::pending_reinforcement:
        return "reinforcement pending";
    case AiObjectiveCoverage::uncovered:
        return "uncovered";
    }
    return "unknown";
}

std::string_view to_string(const AiPushState state) noexcept {
    switch (state) {
    case AiPushState::idle:
        return "idle";
    case AiPushState::staging:
        return "staging";
    case AiPushState::advancing:
        return "advancing";
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
