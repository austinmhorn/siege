#include "core/ai_commander.hpp"

#include "core/deployment.hpp"
#include "core/map_definition.hpp"
#include "core/troop_definition.hpp"
#include "core/zone_capture.hpp"
#include "world/player_state.hpp"
#include "world/world.hpp"

#include <algorithm>

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

} // namespace

AiCommander::AiCommander(const Team team, const AiCommanderRules rules) noexcept
    : team_{team}, rules_{rules} {
    rules_.decision_interval_seconds =
        std::max<std::uint32_t>(1, rules_.decision_interval_seconds);
    rules_.forward_position_fraction =
        std::clamp(rules_.forward_position_fraction, 0.0F, 1.0F);
    for (float& fraction : rules_.deployment_y_fractions) {
        fraction = std::clamp(fraction, 0.0F, 1.0F);
    }
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

    const std::uint64_t interval_ticks =
        static_cast<std::uint64_t>(rules_.decision_interval_seconds) *
        world.match_state().rules().fixed_ticks_per_second;
    if (ticks_until_next_decision_ == 0) {
        make_decision(world);
        ticks_until_next_decision_ = interval_ticks;
    }

    while (fixed_tick_count >= ticks_until_next_decision_) {
        fixed_tick_count -= ticks_until_next_decision_;
        make_decision(world);
        ticks_until_next_decision_ = interval_ticks;
    }
    ticks_until_next_decision_ -= fixed_tick_count;
}

void AiCommander::make_decision(World& world) {
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

} // namespace siege
