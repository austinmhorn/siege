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
               Vec2 position, Vec2 velocity, float maximum_distance) noexcept;

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
};

} // namespace siege
