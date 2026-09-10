#pragma once

#include "core/weapon.hpp"
#include "world/unit.hpp"

namespace siege {

struct TroopDefinition {
    TroopType type;
    float move_speed;
    float rotation_speed;
    float vision_range;
    float vision_angle;
    float awareness_radius;
    float preferred_combat_range;
    float range_tolerance;
    float aggression;
    float retreat_bias;
    float max_health;
    float hit_radius;
    float zone_control_weight;
    WeaponDefinition weapon;
};

inline constexpr TroopDefinition rifle_definition{
    .type = TroopType::rifle,
    .move_speed = 72.0F,
    .rotation_speed = 90.0F,
    .vision_range = 500.0F,
    .vision_angle = 90.0F,
    .awareness_radius = 110.0F,
    .preferred_combat_range = 280.0F,
    .range_tolerance = 35.0F,
    .aggression = 0.9F,
    .retreat_bias = 0.75F,
    .max_health = 100.0F,
    .hit_radius = 20.0F,
    .zone_control_weight = 1.0F,
    .weapon = WeaponDefinition{
        .type = WeaponType::rifle,
        .projectile_speed = 960.0F,
        .fire_interval = 0.60F,
        .range = 360.0F,
        .firing_arc = 12.0F,
        .projectile_max_distance = 520.0F,
        .projectile_damage = 25.0F,
        .splash_radius = 0.0F,
    },
};

inline constexpr TroopDefinition machine_gun_definition{
    .type = TroopType::machine_gun,
    .move_speed = 54.0F,
    .rotation_speed = 60.0F,
    .vision_range = 600.0F,
    .vision_angle = 80.0F,
    .awareness_radius = 110.0F,
    .preferred_combat_range = 390.0F,
    .range_tolerance = 45.0F,
    .aggression = 0.72F,
    .retreat_bias = 0.65F,
    .max_health = 100.0F,
    .hit_radius = 22.0F,
    .zone_control_weight = 1.0F,
    .weapon = WeaponDefinition{
        .type = WeaponType::machine_gun,
        .projectile_speed = 1100.0F,
        .fire_interval = 0.18F,
        .range = 480.0F,
        .firing_arc = 14.0F,
        .projectile_max_distance = 650.0F,
        .projectile_damage = 10.0F,
        .splash_radius = 0.0F,
    },
};

inline constexpr TroopDefinition bazooka_definition{
    .type = TroopType::bazooka,
    .move_speed = 60.0F,
    .rotation_speed = 50.0F,
    .vision_range = 760.0F,
    .vision_angle = 75.0F,
    .awareness_radius = 110.0F,
    .preferred_combat_range = 520.0F,
    .range_tolerance = 55.0F,
    .aggression = 0.68F,
    .retreat_bias = 0.70F,
    .max_health = 80.0F,
    .hit_radius = 20.0F,
    .zone_control_weight = 1.0F,
    .weapon = WeaponDefinition{
        .type = WeaponType::bazooka,
        .projectile_speed = 480.0F,
        .fire_interval = 2.60F,
        .range = 650.0F,
        .firing_arc = 10.0F,
        .projectile_max_distance = 760.0F,
        .projectile_damage = 70.0F,
        .splash_radius = 115.0F,
    },
};

} // namespace siege
