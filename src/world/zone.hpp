#pragma once

#include <cstddef>

namespace siege {

enum class Team {
    none,
    team_a,
    team_b,
};

enum class ZoneType {
    home,
    objective,
};

struct Bounds {
    float x{};
    float y{};
    float width{};
    float height{};
};

class Zone {
public:
    Zone(std::size_t index, Bounds bounds, ZoneType type, Team owner) noexcept;

    [[nodiscard]] std::size_t index() const noexcept;
    [[nodiscard]] const Bounds& bounds() const noexcept;
    [[nodiscard]] ZoneType type() const noexcept;
    [[nodiscard]] Team owner() const noexcept;

private:
    std::size_t index_{};
    Bounds bounds_{};
    ZoneType type_{ZoneType::objective};
    Team owner_{Team::none};
};

} // namespace siege

