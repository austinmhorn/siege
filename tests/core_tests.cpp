#include "core/math.hpp"
#include "core/perception.hpp"
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

siege::Unit test_unit(const siege::Unit::Id id, const siege::Team team,
                      const siege::Vec2 position, const float facing) {
    return siege::Unit{id, siege::TroopType::rifle, team, position,
                       72.0F, 90.0F, 500.0F, 90.0F, 110.0F, facing};
}

} // namespace

int main() {
    using namespace siege;

    bool passed = true;
    Unit clockwise_wrap = test_unit(100, Team::team_a, {}, 350.0F);
    clockwise_wrap.set_desired_facing_angle(10.0F);
    clockwise_wrap.rotate_toward_desired(0.1);
    passed &= check(near(clockwise_wrap.facing_angle(), 359.0F),
                    "rotation takes the shortest positive wraparound step");

    Unit counterclockwise_wrap = test_unit(101, Team::team_b, {}, 10.0F);
    counterclockwise_wrap.set_desired_facing_angle(350.0F);
    counterclockwise_wrap.rotate_toward_desired(0.1);
    passed &= check(near(counterclockwise_wrap.facing_angle(), 1.0F),
                    "rotation takes the shortest negative wraparound step");

    const Unit observer = test_unit(200, Team::team_a, {100.0F, 100.0F}, 270.0F);
    const Unit ahead = test_unit(201, Team::team_b, {300.0F, 100.0F}, 90.0F);
    passed &= check(inside_vision_cone(observer, ahead),
                    "ahead and in range is inside vision cone");
    passed &= check(can_perceive(observer, ahead),
                    "ahead and in range is perceptible");

    const Unit out_of_range =
        test_unit(202, Team::team_b, {601.0F, 100.0F}, 90.0F);
    passed &= check(!inside_vision_cone(observer, out_of_range),
                    "ahead and out of range is outside vision cone");
    passed &= check(!can_perceive(observer, out_of_range),
                    "ahead and out of all ranges is not perceptible");

    const Unit outside_angle =
        test_unit(203, Team::team_b, {200.0F, 300.0F}, 90.0F);
    passed &= check(!inside_vision_cone(observer, outside_angle),
                    "target outside cone angle is not visible through cone");

    const Unit behind = test_unit(204, Team::team_b, {0.0F, 100.0F}, 90.0F);
    passed &= check(!inside_vision_cone(observer, behind),
                    "target behind observer is not visible through cone");
    passed &= check(inside_awareness_radius(observer, behind),
                    "target behind and inside awareness radius is aware");
    passed &= check(can_perceive(observer, behind),
                    "awareness makes a close target behind perceptible");

    const Unit wrap_observer = test_unit(205, Team::team_a, {}, 359.0F);
    const Vec2 wrap_position = direction_from_facing(1.0F) * 200.0F;
    const Unit wrap_target = test_unit(206, Team::team_b, wrap_position, 180.0F);
    passed &= check(inside_vision_cone(wrap_observer, wrap_target),
                    "vision cone handles zero-degree wraparound");

    const Unit range_boundary =
        test_unit(207, Team::team_b, {600.0F, 100.0F}, 90.0F);
    passed &= check(inside_vision_cone(observer, range_boundary),
                    "vision range boundary is inclusive");
    const Vec2 cone_boundary_position =
        observer.position() + direction_from_facing(315.0F) * 300.0F;
    const Unit cone_boundary =
        test_unit(208, Team::team_b, cone_boundary_position, 90.0F);
    passed &= check(inside_vision_cone(observer, cone_boundary),
                    "vision angular boundary is inclusive");
    const Unit awareness_boundary =
        test_unit(209, Team::team_b, {-10.0F, 100.0F}, 90.0F);
    passed &= check(inside_awareness_radius(observer, awareness_boundary),
                    "awareness radius boundary is inclusive");

    const Unit friendly = test_unit(210, Team::team_a, {200.0F, 100.0F}, 90.0F);
    passed &= check(!can_perceive(observer, friendly),
                    "perception queries reject friendly units");

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
