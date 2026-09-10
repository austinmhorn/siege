#pragma once

#include "core/math.hpp"
#include "world/unit.hpp"

#include <cstdint>

namespace siege {

struct PendingDeployment {
    using Id = std::uint32_t;

    Id id{};
    Team team{Team::none};
    TroopType troop_type{TroopType::rifle};
    Vec2 position{};
    double total_seconds{};
    double remaining_seconds{};
};

} // namespace siege
