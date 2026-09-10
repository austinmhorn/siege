#pragma once

#include "world/unit.hpp"

namespace siege {

struct TroopDefinition {
    TroopType type;
    float move_speed;
    float rotation_speed;
    float vision_range;
    float vision_angle;
    float awareness_radius;
};

inline constexpr TroopDefinition rifle_definition{
    .type = TroopType::rifle,
    .move_speed = 72.0F,
    .rotation_speed = 90.0F,
    .vision_range = 500.0F,
    .vision_angle = 90.0F,
    .awareness_radius = 110.0F,
};

} // namespace siege
