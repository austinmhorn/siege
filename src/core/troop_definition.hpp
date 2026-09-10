#pragma once

#include "world/unit.hpp"

namespace siege {

struct TroopDefinition {
    TroopType type;
    float move_speed;
    float rotation_speed;
};

inline constexpr TroopDefinition rifle_definition{
    .type = TroopType::rifle,
    .move_speed = 72.0F,
    .rotation_speed = 90.0F,
};

} // namespace siege
