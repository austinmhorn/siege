#include "world/unit.hpp"

#include <algorithm>
#include <cmath>

namespace siege {

std::string_view to_string(const TroopType type) noexcept {
    switch (type) {
    case TroopType::rifle:
        return "rifle";
    case TroopType::machine_gun:
        return "machine_gun";
    case TroopType::bazooka:
        return "bazooka";
    }
    return "unknown";
}

std::string_view to_string(const Team team) noexcept {
    switch (team) {
    case Team::team_a:
        return "team_a";
    case Team::team_b:
        return "team_b";
    case Team::none:
        return "none";
    }
    return "unknown";
}

std::string_view to_string(const MovementState state) noexcept {
    switch (state) {
    case MovementState::idle:
        return "idle";
    case MovementState::moving:
        return "moving";
    }
    return "unknown";
}

std::string_view to_string(const CombatMovementState state) noexcept {
    switch (state) {
    case CombatMovementState::inactive:
        return "inactive";
    case CombatMovementState::advancing:
        return "advancing";
    case CombatMovementState::closing:
        return "closing";
    case CombatMovementState::engaging:
        return "engaging";
    case CombatMovementState::retreating:
        return "retreating";
    }
    return "unknown";
}

Unit::Unit(const Id id, const TroopType troop_type, const Team team,
           const Vec2 spawn_position, const float move_speed,
           const float rotation_speed, const float vision_range,
           const float vision_angle, const float awareness_radius,
           const float preferred_combat_range, const float range_tolerance,
           const float aggression, const float retreat_bias,
           const float frontline_screen_weight,
           const float support_positioning_bias,
           const float support_rear_distance,
           const float support_search_radius,
           const float max_health, const float hit_radius,
           const WeaponDefinition weapon, const float initial_facing_angle) noexcept
    : id_(id), troop_type_(troop_type), team_(team), position_(spawn_position),
      previous_position_(spawn_position),
      facing_angle_(normalized_angle(initial_facing_angle)),
      previous_facing_angle_(facing_angle_), desired_facing_angle_(facing_angle_),
      preferred_y_(spawn_position.y), move_speed_(move_speed),
      rotation_speed_(rotation_speed), vision_range_(vision_range),
      vision_angle_(vision_angle), awareness_radius_(awareness_radius),
      preferred_combat_range_(preferred_combat_range),
      range_tolerance_(range_tolerance), aggression_(aggression),
      retreat_bias_(retreat_bias),
      frontline_screen_weight_(std::max(frontline_screen_weight, 0.0F)),
      support_positioning_bias_(std::max(support_positioning_bias, 0.0F)),
      support_rear_distance_(std::max(support_rear_distance, 0.0F)),
      support_search_radius_(std::max(support_search_radius, 0.0F)),
      weapon_(weapon),
      health_(std::max(max_health, 0.0F)),
      max_health_(std::max(max_health, 0.0F)),
      hit_radius_(std::max(hit_radius, 0.0F)) {}

void Unit::begin_simulation_step() noexcept {
    previous_position_ = position_;
    previous_facing_angle_ = facing_angle_;
}

void Unit::set_position(const Vec2 position) noexcept {
    position_ = position;
}

void Unit::set_desired_facing_angle(const float angle) noexcept {
    desired_facing_angle_ = normalized_angle(angle);
}

void Unit::set_target_id(const std::optional<Id> target_id) noexcept {
    target_id_ = target_id;
}

void Unit::clear_target() noexcept {
    target_id_.reset();
}

void Unit::rotate_toward_desired(const double delta_seconds) noexcept {
    const float delta = shortest_angle_delta(facing_angle_, desired_facing_angle_);
    const float maximum_step = rotation_speed_ * static_cast<float>(delta_seconds);
    facing_angle_ = normalized_angle(
        facing_angle_ + std::clamp(delta, -maximum_step, maximum_step));
}

void Unit::set_movement_state(const MovementState state) noexcept {
    movement_state_ = state;
}

void Unit::set_combat_movement_state(const CombatMovementState state) noexcept {
    combat_movement_state_ = state;
}

void Unit::set_support_positioning(const std::optional<Id> screen_id,
                                   const Vec2 steering) noexcept {
    support_screen_id_ = screen_id;
    support_steering_ = steering;
}

void Unit::tick_weapon_cooldown(const double delta_seconds) noexcept {
    weapon_cooldown_remaining_ =
        std::max(0.0F, weapon_cooldown_remaining_ -
                          static_cast<float>(delta_seconds));
    if (weapon_cooldown_remaining_ <= 0.0001F) {
        weapon_cooldown_remaining_ = 0.0F;
    }
}

void Unit::reset_weapon_cooldown() noexcept {
    weapon_cooldown_remaining_ = std::max(weapon_.fire_interval, 0.0F);
}

void Unit::apply_damage(const float damage) noexcept {
    if (!is_alive() || damage <= 0.0F) {
        return;
    }
    health_ = std::max(0.0F, health_ - damage);
    if (!is_alive()) {
        clear_target();
        movement_state_ = MovementState::idle;
        combat_movement_state_ = CombatMovementState::inactive;
        set_support_positioning(std::nullopt, {});
    }
}

Unit::Id Unit::id() const noexcept { return id_; }
TroopType Unit::troop_type() const noexcept { return troop_type_; }
Team Unit::team() const noexcept { return team_; }
Vec2 Unit::position() const noexcept { return position_; }
Vec2 Unit::previous_position() const noexcept { return previous_position_; }
float Unit::facing_angle() const noexcept { return facing_angle_; }
float Unit::previous_facing_angle() const noexcept { return previous_facing_angle_; }
float Unit::desired_facing_angle() const noexcept { return desired_facing_angle_; }
std::optional<Unit::Id> Unit::target_id() const noexcept { return target_id_; }
float Unit::preferred_y() const noexcept { return preferred_y_; }
float Unit::move_speed() const noexcept { return move_speed_; }
float Unit::rotation_speed() const noexcept { return rotation_speed_; }
float Unit::vision_range() const noexcept { return vision_range_; }
float Unit::vision_angle() const noexcept { return vision_angle_; }
float Unit::awareness_radius() const noexcept { return awareness_radius_; }
float Unit::preferred_combat_range() const noexcept {
    return preferred_combat_range_;
}
float Unit::range_tolerance() const noexcept { return range_tolerance_; }
float Unit::aggression() const noexcept { return aggression_; }
float Unit::retreat_bias() const noexcept { return retreat_bias_; }
float Unit::frontline_screen_weight() const noexcept {
    return frontline_screen_weight_;
}
float Unit::support_positioning_bias() const noexcept {
    return support_positioning_bias_;
}
float Unit::support_rear_distance() const noexcept {
    return support_rear_distance_;
}
float Unit::support_search_radius() const noexcept {
    return support_search_radius_;
}
std::optional<Unit::Id> Unit::support_screen_id() const noexcept {
    return support_screen_id_;
}
Vec2 Unit::support_steering() const noexcept { return support_steering_; }
const WeaponDefinition& Unit::weapon() const noexcept { return weapon_; }
float Unit::weapon_cooldown_remaining() const noexcept {
    return weapon_cooldown_remaining_;
}
float Unit::health() const noexcept { return health_; }
float Unit::max_health() const noexcept { return max_health_; }
float Unit::hit_radius() const noexcept { return hit_radius_; }
bool Unit::is_alive() const noexcept { return health_ > 0.0F; }
MovementState Unit::movement_state() const noexcept { return movement_state_; }
CombatMovementState Unit::combat_movement_state() const noexcept {
    return combat_movement_state_;
}

} // namespace siege
