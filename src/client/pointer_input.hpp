#pragma once

#include <optional>

namespace siege {

enum class PointerButton {
    primary,
    secondary,
};

enum class PointerDispatch {
    ignored,
    primary,
    secondary,
};

// Converts physical pointer buttons and modifiers into an input role while
// remembering the source button for the complete secondary drag lifecycle.
class PointerInputRouter {
public:
    [[nodiscard]] PointerDispatch press(PointerButton button,
                                        bool control_held) noexcept;
    [[nodiscard]] PointerDispatch release(PointerButton button) noexcept;
    [[nodiscard]] bool secondary_active() const noexcept;

private:
    std::optional<PointerButton> secondary_source_;
};

} // namespace siege
