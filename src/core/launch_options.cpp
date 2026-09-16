#include "core/launch_options.hpp"

#include <optional>

namespace siege {
namespace {

std::optional<AiDifficulty> parse_difficulty(
    const std::string_view value) noexcept {
    if (value == "easy") {
        return AiDifficulty::easy;
    }
    if (value == "medium") {
        return AiDifficulty::medium;
    }
    if (value == "hard") {
        return AiDifficulty::hard;
    }
    return std::nullopt;
}

std::optional<AiPlaystyle> parse_playstyle(
    const std::string_view value) noexcept {
    if (value == "balanced") {
        return AiPlaystyle::balanced;
    }
    if (value == "aggressive") {
        return AiPlaystyle::aggressive;
    }
    if (value == "defensive") {
        return AiPlaystyle::defensive;
    }
    return std::nullopt;
}

} // namespace

LaunchOptionsResult parse_launch_options(
    const std::span<const std::string_view> arguments) {
    LaunchOptionsResult result;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        const std::string_view option = arguments[index];
        if (option != "--ai-difficulty" && option != "--ai-playstyle") {
            result.error = "unknown option: " + std::string{option};
            return result;
        }
        if (++index >= arguments.size()) {
            result.error = "missing value for " + std::string{option};
            return result;
        }
        const std::string_view value = arguments[index];
        if (option == "--ai-difficulty") {
            const auto difficulty = parse_difficulty(value);
            if (!difficulty.has_value()) {
                result.error = "invalid AI difficulty: " + std::string{value};
                return result;
            }
            result.options.ai_difficulty = *difficulty;
        } else {
            const auto playstyle = parse_playstyle(value);
            if (!playstyle.has_value()) {
                result.error = "invalid AI playstyle: " + std::string{value};
                return result;
            }
            result.options.ai_playstyle = *playstyle;
        }
    }
    return result;
}

} // namespace siege
