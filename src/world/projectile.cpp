#include "world/projectile.hpp"

#include <algorithm>

namespace siege {

Projectile::Projectile(const Id id, const WeaponType weapon_type, const Team team,
                       const Unit::Id source_unit_id, const Vec2 position,
                       const Vec2 velocity, const float maximum_distance,
                       const float damage, const float splash_radius,
                       const float vehicle_damage_multiplier,
                       const ProjectileTrajectory trajectory,
                       const Vec2 impact_position,
                       const float flight_duration,
                       const float splash_damage) noexcept
    : id_(id), weapon_type_(weapon_type), team_(team),
      source_unit_id_(source_unit_id), position_(position),
      previous_position_(position), velocity_(velocity),
      remaining_distance_(std::max(maximum_distance, 0.0F)),
      damage_(std::max(damage, 0.0F)),
      splash_damage_(splash_damage < 0.0F ? std::max(damage, 0.0F)
                                          : std::max(splash_damage, 0.0F)),
      splash_radius_(std::max(splash_radius, 0.0F)),
      vehicle_damage_multiplier_(std::max(vehicle_damage_multiplier, 0.0F)),
      trajectory_(trajectory), launch_position_(position),
      impact_position_(impact_position),
      flight_duration_(std::max(static_cast<double>(flight_duration), 0.0)),
      arc_height_(std::min(180.0F,
                           length(impact_position - position) * 0.20F)) {}

void Projectile::begin_simulation_step() noexcept {
    previous_position_ = position_;
}

void Projectile::advance(const double delta_seconds) noexcept {
    if (is_indirect()) {
        flight_elapsed_ = std::min(
            flight_duration_, flight_elapsed_ + std::max(0.0, delta_seconds));
        if (flight_duration_ - flight_elapsed_ <= 1.0e-9) {
            flight_elapsed_ = flight_duration_;
        }
        const float progress = flight_progress();
        position_ = launch_position_ +
                    (impact_position_ - launch_position_) * progress;
        remaining_distance_ = progress >= 1.0F ? 0.0F :
            length(impact_position_ - position_);
        return;
    }
    Vec2 displacement = velocity_ * static_cast<float>(delta_seconds);
    const float displacement_length = length(displacement);
    if (displacement_length > remaining_distance_ &&
        displacement_length > 0.0001F) {
        displacement = displacement * (remaining_distance_ / displacement_length);
    }
    position_ = position_ + displacement;
    remaining_distance_ =
        std::max(0.0F, remaining_distance_ - length(displacement));
}

Projectile::Id Projectile::id() const noexcept { return id_; }
WeaponType Projectile::weapon_type() const noexcept { return weapon_type_; }
Team Projectile::team() const noexcept { return team_; }
Unit::Id Projectile::source_unit_id() const noexcept { return source_unit_id_; }
Vec2 Projectile::position() const noexcept { return position_; }
Vec2 Projectile::previous_position() const noexcept { return previous_position_; }
Vec2 Projectile::velocity() const noexcept { return velocity_; }
float Projectile::remaining_distance() const noexcept { return remaining_distance_; }
float Projectile::damage() const noexcept { return damage_; }
float Projectile::damage_against(const TargetCategory category) const noexcept {
    return damage_ * (category == TargetCategory::vehicle
                          ? vehicle_damage_multiplier_
                          : 1.0F);
}
float Projectile::splash_damage() const noexcept { return splash_damage_; }
float Projectile::splash_damage_against(
    const TargetCategory category) const noexcept {
    return splash_damage_ * (category == TargetCategory::vehicle
                                 ? vehicle_damage_multiplier_
                                 : 1.0F);
}
float Projectile::splash_radius() const noexcept { return splash_radius_; }
ProjectileTrajectory Projectile::trajectory() const noexcept {
    return trajectory_;
}
bool Projectile::is_indirect() const noexcept {
    return trajectory_ == ProjectileTrajectory::indirect_arc;
}
Vec2 Projectile::impact_position() const noexcept { return impact_position_; }
float Projectile::flight_progress() const noexcept {
    if (!is_indirect()) {
        return 0.0F;
    }
    return flight_duration_ <= 0.0
        ? 1.0F
        : static_cast<float>(
              std::clamp(flight_elapsed_ / flight_duration_, 0.0, 1.0));
}
float Projectile::render_height() const noexcept {
    const float progress = flight_progress();
    return is_indirect() ? 4.0F * arc_height_ * progress * (1.0F - progress)
                         : 0.0F;
}
bool Projectile::expired() const noexcept { return remaining_distance_ <= 0.0F; }

} // namespace siege
