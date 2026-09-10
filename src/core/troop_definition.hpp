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
    .weapon = WeaponDefinition{
        .type = WeaponType::rifle,
        .projectile_speed = 960.0F,
        .fire_interval = 0.60F,
        .range = 360.0F,
        .firing_arc = 12.0F,
        .projectile_max_distance = 520.0F,
        .projectile_damage = 25.0F,
    },
};

} // namespace siege
