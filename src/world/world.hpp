#pragma once

#include "world/projectile.hpp"
#include "world/zone.hpp"
#include "world/unit.hpp"

#include <array>
#include <vector>

namespace siege {

class World {
public:
    static constexpr float width = 1920.0F;
    static constexpr float height = 1080.0F;
    static constexpr std::size_t zone_count = 5;

    World();

    [[nodiscard]] const std::array<Zone, zone_count>& zones() const noexcept;
    [[nodiscard]] const std::vector<Unit>& units() const noexcept;
    [[nodiscard]] std::vector<Unit>& units() noexcept;
    [[nodiscard]] const Unit* find_unit(Unit::Id id) const noexcept;
    [[nodiscard]] Unit* find_unit(Unit::Id id) noexcept;
    [[nodiscard]] const std::vector<Projectile>& projectiles() const noexcept;
    [[nodiscard]] std::vector<Projectile>& projectiles() noexcept;
    Projectile& spawn_projectile(WeaponType weapon_type, Team team,
                                 Unit::Id source_unit_id, Vec2 position,
                                 Vec2 velocity, float maximum_distance,
                                 float damage);

private:
    void spawn_test_units();

    std::array<Zone, zone_count> zones_;
    std::vector<Unit> units_;
    std::vector<Projectile> projectiles_;
    Unit::Id next_unit_id_{1};
    Projectile::Id next_projectile_id_{1};
};

} // namespace siege
