#include "core/environment_line_of_sight.hpp"

#include "core/projectile_collision.hpp"

namespace siege {

bool environment_line_of_sight_clear(const MapDefinition& map,
                                     const Vec2 observer,
                                     const Vec2 target) noexcept {
    for (const EnvironmentObjectDefinition& object : map.environment_objects) {
        if (object.physical.blocks_line_of_sight &&
            swept_bounds_hit_fraction(observer, target,
                                      object.footprint).has_value()) {
            return false;
        }
    }
    return true;
}

} // namespace siege
