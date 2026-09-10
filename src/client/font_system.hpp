#pragma once

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct SDL_Renderer;
struct SDL_Texture;
struct TTF_Font;

namespace siege {

enum class FontRole : std::uint8_t {
    debug,
    body,
    heading,
    debug_bold,
    body_bold,
    heading_bold,
};

struct FontColor {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::uint8_t alpha{255};

    bool operator==(const FontColor&) const = default;
};

struct Typography {
    static constexpr float debug_size = 8.0F;
    static constexpr float body_size = 10.0F;
    static constexpr float heading_size = 12.0F;
    static constexpr float debug_line_height = 10.0F;
    static constexpr float body_line_height = 13.0F;
};

class FontSystem {
public:
    FontSystem(SDL_Renderer* renderer, const std::filesystem::path& asset_root);
    ~FontSystem();

    FontSystem(const FontSystem&) = delete;
    FontSystem& operator=(const FontSystem&) = delete;

    void begin_frame();
    [[nodiscard]] bool using_dogica_pixel() const noexcept;
    [[nodiscard]] bool draw(float x, float y, std::string_view text,
                            FontRole role, FontColor color) const;

    template <typename... Args>
    [[nodiscard]] bool draw_format(float x, float y, FontRole role,
                                   FontColor color, const char* format,
                                   Args&&... args) const {
        const int length = std::snprintf(nullptr, 0, format,
                                         std::forward<Args>(args)...);
        if (length < 0) {
            return false;
        }
        std::string text(static_cast<std::size_t>(length), '\0');
        std::snprintf(text.data(), text.size() + 1, format,
                      std::forward<Args>(args)...);
        return draw(x, y, text, role, color);
    }

private:
    struct CachedText {
        std::string text;
        FontRole role;
        FontColor color;
        SDL_Texture* texture{};
        float width{};
        float height{};
        std::uint64_t last_used_frame{};
    };

    [[nodiscard]] TTF_Font* font_for(FontRole role) const noexcept;
    void prune_cache();

    SDL_Renderer* renderer_{};
    std::filesystem::path asset_root_;
    bool ttf_initialized_{};
    TTF_Font* regular_debug_{};
    TTF_Font* regular_body_{};
    TTF_Font* regular_heading_{};
    TTF_Font* bold_debug_{};
    TTF_Font* bold_body_{};
    TTF_Font* bold_heading_{};
    mutable std::vector<CachedText> cache_;
    std::uint64_t frame_index_{};
};

} // namespace siege
