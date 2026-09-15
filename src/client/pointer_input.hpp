#pragma once

#include "client/world_transform.hpp"

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

enum class SecondaryGesture {
    click,
    drag,
};

inline constexpr float secondary_drag_threshold = 8.0F;

[[nodiscard]] SecondaryGesture classify_secondary_gesture(
    Point press, Point release,
    float drag_threshold = secondary_drag_threshold) noexcept;

[[nodiscard]] bool should_begin_individual_path(
    PointerDispatch dispatch, bool deployment_active,
    bool living_team_a_unit_hit) noexcept;

// Converts physical pointer buttons and modifiers into an input role while
// remembering the source button for the complete secondary drag lifecycle.
class PointerInputRouter {
public:
    [[nodiscard]] PointerDispatch press(PointerButton button,
                                        bool control_held) noexcept;
    [[nodiscard]] PointerDispatch release(PointerButton button) noexcept;
    [[nodiscard]] bool secondary_active() const noexcept;
    void cancel() noexcept;

private:
    std::optional<PointerButton> secondary_source_;
};

} // namespace siege
