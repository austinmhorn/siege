#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>

struct SDL_Renderer;
struct SDL_Texture;

namespace siege {

class TextureCache {
public:
    TextureCache(SDL_Renderer* renderer, std::filesystem::path asset_root);
    ~TextureCache();

    TextureCache(const TextureCache&) = delete;
    TextureCache& operator=(const TextureCache&) = delete;

    [[nodiscard]] SDL_Texture* get(const std::filesystem::path& relative_path);
    [[nodiscard]] const std::filesystem::path& asset_root() const noexcept;

private:
    SDL_Renderer* renderer_{};
    std::filesystem::path asset_root_;
    std::unordered_map<std::string, SDL_Texture*> textures_;
};

} // namespace siege
