#pragma once

#include "core/math.hpp"
#include "core/weapon.hpp"
#include "world/unit.hpp"

#include <cstdint>

namespace siege {

class Projectile {
public:
    using Id = std::uint64_t;

    Projectile(Id id, WeaponType weapon_type, Team team, Unit::Id source_unit_id,
               Vec2 position, Vec2 velocity, float maximum_distance,
               float damage, float splash_radius,
               float vehicle_damage_multiplier,
               ProjectileTrajectory trajectory = ProjectileTrajectory::direct,
               Vec2 impact_position = {},
               float flight_duration = 0.0F,
               float splash_damage = -1.0F) noexcept;

    void begin_simulation_step() noexcept;
    void advance(double delta_seconds) noexcept;

    [[nodiscard]] Id id() const noexcept;
    [[nodiscard]] WeaponType weapon_type() const noexcept;
    [[nodiscard]] Team team() const noexcept;
    [[nodiscard]] Unit::Id source_unit_id() const noexcept;
    [[nodiscard]] Vec2 position() const noexcept;
    [[nodiscard]] Vec2 previous_position() const noexcept;
    [[nodiscard]] Vec2 velocity() const noexcept;
    [[nodiscard]] float remaining_distance() const noexcept;
    [[nodiscard]] float damage() const noexcept;
    [[nodiscard]] float damage_against(TargetCategory category) const noexcept;
    [[nodiscard]] float splash_damage() const noexcept;
    [[nodiscard]] float splash_damage_against(
        TargetCategory category) const noexcept;
    [[nodiscard]] float splash_radius() const noexcept;
    [[nodiscard]] ProjectileTrajectory trajectory() const noexcept;
    [[nodiscard]] bool is_indirect() const noexcept;
    [[nodiscard]] Vec2 impact_position() const noexcept;
    [[nodiscard]] float flight_progress() const noexcept;
    [[nodiscard]] float render_height() const noexcept;
    [[nodiscard]] bool expired() const noexcept;

private:
    Id id_{};
    WeaponType weapon_type_{WeaponType::rifle};
    Team team_{Team::none};
    Unit::Id source_unit_id_{};
    Vec2 position_{};
    Vec2 previous_position_{};
    Vec2 velocity_{};
    float remaining_distance_{};
    float damage_{};
    float splash_damage_{};
    float splash_radius_{};
    float vehicle_damage_multiplier_{1.0F};
    ProjectileTrajectory trajectory_{ProjectileTrajectory::direct};
    Vec2 launch_position_{};
    Vec2 impact_position_{};
    double flight_duration_{};
    double flight_elapsed_{};
    float arc_height_{};
};

} // namespace siege
