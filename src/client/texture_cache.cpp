#include "client/texture_cache.hpp"

#include <SDL3/SDL.h>

#include <cstdio>
#include <utility>

namespace siege {

TextureCache::TextureCache(SDL_Renderer* renderer, std::filesystem::path asset_root)
    : renderer_(renderer), asset_root_(std::move(asset_root)) {}

TextureCache::~TextureCache() {
    for (const auto& [path, texture] : textures_) {
        static_cast<void>(path);
        SDL_DestroyTexture(texture);
    }
}

SDL_Texture* TextureCache::get(const std::filesystem::path& relative_path) {
    const auto key = relative_path.generic_string();
    if (const auto found = textures_.find(key); found != textures_.end()) {
        return found->second;
    }

    const auto full_path = asset_root_ / relative_path;
    SDL_Surface* surface = SDL_LoadPNG(full_path.string().c_str());
    if (surface == nullptr) {
        std::fprintf(stderr, "Could not load PNG '%s': %s\n", full_path.string().c_str(),
                     SDL_GetError());
        textures_.emplace(key, nullptr);
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
    SDL_DestroySurface(surface);
    if (texture == nullptr) {
        std::fprintf(stderr, "Could not create texture '%s': %s\n",
                     full_path.string().c_str(), SDL_GetError());
        textures_.emplace(key, nullptr);
        return nullptr;
    }

    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    textures_.emplace(key, texture);
    return texture;
}

const std::filesystem::path& TextureCache::asset_root() const noexcept {
    return asset_root_;
}

} // namespace siege
