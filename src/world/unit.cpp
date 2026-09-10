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

Unit::Unit(const Id id, const TroopType troop_type, const Team team,
           const Vec2 spawn_position, const float move_speed,
           const float rotation_speed, const float vision_range,
           const float vision_angle, const float awareness_radius,
           const float initial_facing_angle) noexcept
    : id_(id), troop_type_(troop_type), team_(team), position_(spawn_position),
      previous_position_(spawn_position),
      facing_angle_(normalized_angle(initial_facing_angle)),
      previous_facing_angle_(facing_angle_), desired_facing_angle_(facing_angle_),
      preferred_y_(spawn_position.y), move_speed_(move_speed),
      rotation_speed_(rotation_speed), vision_range_(vision_range),
      vision_angle_(vision_angle), awareness_radius_(awareness_radius) {}

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

void Unit::rotate_toward_desired(const double delta_seconds) noexcept {
    const float delta = shortest_angle_delta(facing_angle_, desired_facing_angle_);
    const float maximum_step = rotation_speed_ * static_cast<float>(delta_seconds);
    facing_angle_ = normalized_angle(
        facing_angle_ + std::clamp(delta, -maximum_step, maximum_step));
}

void Unit::set_movement_state(const MovementState state) noexcept {
    movement_state_ = state;
}

Unit::Id Unit::id() const noexcept { return id_; }
TroopType Unit::troop_type() const noexcept { return troop_type_; }
Team Unit::team() const noexcept { return team_; }
Vec2 Unit::position() const noexcept { return position_; }
Vec2 Unit::previous_position() const noexcept { return previous_position_; }
float Unit::facing_angle() const noexcept { return facing_angle_; }
float Unit::previous_facing_angle() const noexcept { return previous_facing_angle_; }
float Unit::desired_facing_angle() const noexcept { return desired_facing_angle_; }
float Unit::preferred_y() const noexcept { return preferred_y_; }
float Unit::move_speed() const noexcept { return move_speed_; }
float Unit::rotation_speed() const noexcept { return rotation_speed_; }
float Unit::vision_range() const noexcept { return vision_range_; }
float Unit::vision_angle() const noexcept { return vision_angle_; }
float Unit::awareness_radius() const noexcept { return awareness_radius_; }
MovementState Unit::movement_state() const noexcept { return movement_state_; }

} // namespace siege
