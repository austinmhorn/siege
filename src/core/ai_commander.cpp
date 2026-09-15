#include "core/ai_commander.hpp"

#include "core/deployment.hpp"
#include "core/frontline.hpp"
#include "core/map_definition.hpp"
#include "core/tactical_command.hpp"
#include "core/troop_definition.hpp"
#include "core/zone_capture.hpp"
#include "world/player_state.hpp"
#include "world/world.hpp"

#include <algorithm>
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
        if (unit.team() == team) {
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
    const World& world, const Team team) noexcept {
    std::optional<std::size_t> threatened;
    int greatest_enemy_presence = 0;
    for (const std::size_t index : world.map().objective_zone_indices) {
        if (index >= world.zones().size()) {
            continue;
        }
        const Zone& zone = world.zones()[index];
        const int enemies = enemy_count(zone, team);
        if (zone.owner() == team && enemies > greatest_enemy_presence) {
            threatened = index;
            greatest_enemy_presence = enemies;
        }
    }
    return threatened;
}

} // namespace

AiCommander::AiCommander(const Team team, const AiCommanderRules rules) noexcept
    : team_{team}, rules_{rules} {
    rules_.decision_interval_seconds =
        std::max<std::uint32_t>(1, rules_.decision_interval_seconds);
    rules_.strategy_interval_seconds =
        std::max<std::uint32_t>(1, rules_.strategy_interval_seconds);
    rules_.forward_position_fraction =
        std::clamp(rules_.forward_position_fraction, 0.0F, 1.0F);
    for (float& fraction : rules_.deployment_y_fractions) {
        fraction = std::clamp(fraction, 0.0F, 1.0F);
    }
    rules_.force_selection_margin =
        std::max(0.0F, rules_.force_selection_margin);
    rules_.fallback_force_limit =
        std::max<std::size_t>(1, rules_.fallback_force_limit);
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

    const std::uint64_t purchase_interval_ticks =
        static_cast<std::uint64_t>(rules_.decision_interval_seconds) *
        world.match_state().rules().fixed_ticks_per_second;
    const std::uint64_t strategy_interval_ticks =
        static_cast<std::uint64_t>(rules_.strategy_interval_seconds) *
        world.match_state().rules().fixed_ticks_per_second;
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
    PlayerState* player = world.find_player(team_);
    if (player == nullptr) {
        last_result_ = AiDecisionResult::rejected;
        return;
    }

    std::optional<std::size_t> selected_index;
    for (std::size_t offset = 0; offset < rules_.troop_mix.size(); ++offset) {
        const std::size_t index =
            (troop_mix_cursor_ + offset) % rules_.troop_mix.size();
        const TroopDefinition* definition =
            troop_definition_for(rules_.troop_mix[index]);
        if (definition != nullptr &&
            player->can_afford(definition->purchase_cost)) {
            selected_index = index;
            break;
        }
    }
    if (!selected_index.has_value()) {
        last_result_ = AiDecisionResult::no_affordable_troop;
        return;
    }

    const TroopType troop_type = rules_.troop_mix[*selected_index];
    last_troop_choice_ = troop_type;
    const auto bounds = frontmost_deployment_bounds(world, team_);
    const TeamForwardDefinition* forward =
        team_forward_definition(world.map(), team_);
    if (!bounds.has_value() || forward == nullptr) {
        last_result_ = AiDecisionResult::no_valid_deployment;
        return;
    }

    const float forward_fraction = forward->x_direction > 0.0F
        ? rules_.forward_position_fraction
        : 1.0F - rules_.forward_position_fraction;
    const float y_fraction = rules_.deployment_y_fractions[
        placement_cursor_ % rules_.deployment_y_fractions.size()];
    const Vec2 position{
        bounds->x + bounds->width * forward_fraction,
        bounds->y + bounds->height * y_fraction,
    };
    const DeploymentResult result =
        request_deployment(world, team_, troop_type, position);
    if (result != DeploymentResult::accepted) {
        last_result_ = AiDecisionResult::rejected;
        return;
    }

    last_result_ = AiDecisionResult::purchased;
    last_deployment_position_ = position;
    troop_mix_cursor_ = (*selected_index + 1) % rules_.troop_mix.size();
    ++placement_cursor_;
    ++successful_deployments_;
}

void AiCommander::make_strategy_decision(World& world) {
    ++strategy_evaluation_count_;
    const auto threat = threatened_owned_objective(world, team_);
    const auto frontline = frontline_objective(world, team_);
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

} // namespace siege
