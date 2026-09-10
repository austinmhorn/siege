#pragma once

#include <algorithm>
#include <cmath>

namespace siege {

struct Vec2 {
    float x{};
    float y{};
};

[[nodiscard]] inline Vec2 operator+(const Vec2 left, const Vec2 right) noexcept {
    return Vec2{left.x + right.x, left.y + right.y};
}

[[nodiscard]] inline Vec2 operator-(const Vec2 left, const Vec2 right) noexcept {
    return Vec2{left.x - right.x, left.y - right.y};
}

[[nodiscard]] inline Vec2 operator*(const Vec2 vector, const float scalar) noexcept {
    return Vec2{vector.x * scalar, vector.y * scalar};
}

[[nodiscard]] inline float length_squared(const Vec2 vector) noexcept {
    return vector.x * vector.x + vector.y * vector.y;
}

[[nodiscard]] inline float length(const Vec2 vector) noexcept {
    return std::sqrt(length_squared(vector));
}

[[nodiscard]] inline Vec2 normalized(const Vec2 vector) noexcept {
    const float magnitude = length(vector);
    if (magnitude <= 0.0001F) {
        return Vec2{};
    }
    return vector * (1.0F / magnitude);
}

[[nodiscard]] inline float normalized_angle(float degrees) noexcept {
    degrees = std::fmod(degrees, 360.0F);
    return degrees < 0.0F ? degrees + 360.0F : degrees;
}

[[nodiscard]] inline float shortest_angle_delta(const float from,
                                                const float to) noexcept {
    return std::remainder(to - from, 360.0F);
}

[[nodiscard]] inline float lerp(const float from, const float to,
                                const float alpha) noexcept {
    return from + (to - from) * std::clamp(alpha, 0.0F, 1.0F);
}

[[nodiscard]] inline Vec2 lerp(const Vec2 from, const Vec2 to,
                              const float alpha) noexcept {
    return Vec2{lerp(from.x, to.x, alpha), lerp(from.y, to.y, alpha)};
}

[[nodiscard]] inline float lerp_angle(const float from, const float to,
                                     const float alpha) noexcept {
    return normalized_angle(from + shortest_angle_delta(from, to) *
                                      std::clamp(alpha, 0.0F, 1.0F));
}

} // namespace siege
