#pragma once

#include "core/map_definition.hpp"
#include "core/match.hpp"
#include "world/combat_event.hpp"
#include "world/player_state.hpp"
#include "world/pending_deployment.hpp"
#include "world/projectile.hpp"
#include "world/zone.hpp"
#include "world/zone_event.hpp"
#include "world/unit.hpp"

#include <array>
#include <cstdint>
#include <vector>

namespace siege {

class World {
public:
    explicit World(MatchRules match_rules = default_match_rules,
                   const MapDefinition& map = default_map_definition());

    [[nodiscard]] const MapDefinition& map() const noexcept;
    [[nodiscard]] std::span<const Zone> zones() const noexcept;
    [[nodiscard]] std::span<Zone> zones() noexcept;
    [[nodiscard]] const std::array<PlayerState, 2>& players() const noexcept;
    [[nodiscard]] std::array<PlayerState, 2>& players() noexcept;
    [[nodiscard]] const PlayerState* find_player(Team team) const noexcept;
    [[nodiscard]] PlayerState* find_player(Team team) noexcept;
    [[nodiscard]] const MatchState& match_state() const noexcept;
    [[nodiscard]] MatchState& match_state() noexcept;
    [[nodiscard]] std::uint64_t scoring_tick_progress() const noexcept;
    [[nodiscard]] std::uint64_t advance_scoring_clock(
        std::uint64_t tick_count, std::uint32_t interval_ticks) noexcept;
    [[nodiscard]] const std::vector<Unit>& units() const noexcept;
    [[nodiscard]] std::vector<Unit>& units() noexcept;
    [[nodiscard]] const Unit* find_unit(Unit::Id id) const noexcept;
    [[nodiscard]] Unit* find_unit(Unit::Id id) noexcept;
    [[nodiscard]] const std::vector<PendingDeployment>& pending_deployments()
        const noexcept;
    [[nodiscard]] std::vector<PendingDeployment>& pending_deployments() noexcept;
    PendingDeployment& queue_deployment(Team team, TroopType troop_type,
                                        Vec2 position, double total_seconds);
    Unit& spawn_unit(TroopType troop_type, Team team, Vec2 position);
    [[nodiscard]] const std::vector<Projectile>& projectiles() const noexcept;
    [[nodiscard]] std::vector<Projectile>& projectiles() noexcept;
    [[nodiscard]] const std::vector<DeathEvent>& death_events() const noexcept;
    [[nodiscard]] const std::vector<FireEvent>& fire_events() const noexcept;
    [[nodiscard]] const std::vector<ExplosionEvent>& explosion_events() const noexcept;
    [[nodiscard]] const std::vector<ZoneOwnershipEvent>& zone_ownership_events()
        const noexcept;
    [[nodiscard]] std::vector<ZoneOwnershipEvent>& zone_ownership_events()
        noexcept;
    void clear_transient_events() noexcept;
    void reset_for_sudden_death() noexcept;
    void remove_dead_units();
    void emit_fire_event(const Unit& unit);
    Projectile& spawn_projectile(WeaponType weapon_type, Team team,
                                 Unit::Id source_unit_id, Vec2 position,
                                 Vec2 velocity, float maximum_distance,
                                 float damage, float splash_radius = 0.0F,
                                 float vehicle_damage_multiplier = 1.0F);
    Projectile& spawn_indirect_projectile(
        WeaponType weapon_type, Team team, Unit::Id source_unit_id,
        Vec2 position, Vec2 impact_position, float projectile_speed,
        float damage, float splash_radius,
        float vehicle_damage_multiplier = 1.0F);
    void emit_explosion_event(const Projectile& projectile, Vec2 position);
    void emit_zone_ownership_event(std::size_t zone_id, Team previous_owner,
                                   Team new_owner,
                                   ZoneTransitionType type);

private:
    void spawn_test_units();

    const MapDefinition* map_;
    std::vector<Zone> zones_;
    std::array<PlayerState, 2> players_;
    MatchState match_state_;
    std::vector<Unit> units_;
    std::vector<PendingDeployment> pending_deployments_;
    std::vector<Projectile> projectiles_;
    std::vector<DeathEvent> death_events_;
    std::vector<FireEvent> fire_events_;
    std::vector<ExplosionEvent> explosion_events_;
    std::vector<ZoneOwnershipEvent> zone_ownership_events_;
    Unit::Id next_unit_id_{1};
    PendingDeployment::Id next_pending_deployment_id_{1};
    Projectile::Id next_projectile_id_{1};
    std::uint64_t scoring_tick_progress_{};
};

} // namespace siege
