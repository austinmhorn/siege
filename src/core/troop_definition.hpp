#pragma once

#include "core/weapon.hpp"
#include "world/player_state.hpp"
#include "world/unit.hpp"

#include <string_view>

namespace siege {

struct TroopDefinition {
    TroopType type;
    std::string_view display_name;
    float move_speed;
    float rotation_speed;
    float vision_range;
    float vision_angle;
    float awareness_radius;
    float preferred_combat_range;
    float range_tolerance;
    float aggression;
    float retreat_bias;
    float frontline_screen_weight;
    float support_positioning_bias;
    float support_rear_distance;
    float support_search_radius;
    float max_health;
    float hit_radius;
    TargetCategory target_category;
    bool prefers_vehicle_targets;
    bool independent_turret;
    float turret_rotation_speed;
    float zone_control_weight;
    Money purchase_cost;
    double deployment_seconds;
    WeaponDefinition weapon;
};

inline constexpr TroopDefinition rifle_definition{
    .type = TroopType::rifle,
    .display_name = "Rifleman",
    .move_speed = 72.0F,
    .rotation_speed = 90.0F,
    .vision_range = 500.0F,
    .vision_angle = 90.0F,
    .awareness_radius = 110.0F,
    .preferred_combat_range = 280.0F,
    .range_tolerance = 35.0F,
    .aggression = 0.9F,
    .retreat_bias = 0.75F,
    .frontline_screen_weight = 1.0F,
    .support_positioning_bias = 0.0F,
    .support_rear_distance = 0.0F,
    .support_search_radius = 0.0F,
    .max_health = 100.0F,
    .hit_radius = 20.0F,
    .target_category = TargetCategory::infantry,
    .prefers_vehicle_targets = false,
    .independent_turret = false,
    .turret_rotation_speed = 0.0F,
    .zone_control_weight = 1.0F,
    .purchase_cost = 2500,
    .deployment_seconds = 0.75,
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
    .display_name = "Machine Gun",
    .move_speed = 54.0F,
    .rotation_speed = 60.0F,
    .vision_range = 600.0F,
    .vision_angle = 80.0F,
    .awareness_radius = 110.0F,
    .preferred_combat_range = 390.0F,
    .range_tolerance = 45.0F,
    .aggression = 0.62F,
    .retreat_bias = 0.85F,
    .frontline_screen_weight = 0.0F,
    .support_positioning_bias = 0.85F,
    .support_rear_distance = 120.0F,
    .support_search_radius = 420.0F,
    .max_health = 100.0F,
    .hit_radius = 22.0F,
    .target_category = TargetCategory::infantry,
    .prefers_vehicle_targets = false,
    .independent_turret = false,
    .turret_rotation_speed = 0.0F,
    .zone_control_weight = 1.0F,
    .purchase_cost = 4000,
    .deployment_seconds = 1.25,
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
    .display_name = "Bazooka",
    .move_speed = 60.0F,
    .rotation_speed = 50.0F,
    .vision_range = 760.0F,
    .vision_angle = 75.0F,
    .awareness_radius = 110.0F,
    .preferred_combat_range = 520.0F,
    .range_tolerance = 55.0F,
    .aggression = 0.50F,
    .retreat_bias = 1.0F,
    .frontline_screen_weight = 0.0F,
    .support_positioning_bias = 1.10F,
    .support_rear_distance = 180.0F,
    .support_search_radius = 500.0F,
    .max_health = 80.0F,
    .hit_radius = 20.0F,
    .target_category = TargetCategory::infantry,
    .prefers_vehicle_targets = false,
    .independent_turret = false,
    .turret_rotation_speed = 0.0F,
    .zone_control_weight = 1.0F,
    .purchase_cost = 6000,
    .deployment_seconds = 1.75,
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

inline constexpr TroopDefinition medium_tank_definition{
    .type = TroopType::medium_tank,
    .display_name = "Medium Tank",
    .move_speed = 36.0F,
    .rotation_speed = 35.0F,
    .vision_range = 550.0F,
    .vision_angle = 90.0F,
    .awareness_radius = 120.0F,
    .preferred_combat_range = 520.0F,
    .range_tolerance = 50.0F,
    .aggression = 0.55F,
    .retreat_bias = 0.90F,
    .frontline_screen_weight = 0.0F,
    .support_positioning_bias = 0.70F,
    .support_rear_distance = 90.0F,
    .support_search_radius = 500.0F,
    .max_health = 600.0F,
    .hit_radius = 34.0F,
    .target_category = TargetCategory::vehicle,
    .prefers_vehicle_targets = false,
    .independent_turret = true,
    .turret_rotation_speed = 55.0F,
    .zone_control_weight = 1.0F,
    .purchase_cost = 12'000,
    .deployment_seconds = 3.0,
    .weapon = WeaponDefinition{
        .type = WeaponType::tank_cannon,
        .projectile_speed = 620.0F,
        .fire_interval = 2.4F,
        .range = 650.0F,
        .firing_arc = 6.0F,
        .projectile_max_distance = 760.0F,
        .projectile_damage = 120.0F,
        .splash_radius = 90.0F,
    },
};

inline constexpr TroopDefinition anti_tank_definition{
    .type = TroopType::anti_tank,
    .display_name = "Anti-Tank",
    .move_speed = 58.0F,
    .rotation_speed = 60.0F,
    .vision_range = 800.0F,
    .vision_angle = 75.0F,
    .awareness_radius = 110.0F,
    .preferred_combat_range = 580.0F,
    .range_tolerance = 45.0F,
    .aggression = 0.42F,
    .retreat_bias = 1.10F,
    .frontline_screen_weight = 0.0F,
    .support_positioning_bias = 1.20F,
    .support_rear_distance = 210.0F,
    .support_search_radius = 560.0F,
    .max_health = 80.0F,
    .hit_radius = 20.0F,
    .target_category = TargetCategory::infantry,
    .prefers_vehicle_targets = true,
    .independent_turret = false,
    .turret_rotation_speed = 0.0F,
    .zone_control_weight = 1.0F,
    .purchase_cost = 7'000,
    .deployment_seconds = 2.0,
    .weapon = WeaponDefinition{
        .type = WeaponType::anti_tank_missile,
        .projectile_speed = 560.0F,
        .fire_interval = 3.0F,
        .range = 700.0F,
        .firing_arc = 8.0F,
        .projectile_max_distance = 800.0F,
        .projectile_damage = 60.0F,
        .splash_radius = 45.0F,
        .vehicle_damage_multiplier = 3.5F,
    },
};

[[nodiscard]] constexpr const TroopDefinition* troop_definition_for(
    const TroopType type) noexcept {
    switch (type) {
    case TroopType::rifle:
        return &rifle_definition;
    case TroopType::machine_gun:
        return &machine_gun_definition;
    case TroopType::bazooka:
        return &bazooka_definition;
    case TroopType::medium_tank:
        return &medium_tank_definition;
    case TroopType::anti_tank:
        return &anti_tank_definition;
    }
    return nullptr;
}

[[nodiscard]] constexpr std::string_view troop_display_name(
    const TroopType type) noexcept {
    const TroopDefinition* definition = troop_definition_for(type);
    return definition == nullptr ? std::string_view{"Unknown"}
                                 : definition->display_name;
}

} // namespace siege
