#include "client/pointer_input.hpp"

#include <algorithm>

namespace siege {

SecondaryGesture classify_secondary_gesture(const Point press,
                                             const Point release,
                                             const float drag_threshold) noexcept {
    const float threshold = std::max(0.0F, drag_threshold);
    const float delta_x = release.x - press.x;
    const float delta_y = release.y - press.y;
    return delta_x * delta_x + delta_y * delta_y > threshold * threshold
        ? SecondaryGesture::drag
        : SecondaryGesture::click;
}

bool should_begin_individual_path(const PointerDispatch dispatch,
                                  const bool deployment_active,
                                  const bool living_team_a_unit_hit) noexcept {
    return dispatch == PointerDispatch::primary && !deployment_active &&
           living_team_a_unit_hit;
}

PointerDispatch PointerInputRouter::press(const PointerButton button,
                                          const bool control_held) noexcept {
    if (secondary_source_.has_value()) {
        return PointerDispatch::ignored;
    }
    if (button == PointerButton::secondary ||
        (button == PointerButton::primary && control_held)) {
        secondary_source_ = button;
        return PointerDispatch::secondary;
    }
    return PointerDispatch::primary;
}

PointerDispatch PointerInputRouter::release(const PointerButton button) noexcept {
    if (secondary_source_.has_value()) {
        if (*secondary_source_ != button) {
            return PointerDispatch::ignored;
        }
        secondary_source_.reset();
        return PointerDispatch::secondary;
    }
    return button == PointerButton::primary ? PointerDispatch::primary
                                            : PointerDispatch::ignored;
}

bool PointerInputRouter::secondary_active() const noexcept {
    return secondary_source_.has_value();
}

} // namespace siege
