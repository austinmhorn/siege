#pragma once

#include "world/unit.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace siege {

class World;

struct AiCommanderRules {
    std::uint32_t decision_interval_seconds;
    std::array<TroopType, 4> troop_mix;
    std::array<float, 6> deployment_y_fractions;
    float forward_position_fraction;
};

inline constexpr AiCommanderRules default_ai_commander_rules{
    .decision_interval_seconds = 2,
    .troop_mix = {TroopType::rifle, TroopType::machine_gun,
                  TroopType::rifle, TroopType::bazooka},
    .deployment_y_fractions = {0.22F, 0.38F, 0.50F, 0.62F, 0.78F, 0.50F},
    .forward_position_fraction = 0.80F,
};

enum class AiCommanderStatus {
    enabled,
    paused_local_control,
    stopped_match_finished,
};

enum class AiDecisionResult {
    none,
    purchased,
    no_affordable_troop,
    no_valid_deployment,
    rejected,
};

class AiCommander {
public:
    explicit AiCommander(
        Team team,
        AiCommanderRules rules = default_ai_commander_rules) noexcept;

    // One fixed simulation tick is the normal call. Passing multiple ticks is
    // supported so deterministic tests/headless hosts can advance in batches.
    void update(World& world, Team locally_controlled_team,
                std::uint64_t fixed_tick_count = 1);

    [[nodiscard]] Team team() const noexcept;
    [[nodiscard]] AiCommanderStatus status() const noexcept;
    [[nodiscard]] AiDecisionResult last_result() const noexcept;
    [[nodiscard]] std::optional<TroopType> last_troop_choice() const noexcept;
    [[nodiscard]] std::optional<Vec2> last_deployment_position() const noexcept;
    [[nodiscard]] std::uint64_t ticks_until_next_decision() const noexcept;
    [[nodiscard]] std::uint64_t successful_deployments() const noexcept;
    [[nodiscard]] const AiCommanderRules& rules() const noexcept;

private:
    void make_decision(World& world);

    Team team_{Team::none};
    AiCommanderRules rules_{};
    AiCommanderStatus status_{AiCommanderStatus::enabled};
    AiDecisionResult last_result_{AiDecisionResult::none};
    std::optional<TroopType> last_troop_choice_{};
    std::optional<Vec2> last_deployment_position_{};
    std::uint64_t ticks_until_next_decision_{};
    std::uint64_t successful_deployments_{};
    std::size_t troop_mix_cursor_{};
    std::size_t placement_cursor_{};
};

[[nodiscard]] std::string_view to_string(AiCommanderStatus status) noexcept;
[[nodiscard]] std::string_view to_string(AiDecisionResult result) noexcept;

} // namespace siege
