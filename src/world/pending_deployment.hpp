#pragma once

#include "core/math.hpp"
#include "world/unit.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace siege {

struct PendingDeployment {
    using Id = std::uint32_t;

    Id id{};
    Team team{Team::none};
    TroopType troop_type{TroopType::rifle};
    Vec2 position{};
    double total_seconds{};
    double remaining_seconds{};
    std::optional<std::size_t> ai_objective_zone{};
};

} // namespace siege
