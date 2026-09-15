#pragma once

#include "world/unit.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace siege {

class World;

struct AiCommanderRules {
    std::uint32_t decision_interval_seconds;
    std::uint32_t strategy_interval_seconds;
    std::array<TroopType, 4> troop_mix;
    std::array<float, 6> deployment_y_fractions;
    float forward_position_fraction;
    float force_selection_margin;
    std::size_t fallback_force_limit;
    float regroup_outnumber_ratio;
    int regroup_minimum_enemy_advantage;
    float regroup_scatter_distance;
    float regroup_severe_scatter_distance;
};

inline constexpr AiCommanderRules default_ai_commander_rules{
    .decision_interval_seconds = 2,
    .strategy_interval_seconds = 1,
    .troop_mix = {TroopType::rifle, TroopType::machine_gun,
                  TroopType::rifle, TroopType::bazooka},
    .deployment_y_fractions = {0.22F, 0.38F, 0.50F, 0.62F, 0.78F, 0.50F},
    .forward_position_fraction = 0.80F,
    .force_selection_margin = 220.0F,
    .fallback_force_limit = 6,
    .regroup_outnumber_ratio = 1.50F,
    .regroup_minimum_enemy_advantage = 2,
    .regroup_scatter_distance = 260.0F,
    .regroup_severe_scatter_distance = 420.0F,
};

enum class AiStrategy {
    attack,
    defend,
    regroup,
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
    [[nodiscard]] AiStrategy strategy() const noexcept;
    [[nodiscard]] std::optional<std::size_t> target_objective() const noexcept;
    [[nodiscard]] int relevant_friendly_strength() const noexcept;
    [[nodiscard]] int relevant_enemy_strength() const noexcept;
    [[nodiscard]] std::optional<TacticalOrder> last_tactical_command()
        const noexcept;
    [[nodiscard]] std::size_t last_commanded_unit_count() const noexcept;
    [[nodiscard]] std::span<const Unit::Id> last_commanded_unit_ids()
        const noexcept;
    [[nodiscard]] std::uint64_t ticks_until_next_strategy_evaluation()
        const noexcept;
    [[nodiscard]] std::uint64_t strategy_evaluation_count() const noexcept;
    [[nodiscard]] std::uint64_t tactical_command_issue_count() const noexcept;
    [[nodiscard]] const AiCommanderRules& rules() const noexcept;

private:
    void make_purchase_decision(World& world);
    void make_strategy_decision(World& world);
    void issue_tactical_command(World& world, TacticalOrder order,
                                std::size_t objective_index,
                                std::vector<Unit::Id> unit_ids);

    Team team_{Team::none};
    AiCommanderRules rules_{};
    AiCommanderStatus status_{AiCommanderStatus::enabled};
    AiDecisionResult last_result_{AiDecisionResult::none};
    std::optional<TroopType> last_troop_choice_{};
    std::optional<Vec2> last_deployment_position_{};
    std::uint64_t ticks_until_next_decision_{};
    std::uint64_t ticks_until_next_strategy_evaluation_{};
    std::uint64_t successful_deployments_{};
    std::uint64_t strategy_evaluation_count_{};
    std::uint64_t tactical_command_issue_count_{};
    std::size_t troop_mix_cursor_{};
    std::size_t placement_cursor_{};
    AiStrategy strategy_{AiStrategy::attack};
    std::optional<std::size_t> target_objective_{};
    int relevant_friendly_strength_{};
    int relevant_enemy_strength_{};
    std::optional<TacticalOrder> last_tactical_command_{};
    std::size_t last_commanded_unit_count_{};
    std::optional<std::size_t> last_command_objective_{};
    std::vector<Unit::Id> last_commanded_unit_ids_{};
};

[[nodiscard]] std::string_view to_string(AiCommanderStatus status) noexcept;
[[nodiscard]] std::string_view to_string(AiDecisionResult result) noexcept;
[[nodiscard]] std::string_view to_string(AiStrategy strategy) noexcept;

} // namespace siege
