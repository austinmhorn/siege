#include "world/projectile.hpp"

#include <algorithm>

namespace siege {

Projectile::Projectile(const Id id, const WeaponType weapon_type, const Team team,
                       const Unit::Id source_unit_id, const Vec2 position,
                       const Vec2 velocity, const float maximum_distance) noexcept
    : id_(id), weapon_type_(weapon_type), team_(team),
      source_unit_id_(source_unit_id), position_(position),
      previous_position_(position), velocity_(velocity),
      remaining_distance_(std::max(maximum_distance, 0.0F)) {}

void Projectile::begin_simulation_step() noexcept {
    previous_position_ = position_;
}

void Projectile::advance(const double delta_seconds) noexcept {
    const Vec2 displacement = velocity_ * static_cast<float>(delta_seconds);
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
bool Projectile::expired() const noexcept { return remaining_distance_ <= 0.0F; }

} // namespace siege
