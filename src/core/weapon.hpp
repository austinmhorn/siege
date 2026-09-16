#pragma once

namespace siege {

enum class WeaponType {
    rifle,
    machine_gun,
    bazooka,
    tank_cannon,
    anti_tank_missile,
};

enum class TargetCategory {
    infantry,
    vehicle,
};

struct WeaponDefinition {
    WeaponType type;
    float projectile_speed;
    float fire_interval;
    float range;
    float firing_arc;
    float projectile_max_distance;
    float projectile_damage;
    float splash_radius;
    float vehicle_damage_multiplier{1.0F};
};

class Unit;

[[nodiscard]] bool can_fire_at(const Unit& observer,
                               const Unit& target) noexcept;

} // namespace siege
