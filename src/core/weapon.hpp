#pragma once

namespace siege {

enum class WeaponType {
    rifle,
    machine_gun,
    bazooka,
    tank_cannon,
    anti_tank_missile,
    mortar_shell,
};

enum class ProjectileTrajectory {
    direct,
    indirect_arc,
};

enum class WeaponRangeMode {
    fixed,
    map_diagonal,
};

enum class TargetCategory {
    infantry,
    vehicle,
};

struct WeaponDefinition {
    WeaponType type;
    float projectile_speed;
    float fire_interval;
    float minimum_range{0.0F};
    float range;
    float firing_arc;
    float projectile_max_distance;
    float projectile_damage;
    float splash_radius;
    float splash_damage{0.0F};
    float vehicle_damage_multiplier{1.0F};
    ProjectileTrajectory trajectory{ProjectileTrajectory::direct};
    WeaponRangeMode maximum_range_mode{WeaponRangeMode::fixed};
};

class Unit;
struct MapDefinition;

[[nodiscard]] float effective_weapon_range(
    const MapDefinition& map, const WeaponDefinition& weapon) noexcept;

[[nodiscard]] bool can_fire_at(const Unit& observer,
                               const Unit& target) noexcept;
[[nodiscard]] bool can_fire_at(const MapDefinition& map, const Unit& observer,
                               const Unit& target) noexcept;

} // namespace siege
