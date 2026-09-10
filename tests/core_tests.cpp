#include "core/math.hpp"
#include "core/simulation.hpp"
#include "world/unit.hpp"
#include "world/world.hpp"

#include <array>
#include <cmath>
#include <cstdio>

namespace {

bool check(const bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "check failed: %s\n", message);
    }
    return condition;
}

bool near(const float left, const float right, const float tolerance = 0.01F) {
    return std::abs(left - right) <= tolerance;
}

} // namespace

int main() {
    using namespace siege;

    bool passed = true;
    Unit clockwise_wrap{100, TroopType::rifle, Team::team_a, {}, 72.0F, 90.0F,
                        350.0F};
    clockwise_wrap.set_desired_facing_angle(10.0F);
    clockwise_wrap.rotate_toward_desired(0.1);
    passed &= check(near(clockwise_wrap.facing_angle(), 359.0F),
                    "rotation takes the shortest positive wraparound step");

    Unit counterclockwise_wrap{101, TroopType::rifle, Team::team_b, {}, 72.0F,
                               90.0F, 10.0F};
    counterclockwise_wrap.set_desired_facing_angle(350.0F);
    counterclockwise_wrap.rotate_toward_desired(0.1);
    passed &= check(near(counterclockwise_wrap.facing_angle(), 1.0F),
                    "rotation takes the shortest negative wraparound step");

    World world;
    Simulation simulation{world};
    std::array<Vec2, 8> spawn_positions{};
    for (std::size_t index = 0; index < world.units().size(); ++index) {
        spawn_positions[index] = world.units()[index].position();
    }

    simulation.update(1.0 / 60.0);
    for (const auto& unit : world.units()) {
        passed &= check(std::abs(shortest_angle_delta(unit.facing_angle(),
                                                      unit.desired_facing_angle())) > 1.0F,
                        "facing rotates gradually instead of snapping");
    }

    for (int tick = 1; tick < 300; ++tick) {
        simulation.update(1.0 / 60.0);
    }

    for (std::size_t index = 0; index < world.units().size(); ++index) {
        const auto& unit = world.units()[index];
        const float x_delta = unit.position().x - spawn_positions[index].x;
        if (unit.team() == Team::team_a) {
            passed &= check(x_delta > 300.0F, "team_a advances toward increasing x");
        } else {
            passed &= check(x_delta < -300.0F, "team_b advances toward decreasing x");
        }
        passed &= check(std::abs(unit.position().y - unit.preferred_y()) < 55.0F,
                        "unit remains near its spawn preferred_y");
        passed &= check(unit.movement_state() == MovementState::moving,
                        "unit reports moving while advancing");
    }

    const float team_a_spawn_pair_distance =
        length(world.units()[0].position() - world.units()[1].position());
    const float team_b_spawn_pair_distance =
        length(world.units()[4].position() - world.units()[5].position());
    passed &= check(team_a_spawn_pair_distance > 25.0F,
                    "overlapping team_a units softly separate");
    passed &= check(team_b_spawn_pair_distance > 25.0F,
                    "overlapping team_b units softly separate");

    if (!passed) {
        return 1;
    }

    std::printf("core movement checks passed at tick %llu\n", simulation.tick_count());
    return 0;
}
