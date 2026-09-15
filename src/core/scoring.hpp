#pragma once

#include <cstdint>

namespace siege {

class World;

struct ScoringRules {
    std::uint32_t interval_ticks;
    std::int64_t points_per_objective;
};

inline constexpr ScoringRules default_scoring_rules{
    .interval_ticks = 60,
    .points_per_objective = 1,
};

void update_objective_scoring(
    World& world, std::uint64_t fixed_tick_count = 1,
    ScoringRules rules = default_scoring_rules) noexcept;

} // namespace siege
