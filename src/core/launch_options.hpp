#pragma once

#include "core/ai_commander.hpp"

#include <span>
#include <string>
#include <string_view>

namespace siege {

struct LaunchOptions {
    AiDifficulty ai_difficulty{AiDifficulty::medium};
    AiPlaystyle ai_playstyle{AiPlaystyle::balanced};
};

struct LaunchOptionsResult {
    LaunchOptions options{};
    std::string error{};

    [[nodiscard]] explicit operator bool() const noexcept {
        return error.empty();
    }
};

[[nodiscard]] LaunchOptionsResult parse_launch_options(
    std::span<const std::string_view> arguments);

} // namespace siege
