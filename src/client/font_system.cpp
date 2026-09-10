#include "client/font_system.hpp"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace siege {
namespace {

constexpr std::size_t maximum_cached_textures = 512;

} // namespace

FontSystem::FontSystem(SDL_Renderer* renderer,
                       const std::filesystem::path& asset_root)
    : renderer_(renderer), asset_root_(asset_root) {
    ttf_initialized_ = TTF_Init();
    if (!ttf_initialized_) {
        std::fprintf(stderr, "SDL_ttf initialization failed; using SDL debug font: %s\n",
                     SDL_GetError());
        return;
    }

    const auto regular_path = asset_root_ / "fonts/dogica/dogicapixel.ttf";
    const auto bold_path = asset_root_ / "fonts/dogica/dogicapixelbold.ttf";
    const auto open = [](const std::filesystem::path& path, const float size) {
        return TTF_OpenFont(path.string().c_str(), size);
    };
    regular_debug_ = open(regular_path, Typography::debug_size);
    regular_body_ = open(regular_path, Typography::body_size);
    regular_heading_ = open(regular_path, Typography::heading_size);
    bold_debug_ = open(bold_path, Typography::debug_size);
    bold_body_ = open(bold_path, Typography::body_size);
    bold_heading_ = open(bold_path, Typography::heading_size);

    if (!using_dogica_pixel()) {
        std::fprintf(stderr,
                     "Dogica Pixel runtime fonts unavailable under %s; using SDL debug font fallback\n",
                     (asset_root_ / "fonts/dogica").string().c_str());
    }
}

FontSystem::~FontSystem() {
    for (auto& entry : cache_) {
        SDL_DestroyTexture(entry.texture);
    }
    for (TTF_Font* font : {regular_debug_, regular_body_, regular_heading_,
                           bold_debug_, bold_body_, bold_heading_}) {
        if (font != nullptr) {
            TTF_CloseFont(font);
        }
    }
    if (ttf_initialized_) {
        TTF_Quit();
    }
}

void FontSystem::begin_frame() {
    ++frame_index_;
    prune_cache();
}

bool FontSystem::using_dogica_pixel() const noexcept {
    return regular_debug_ != nullptr && regular_body_ != nullptr &&
           regular_heading_ != nullptr && bold_debug_ != nullptr &&
           bold_body_ != nullptr && bold_heading_ != nullptr;
}

TTF_Font* FontSystem::font_for(const FontRole role) const noexcept {
    switch (role) {
    case FontRole::debug:
        return regular_debug_;
    case FontRole::body:
        return regular_body_;
    case FontRole::heading:
        return regular_heading_;
    case FontRole::debug_bold:
        return bold_debug_;
    case FontRole::body_bold:
        return bold_body_;
    case FontRole::heading_bold:
        return bold_heading_;
    }
    return nullptr;
}

bool FontSystem::draw(const float x, const float y, const std::string_view text,
                      const FontRole role, const FontColor color) const {
    TTF_Font* font = font_for(role);
    if (font == nullptr) {
        SDL_SetRenderDrawColor(renderer_, color.red, color.green, color.blue,
                               color.alpha);
        return SDL_RenderDebugText(renderer_, std::round(x), std::round(y),
                                   std::string{text}.c_str());
    }

    auto cached = std::find_if(cache_.begin(), cache_.end(),
                               [text, role, color](const CachedText& entry) {
                                   return entry.role == role && entry.color == color &&
                                          entry.text == text;
                               });
    if (cached == cache_.end()) {
        const SDL_Color sdl_color{color.red, color.green, color.blue, color.alpha};
        SDL_Surface* surface =
            TTF_RenderText_Solid(font, text.data(), text.size(), sdl_color);
        if (surface == nullptr) {
            return false;
        }
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
        const float width = static_cast<float>(surface->w);
        const float height = static_cast<float>(surface->h);
        SDL_DestroySurface(surface);
        if (texture == nullptr ||
            !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)) {
            SDL_DestroyTexture(texture);
            return false;
        }
        cache_.push_back(CachedText{std::string{text}, role, color, texture,
                                    width, height, frame_index_});
        cached = std::prev(cache_.end());
    }
    cached->last_used_frame = frame_index_;
    const SDL_FRect destination{std::round(x), std::round(y), cached->width,
                                cached->height};
    return SDL_RenderTexture(renderer_, cached->texture, nullptr, &destination);
}

void FontSystem::prune_cache() {
    if (cache_.size() <= maximum_cached_textures) {
        return;
    }
    std::erase_if(cache_, [this](CachedText& entry) {
        if (entry.last_used_frame + 2 >= frame_index_) {
            return false;
        }
        SDL_DestroyTexture(entry.texture);
        return true;
    });
    while (cache_.size() > maximum_cached_textures) {
        auto oldest = std::min_element(
            cache_.begin(), cache_.end(), [](const CachedText& left,
                                             const CachedText& right) {
                return left.last_used_frame < right.last_used_frame;
            });
        SDL_DestroyTexture(oldest->texture);
        cache_.erase(oldest);
    }
}

} // namespace siege
