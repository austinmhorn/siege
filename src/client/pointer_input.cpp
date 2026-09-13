#include "client/pointer_input.hpp"

namespace siege {

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
