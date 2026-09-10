#include "core/simulation.hpp"

#include "core/targeting.hpp"
#include "world/world.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace siege {
namespace {

constexpr float separation_radius = 80.0F;
constexpr float separation_weight = 0.9F;
constexpr float preferred_y_scale = 120.0F;
constexpr float maximum_y_correction = 0.45F;
constexpr float world_margin = 32.0F;

struct MotionIntent {
    Vec2 velocity;
    float desired_facing;
    MovementState state;
};

Vec2 separation_for(const Unit& unit, const std::vector<Unit>& units) noexcept {
    Vec2 separation{};
    for (const auto& other : units) {
        if (other.id() == unit.id() || other.team() != unit.team()) {
            continue;
        }

        Vec2 offset = unit.position() - other.position();
        float distance = length(offset);
        if (distance >= separation_radius) {
            continue;
        }

        if (distance <= 0.0001F) {
            offset = Vec2{0.0F, unit.id() < other.id() ? -1.0F : 1.0F};
            distance = 0.0F;
        } else {
            offset = offset * (1.0F / distance);
        }

        const float influence = 1.0F - distance / separation_radius;
        separation = separation + offset * influence;
    }
    return separation * separation_weight;
}

} // namespace

Simulation::Simulation(World& world) noexcept : world_(world) {}

void Simulation::update(const double fixed_delta_seconds) noexcept {
    auto& units = world_.units();
    for (auto& unit : units) {
        unit.begin_simulation_step();
    }

    std::vector<std::optional<Unit::Id>> target_ids;
    target_ids.reserve(units.size());
    for (const auto& unit : units) {
        target_ids.push_back(select_target(unit, units));
    }

    std::vector<MotionIntent> intents;
    intents.reserve(units.size());
    for (std::size_t index = 0; index < units.size(); ++index) {
        const auto& unit = units[index];
        const float advance_x = unit.team() == Team::team_a ? 1.0F : -1.0F;
        const bool reached_edge =
            (unit.team() == Team::team_a && unit.position().x >= World::width - world_margin) ||
            (unit.team() == Team::team_b && unit.position().x <= world_margin);
        Vec2 velocity{};
        float desired_facing = unit.facing_angle();
        MovementState state = MovementState::idle;
        if (unit.team() != Team::none && !reached_edge) {
            const float y_error = unit.preferred_y() - unit.position().y;
            Vec2 steering{
                advance_x,
                std::clamp(y_error / preferred_y_scale, -maximum_y_correction,
                           maximum_y_correction),
            };
            steering = steering + separation_for(unit, units);
            const Vec2 direction = normalized(steering);
            velocity = direction * unit.move_speed();
            desired_facing = facing_from_direction(direction);
            state = MovementState::moving;
        }

        if (target_ids[index].has_value()) {
            const Unit* target = world_.find_unit(*target_ids[index]);
            const Vec2 target_direction = target->position() - unit.position();
            if (length_squared(target_direction) > 0.0001F) {
                desired_facing = facing_from_direction(target_direction);
            }
        }
        intents.push_back(MotionIntent{velocity, desired_facing, state});
    }

    for (std::size_t index = 0; index < units.size(); ++index) {
        auto& unit = units[index];
        const auto& intent = intents[index];
        const auto next_position =
            unit.position() + intent.velocity * static_cast<float>(fixed_delta_seconds);
        unit.set_position(Vec2{
            std::clamp(next_position.x, world_margin, World::width - world_margin),
            std::clamp(next_position.y, world_margin, World::height - world_margin),
        });
        unit.set_target_id(target_ids[index]);
        unit.set_desired_facing_angle(intent.desired_facing);
        unit.rotate_toward_desired(fixed_delta_seconds);
        unit.set_movement_state(intent.state);
    }

    ++tick_count_;
}

unsigned long long Simulation::tick_count() const noexcept {
    return tick_count_;
}

} // namespace siege
