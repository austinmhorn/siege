#pragma once

#include "world/zone.hpp"

#include <optional>

namespace siege {

struct Point {
    float x{};
    float y{};
};

class WorldTransform {
public:
    WorldTransform(float world_width, float world_height, int output_width,
                   int output_height) noexcept;

    [[nodiscard]] Point world_to_drawable(Point point) const noexcept;
    [[nodiscard]] Bounds world_to_drawable(const Bounds& bounds) const noexcept;
    [[nodiscard]] std::optional<Point> drawable_to_world(Point point) const noexcept;
    [[nodiscard]] const Bounds& viewport() const noexcept;
    [[nodiscard]] float scale() const noexcept;

private:
    float world_width_{};
    float world_height_{};
    float scale_{};
    Bounds viewport_{};
};

} // namespace siege

