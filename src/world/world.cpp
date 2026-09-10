#include "world/world.hpp"

namespace siege {
namespace {

constexpr float zone_width = World::width / static_cast<float>(World::zone_count);

constexpr Bounds zone_bounds(const std::size_t index) noexcept {
    return Bounds{static_cast<float>(index) * zone_width, 0.0F, zone_width,
                  World::height};
}

} // namespace

World::World()
    : zones_{Zone{0, zone_bounds(0), ZoneType::home, Team::team_a},
             Zone{1, zone_bounds(1), ZoneType::objective, Team::none},
             Zone{2, zone_bounds(2), ZoneType::objective, Team::none},
             Zone{3, zone_bounds(3), ZoneType::objective, Team::none},
             Zone{4, zone_bounds(4), ZoneType::home, Team::team_b}} {}

const std::array<Zone, World::zone_count>& World::zones() const noexcept {
    return zones_;
}

} // namespace siege

