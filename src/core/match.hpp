#pragma once

#include "world/player_state.hpp"

#include <cstdint>
#include <string_view>

namespace siege {

enum class MatchPhase {
    active,
    finished,
};

enum class MatchResult {
    none,
    team_a,
    team_b,
    tie,
};

struct MatchRules {
    std::uint32_t fixed_ticks_per_second;
    std::uint32_t duration_seconds;

    [[nodiscard]] constexpr std::uint64_t duration_ticks() const noexcept {
        return static_cast<std::uint64_t>(fixed_ticks_per_second) *
               duration_seconds;
    }
};

inline constexpr MatchRules default_match_rules{
    .fixed_ticks_per_second = 60,
    .duration_seconds = 300,
};

class MatchState {
public:
    explicit MatchState(MatchRules rules = default_match_rules) noexcept;

    [[nodiscard]] MatchPhase phase() const noexcept;
    [[nodiscard]] MatchResult result() const noexcept;
    [[nodiscard]] bool active() const noexcept;
    [[nodiscard]] std::uint64_t remaining_ticks() const noexcept;
    [[nodiscard]] std::uint32_t remaining_display_seconds() const noexcept;
    [[nodiscard]] const MatchRules& rules() const noexcept;

    // Returns true only on the single transition from active to finished.
    [[nodiscard]] bool advance(std::uint64_t fixed_tick_count,
                               Score team_a_score,
                               Score team_b_score) noexcept;

private:
    MatchRules rules_;
    std::uint64_t remaining_ticks_{};
    MatchPhase phase_{MatchPhase::active};
    MatchResult result_{MatchResult::none};
};

[[nodiscard]] std::string_view to_string(MatchPhase phase) noexcept;
[[nodiscard]] std::string_view to_string(MatchResult result) noexcept;

} // namespace siege
