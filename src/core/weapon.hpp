#pragma once

namespace siege {

enum class WeaponType {
    rifle,
    machine_gun,
    bazooka,
};

struct WeaponDefinition {
    WeaponType type;
    float projectile_speed;
    float fire_interval;
    float range;
    float firing_arc;
    float projectile_max_distance;
};

class Unit;

[[nodiscard]] bool can_fire_at(const Unit& observer,
                               const Unit& target) noexcept;

} // namespace siege
