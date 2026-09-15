#include "core/match.hpp"

#include <algorithm>

namespace siege {

MatchState::MatchState(MatchRules rules) noexcept : rules_(rules) {
    rules_.fixed_ticks_per_second =
        std::max<std::uint32_t>(1, rules_.fixed_ticks_per_second);
    remaining_ticks_ = rules_.duration_ticks();
    if (remaining_ticks_ == 0) {
        phase_ = MatchPhase::finished;
        result_ = MatchResult::tie;
    }
}

MatchPhase MatchState::phase() const noexcept { return phase_; }

MatchResult MatchState::result() const noexcept { return result_; }

bool MatchState::active() const noexcept { return phase_ == MatchPhase::active; }

std::uint64_t MatchState::remaining_ticks() const noexcept {
    return remaining_ticks_;
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

bool MatchState::advance(const std::uint64_t fixed_tick_count,
                         const Score team_a_score,
                         const Score team_b_score) noexcept {
    if (!active() || fixed_tick_count == 0) {
        return false;
    }
    if (fixed_tick_count < remaining_ticks_) {
        remaining_ticks_ -= fixed_tick_count;
        return false;
    }

    remaining_ticks_ = 0;
    phase_ = MatchPhase::finished;
    result_ = team_a_score > team_b_score
                  ? MatchResult::team_a
                  : team_b_score > team_a_score ? MatchResult::team_b
                                                : MatchResult::tie;
    return true;
}

std::string_view to_string(const MatchPhase phase) noexcept {
    switch (phase) {
    case MatchPhase::active:
        return "active";
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
    case MatchResult::tie:
        return "tie";
    }
    return "unknown";
}

} // namespace siege
