#pragma once

#include "core/math.hpp"
#include "world/zone.hpp"

#include <cstdint>
#include <optional>
#include <string_view>

namespace siege {

enum class TroopType {
    rifle,
    machine_gun,
    bazooka,
};

enum class MovementState {
    idle,
    moving,
};

enum class CombatMovementState {
    advancing,
    closing,
    engaging,
    retreating,
};

[[nodiscard]] std::string_view to_string(TroopType type) noexcept;
[[nodiscard]] std::string_view to_string(Team team) noexcept;
[[nodiscard]] std::string_view to_string(MovementState state) noexcept;
[[nodiscard]] std::string_view to_string(CombatMovementState state) noexcept;

class Unit {
public:
    using Id = std::uint32_t;

    Unit(Id id, TroopType troop_type, Team team, Vec2 spawn_position,
         float move_speed, float rotation_speed, float vision_range,
         float vision_angle, float awareness_radius, float preferred_combat_range,
         float range_tolerance, float aggression, float retreat_bias,
         float initial_facing_angle) noexcept;

    void begin_simulation_step() noexcept;
    void set_position(Vec2 position) noexcept;
    void set_desired_facing_angle(float angle) noexcept;
    void set_target_id(std::optional<Id> target_id) noexcept;
    void clear_target() noexcept;
    void rotate_toward_desired(double delta_seconds) noexcept;
    void set_movement_state(MovementState state) noexcept;
    void set_combat_movement_state(CombatMovementState state) noexcept;

    [[nodiscard]] Id id() const noexcept;
    [[nodiscard]] TroopType troop_type() const noexcept;
    [[nodiscard]] Team team() const noexcept;
    [[nodiscard]] Vec2 position() const noexcept;
    [[nodiscard]] Vec2 previous_position() const noexcept;
    [[nodiscard]] float facing_angle() const noexcept;
    [[nodiscard]] float previous_facing_angle() const noexcept;
    [[nodiscard]] float desired_facing_angle() const noexcept;
    [[nodiscard]] std::optional<Id> target_id() const noexcept;
    [[nodiscard]] float preferred_y() const noexcept;
    [[nodiscard]] float move_speed() const noexcept;
    [[nodiscard]] float rotation_speed() const noexcept;
    [[nodiscard]] float vision_range() const noexcept;
    [[nodiscard]] float vision_angle() const noexcept;
    [[nodiscard]] float awareness_radius() const noexcept;
    [[nodiscard]] float preferred_combat_range() const noexcept;
    [[nodiscard]] float range_tolerance() const noexcept;
    [[nodiscard]] float aggression() const noexcept;
    [[nodiscard]] float retreat_bias() const noexcept;
    [[nodiscard]] MovementState movement_state() const noexcept;
    [[nodiscard]] CombatMovementState combat_movement_state() const noexcept;

private:
    Id id_{};
    TroopType troop_type_{TroopType::rifle};
    Team team_{Team::none};
    Vec2 position_{};
    Vec2 previous_position_{};
    float facing_angle_{};
    float previous_facing_angle_{};
    float desired_facing_angle_{};
    std::optional<Id> target_id_{};
    float preferred_y_{};
    float move_speed_{};
    float rotation_speed_{};
    float vision_range_{};
    float vision_angle_{};
    float awareness_radius_{};
    float preferred_combat_range_{};
    float range_tolerance_{};
    float aggression_{};
    float retreat_bias_{};
    MovementState movement_state_{MovementState::idle};
    CombatMovementState combat_movement_state_{CombatMovementState::advancing};
};

} // namespace siege
