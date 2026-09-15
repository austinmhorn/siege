#include "core/scoring.hpp"

#include "world/world.hpp"

#include <algorithm>

namespace siege {
namespace {

bool fully_owned(const Zone& zone) noexcept {
    return (zone.owner() == Team::team_a && zone.capture_value() >= 100.0F) ||
           (zone.owner() == Team::team_b && zone.capture_value() <= -100.0F);
}

} // namespace

void update_objective_scoring(World& world,
                              const std::uint64_t fixed_tick_count,
                              ScoringRules rules) noexcept {
    rules.interval_ticks = std::max<std::uint32_t>(1, rules.interval_ticks);
    rules.points_per_objective =
        std::max<std::int64_t>(0, rules.points_per_objective);
    if (fixed_tick_count == 0 || rules.points_per_objective == 0) {
        return;
    }

    const std::uint64_t scoring_intervals = world.advance_scoring_clock(
        fixed_tick_count, rules.interval_ticks);
    if (scoring_intervals == 0) {
        return;
    }

    for (const auto& zone : world.zones()) {
        if (zone.type() != ZoneType::objective || !fully_owned(zone) ||
            !zone.occupied()) {
            continue;
        }
        if (PlayerState* player = world.find_player(zone.owner())) {
            player->add_score(
                rules.points_per_objective *
                static_cast<std::int64_t>(scoring_intervals));
        }
    }
}

} // namespace siege
