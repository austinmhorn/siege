#pragma once

#include "core/math.hpp"
#include "core/weapon.hpp"
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
    inactive,
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
         float frontline_screen_weight, float support_positioning_bias,
         float support_rear_distance, float support_search_radius,
         float max_health, float hit_radius, WeaponDefinition weapon,
         float initial_facing_angle) noexcept;

    void begin_simulation_step() noexcept;
    void set_position(Vec2 position) noexcept;
    void set_desired_facing_angle(float angle) noexcept;
    void set_target_id(std::optional<Id> target_id) noexcept;
    void clear_target() noexcept;
    void rotate_toward_desired(double delta_seconds) noexcept;
    void set_movement_state(MovementState state) noexcept;
    void set_combat_movement_state(CombatMovementState state) noexcept;
    void set_support_positioning(std::optional<Id> screen_id,
                                 Vec2 steering) noexcept;
    void tick_weapon_cooldown(double delta_seconds) noexcept;
    void reset_weapon_cooldown() noexcept;
    void apply_damage(float damage) noexcept;

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
    [[nodiscard]] float frontline_screen_weight() const noexcept;
    [[nodiscard]] float support_positioning_bias() const noexcept;
    [[nodiscard]] float support_rear_distance() const noexcept;
    [[nodiscard]] float support_search_radius() const noexcept;
    [[nodiscard]] std::optional<Id> support_screen_id() const noexcept;
    [[nodiscard]] Vec2 support_steering() const noexcept;
    [[nodiscard]] const WeaponDefinition& weapon() const noexcept;
    [[nodiscard]] float weapon_cooldown_remaining() const noexcept;
    [[nodiscard]] float health() const noexcept;
    [[nodiscard]] float max_health() const noexcept;
    [[nodiscard]] float hit_radius() const noexcept;
    [[nodiscard]] bool is_alive() const noexcept;
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
    float frontline_screen_weight_{};
    float support_positioning_bias_{};
    float support_rear_distance_{};
    float support_search_radius_{};
    std::optional<Id> support_screen_id_{};
    Vec2 support_steering_{};
    WeaponDefinition weapon_{};
    float weapon_cooldown_remaining_{};
    float health_{};
    float max_health_{};
    float hit_radius_{};
    MovementState movement_state_{MovementState::idle};
    CombatMovementState combat_movement_state_{CombatMovementState::advancing};
};

} // namespace siege
