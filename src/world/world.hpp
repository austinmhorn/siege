#pragma once

#include "world/zone.hpp"

#include <array>

namespace siege {

class World {
public:
    static constexpr float width = 1920.0F;
    static constexpr float height = 1080.0F;
    static constexpr std::size_t zone_count = 5;

    World();

    [[nodiscard]] const std::array<Zone, zone_count>& zones() const noexcept;

private:
    std::array<Zone, zone_count> zones_;
};

} // namespace siege

