#pragma once

#include "core/math.hpp"
#include "core/weapon.hpp"
#include "world/unit.hpp"

#include <cstdint>

namespace siege {

struct DeathEvent {
    Unit::Id unit_id;
    TroopType troop_type;
    Team team;
    Vec2 position;
    float facing_angle;
};

struct FireEvent {
    Unit::Id unit_id;
    TroopType troop_type;
    WeaponType weapon_type;
};

struct ExplosionEvent {
    std::uint64_t projectile_id;
    WeaponType weapon_type;
    Team team;
    Vec2 position;
    float radius;
};

} // namespace siege
