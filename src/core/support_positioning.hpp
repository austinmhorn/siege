#pragma once

#include "core/map_definition.hpp"
#include "world/unit.hpp"

#include <optional>
#include <vector>

namespace siege {

struct SupportPositioning {
    std::optional<Unit::Id> screen_id;
    Vec2 steering;
};

[[nodiscard]] SupportPositioning support_positioning_for(
    const Unit& unit, const std::vector<Unit>& units,
    const MapDefinition& map = default_map_definition()) noexcept;

} // namespace siege
