#pragma once

#include "world/zone.hpp"

#include <cstddef>

namespace siege {

enum class ZoneTransitionType {
    neutralized,
    captured,
};

struct ZoneOwnershipEvent {
    std::size_t zone_id;
    Team previous_owner;
    Team new_owner;
    ZoneTransitionType type;
};

} // namespace siege
