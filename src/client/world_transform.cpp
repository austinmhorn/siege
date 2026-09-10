#include "client/world_transform.hpp"

#include <algorithm>

namespace siege {

WorldTransform::WorldTransform(const float world_width, const float world_height,
                               const int output_width, const int output_height) noexcept
    : world_width_(world_width), world_height_(world_height) {
    const auto drawable_width = static_cast<float>(std::max(output_width, 1));
    const auto drawable_height = static_cast<float>(std::max(output_height, 1));
    scale_ = std::min(drawable_width / world_width_, drawable_height / world_height_);

    viewport_.width = world_width_ * scale_;
    viewport_.height = world_height_ * scale_;
    viewport_.x = (drawable_width - viewport_.width) * 0.5F;
    viewport_.y = (drawable_height - viewport_.height) * 0.5F;
}

Point WorldTransform::world_to_drawable(const Point point) const noexcept {
    return Point{viewport_.x + point.x * scale_, viewport_.y + point.y * scale_};
}

Bounds WorldTransform::world_to_drawable(const Bounds& bounds) const noexcept {
    const auto origin = world_to_drawable(Point{bounds.x, bounds.y});
    return Bounds{origin.x, origin.y, bounds.width * scale_, bounds.height * scale_};
}

std::optional<Point> WorldTransform::drawable_to_world(const Point point) const noexcept {
    const bool inside = point.x >= viewport_.x && point.y >= viewport_.y &&
                        point.x <= viewport_.x + viewport_.width &&
                        point.y <= viewport_.y + viewport_.height;
    if (!inside) {
        return std::nullopt;
    }

    return Point{(point.x - viewport_.x) / scale_,
                 (point.y - viewport_.y) / scale_};
}

const Bounds& WorldTransform::viewport() const noexcept {
    return viewport_;
}

float WorldTransform::scale() const noexcept {
    return scale_;
}

} // namespace siege

