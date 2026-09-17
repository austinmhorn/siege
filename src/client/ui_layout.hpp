#pragma once

#include "core/math.hpp"

#include <algorithm>
#include <cstddef>

namespace siege::ui_layout {

inline constexpr float reference_width = 1920.0F;
inline constexpr float reference_height = 1080.0F;
inline constexpr float minimum_scale = 1.0F;
inline constexpr float maximum_scale = 4.0F / 3.0F;
inline constexpr float deployment_hud_magnification = 1.20F;
inline constexpr std::size_t deployment_card_count = 8;
inline constexpr float deployment_bar_height = 132.0F;
inline constexpr float deployment_button_width = 160.0F;
inline constexpr float deployment_button_height = 104.0F;
inline constexpr float deployment_button_gap = 12.0F;
inline constexpr float deployment_minimum_button_gap = 6.0F;
inline constexpr float deployment_group_margin = 12.0F;
inline constexpr float deployment_button_top_inset = 14.0F;
inline constexpr float deployment_card_padding = 6.0F;
inline constexpr float deployment_portrait_height = 59.0F;
inline constexpr float deployment_portrait_text_gap = 2.0F;
inline constexpr float deployment_name_detail_gap = 15.0F;
inline constexpr float deployment_cash_gap = 8.0F;
inline constexpr float deployment_cash_horizontal_padding = 10.0F;
inline constexpr float deployment_cash_vertical_padding = 5.0F;
inline constexpr float deployment_selection_border_width = 2.0F;
inline constexpr float deployment_full_text_width = 140.0F;
inline constexpr float deployment_minimum_text_scale = 0.85F;
inline constexpr float deployment_full_detail_width = 118.0F;
inline constexpr float deployment_minimum_detail_scale = 0.70F;

struct DeploymentLayout {
    float responsive_scale;
    float scale;
    float text_scale;
    float detail_text_scale;
    float bar_height;
    float button_width;
    float button_height;
    float button_gap;
    float button_top_inset;
    float card_padding;
    float portrait_height;
    float portrait_text_gap;
    float name_detail_gap;
    float cash_gap;
    float cash_horizontal_padding;
    float cash_vertical_padding;
    float selection_border_width;
    float group_left;

    [[nodiscard]] constexpr Bounds button_bounds(
        const std::size_t index, const float output_height) const noexcept {
        return Bounds{
            group_left + static_cast<float>(index) *
                             (button_width + button_gap),
            output_height - bar_height + button_top_inset,
            button_width,
            button_height,
        };
    }

    [[nodiscard]] constexpr Bounds portrait_bounds(
        const std::size_t index, const float output_height) const noexcept {
        const Bounds button = button_bounds(index, output_height);
        return Bounds{
            button.x + card_padding,
            button.y + card_padding,
            std::max(0.0F, button.width - card_padding * 2.0F),
            portrait_height,
        };
    }

    [[nodiscard]] constexpr float name_y(
        const std::size_t index, const float output_height) const noexcept {
        const Bounds portrait = portrait_bounds(index, output_height);
        return portrait.y + portrait.height + portrait_text_gap;
    }

    [[nodiscard]] constexpr float detail_y(
        const std::size_t index, const float output_height) const noexcept {
        return name_y(index, output_height) + name_detail_gap;
    }
};

[[nodiscard]] inline DeploymentLayout deployment_layout(
    const int output_width, const int output_height,
    const std::size_t card_count = deployment_card_count) noexcept {
    const float width = static_cast<float>(std::max(output_width, 1));
    const float height = static_cast<float>(std::max(output_height, 1));
    const float reference_scale =
        std::min(width / reference_width, height / reference_height);
    const float responsive_scale =
        std::clamp(reference_scale, minimum_scale, maximum_scale);
    const float scale = responsive_scale * deployment_hud_magnification;
    const float margin = deployment_group_margin * scale;
    const float desired_gap = deployment_button_gap * scale;
    const float minimum_gap =
        deployment_minimum_button_gap * responsive_scale;
    const float desired_button_width = deployment_button_width * scale;
    float gap = desired_gap;
    if (card_count > 1) {
        const float width_after_cards_and_margins =
            width - margin * 2.0F -
            desired_button_width * static_cast<float>(card_count);
        gap = std::clamp(
            width_after_cards_and_margins /
                static_cast<float>(card_count - 1),
            minimum_gap, desired_gap);
    }
    const float gap_total = card_count > 1
        ? gap * static_cast<float>(card_count - 1)
        : 0.0F;
    const float available_width = std::max(1.0F, width - margin * 2.0F - gap_total);
    const float button_width = card_count == 0
        ? 0.0F
        : std::min(desired_button_width,
                   available_width / static_cast<float>(card_count));
    const float text_scale = std::clamp(
        scale * button_width / deployment_full_text_width,
        deployment_minimum_text_scale, scale);
    const float detail_text_scale = std::clamp(
        scale * button_width / deployment_full_detail_width,
        deployment_minimum_detail_scale, text_scale);
    const float total_width = button_width * static_cast<float>(card_count) +
                              gap_total;

    return DeploymentLayout{
        .responsive_scale = responsive_scale,
        .scale = scale,
        // Preserve the HUD magnification until narrow cards would clip their
        // titles, then compress only toward the readable reference scale.
        .text_scale = text_scale,
        .detail_text_scale = detail_text_scale,
        .bar_height = deployment_bar_height * scale,
        .button_width = button_width,
        .button_height = deployment_button_height * scale,
        .button_gap = gap,
        .button_top_inset = deployment_button_top_inset * scale,
        .card_padding = deployment_card_padding * scale,
        .portrait_height = deployment_portrait_height * scale,
        .portrait_text_gap = deployment_portrait_text_gap * scale,
        .name_detail_gap = deployment_name_detail_gap * scale,
        .cash_gap = deployment_cash_gap * scale,
        .cash_horizontal_padding =
            deployment_cash_horizontal_padding * scale,
        .cash_vertical_padding = deployment_cash_vertical_padding * scale,
        .selection_border_width =
            deployment_selection_border_width * scale,
        .group_left = (width - total_width) * 0.5F,
    };
}

} // namespace siege::ui_layout
