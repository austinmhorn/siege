#include "core/match.hpp"

#include "world/world.hpp"

#include <algorithm>

namespace siege {

MatchState::MatchState(MatchRules rules) noexcept : rules_(rules) {
    rules_.fixed_ticks_per_second =
        std::max<std::uint32_t>(1, rules_.fixed_ticks_per_second);
    remaining_ticks_ = rules_.duration_ticks();
    if (remaining_ticks_ == 0) {
        phase_ = MatchPhase::sudden_death;
    }
}

MatchPhase MatchState::phase() const noexcept { return phase_; }

MatchResult MatchState::result() const noexcept { return result_; }

bool MatchState::active() const noexcept { return phase_ != MatchPhase::finished; }

std::uint64_t MatchState::remaining_ticks() const noexcept {
    return remaining_ticks_;
}

std::uint64_t MatchState::phase_elapsed_ticks() const noexcept {
    return phase_elapsed_ticks_;
}

std::uint32_t MatchState::remaining_display_seconds() const noexcept {
    if (remaining_ticks_ == 0) {
        return 0;
    }
    return static_cast<std::uint32_t>(
        (remaining_ticks_ + rules_.fixed_ticks_per_second - 1) /
        rules_.fixed_ticks_per_second);
}

const MatchRules& MatchState::rules() const noexcept { return rules_; }

MatchTransition MatchState::advance(const std::uint64_t fixed_tick_count,
                                    const Score team_a_score,
                                    const Score team_b_score) noexcept {
    if (phase_ == MatchPhase::finished || fixed_tick_count == 0) {
        return MatchTransition::none;
    }
    if (phase_ == MatchPhase::sudden_death) {
        phase_elapsed_ticks_ += fixed_tick_count;
        return MatchTransition::none;
    }
    if (fixed_tick_count < remaining_ticks_) {
        remaining_ticks_ -= fixed_tick_count;
        phase_elapsed_ticks_ += fixed_tick_count;
        return MatchTransition::none;
    }

    phase_elapsed_ticks_ += remaining_ticks_;
    remaining_ticks_ = 0;
    if (team_a_score == team_b_score) {
        phase_ = MatchPhase::sudden_death;
        phase_elapsed_ticks_ = 0;
        return MatchTransition::sudden_death;
    }
    phase_ = MatchPhase::finished;
    result_ = team_a_score > team_b_score
                  ? MatchResult::team_a
                  : MatchResult::team_b;
    return MatchTransition::finished;
}

bool MatchState::resolve_sudden_death(const Team winner) noexcept {
    if (phase_ != MatchPhase::sudden_death ||
        (winner != Team::team_a && winner != Team::team_b)) {
        return false;
    }
    phase_ = MatchPhase::finished;
    result_ = winner == Team::team_a ? MatchResult::team_a
                                     : MatchResult::team_b;
    return true;
}

std::string_view to_string(const MatchPhase phase) noexcept {
    switch (phase) {
    case MatchPhase::regulation:
        return "regulation";
    case MatchPhase::sudden_death:
        return "sudden_death";
    case MatchPhase::finished:
        return "finished";
    }
    return "unknown";
}

std::string_view to_string(const MatchResult result) noexcept {
    switch (result) {
    case MatchResult::none:
        return "none";
    case MatchResult::team_a:
        return "team_a";
    case MatchResult::team_b:
        return "team_b";
    }
    return "unknown";
}

bool resolve_sudden_death_center_capture(World& world) noexcept {
    if (world.match_state().phase() != MatchPhase::sudden_death) {
        return false;
    }
    const std::size_t center_index =
        world.map().center_objective_zone_index;
    for (const auto& event : world.zone_ownership_events()) {
        if (event.zone_id == center_index &&
            event.type == ZoneTransitionType::captured &&
            world.match_state().resolve_sudden_death(event.new_owner)) {
            return true;
        }
    }
    return false;
}

} // namespace siege
