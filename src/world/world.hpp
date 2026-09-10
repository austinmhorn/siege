#pragma once

#include "world/combat_event.hpp"
#include "world/projectile.hpp"
#include "world/zone.hpp"
#include "world/zone_event.hpp"
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
    [[nodiscard]] std::array<Zone, zone_count>& zones() noexcept;
    [[nodiscard]] const std::vector<Unit>& units() const noexcept;
    [[nodiscard]] std::vector<Unit>& units() noexcept;
    [[nodiscard]] const Unit* find_unit(Unit::Id id) const noexcept;
    [[nodiscard]] Unit* find_unit(Unit::Id id) noexcept;
    [[nodiscard]] const std::vector<Projectile>& projectiles() const noexcept;
    [[nodiscard]] std::vector<Projectile>& projectiles() noexcept;
    [[nodiscard]] const std::vector<DeathEvent>& death_events() const noexcept;
    [[nodiscard]] const std::vector<FireEvent>& fire_events() const noexcept;
    [[nodiscard]] const std::vector<ExplosionEvent>& explosion_events() const noexcept;
    [[nodiscard]] const std::vector<ZoneOwnershipEvent>& zone_ownership_events()
        const noexcept;
    void clear_transient_events() noexcept;
    void remove_dead_units();
    void emit_fire_event(const Unit& unit);
    Projectile& spawn_projectile(WeaponType weapon_type, Team team,
                                 Unit::Id source_unit_id, Vec2 position,
                                 Vec2 velocity, float maximum_distance,
                                 float damage, float splash_radius = 0.0F);
    void emit_explosion_event(const Projectile& projectile, Vec2 position);
    void emit_zone_ownership_event(std::size_t zone_id, Team previous_owner,
                                   Team new_owner,
                                   ZoneTransitionType type);

private:
    void spawn_test_units();

    std::array<Zone, zone_count> zones_;
    std::vector<Unit> units_;
    std::vector<Projectile> projectiles_;
    std::vector<DeathEvent> death_events_;
    std::vector<FireEvent> fire_events_;
    std::vector<ExplosionEvent> explosion_events_;
    std::vector<ZoneOwnershipEvent> zone_ownership_events_;
    Unit::Id next_unit_id_{1};
    Projectile::Id next_projectile_id_{1};
};

} // namespace siege
