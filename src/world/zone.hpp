#pragma once

#include <cstddef>

namespace siege {

enum class Team {
    none,
    team_a,
    team_b,
};

enum class ZoneType {
    home,
    objective,
};

struct Bounds {
    float x{};
    float y{};
    float width{};
    float height{};
};

class Zone {
public:
    Zone(std::size_t index, Bounds bounds, ZoneType type, Team owner) noexcept;

    [[nodiscard]] std::size_t index() const noexcept;
    [[nodiscard]] const Bounds& bounds() const noexcept;
    [[nodiscard]] ZoneType type() const noexcept;
    [[nodiscard]] Team owner() const noexcept;
    [[nodiscard]] int team_a_count() const noexcept;
    [[nodiscard]] int team_b_count() const noexcept;
    [[nodiscard]] int pressure() const noexcept;
    [[nodiscard]] float capture_value() const noexcept;
    [[nodiscard]] bool occupied() const noexcept;
    [[nodiscard]] bool contested() const noexcept;
    [[nodiscard]] double secure_timer_seconds() const noexcept;
    [[nodiscard]] bool secured() const noexcept;

    void clear_presence() noexcept;
    void add_presence(Team team) noexcept;
    void advance_capture(float amount) noexcept;
    void set_owner(Team owner) noexcept;
    void update_security(double delta_seconds,
                         double required_seconds) noexcept;

private:
    std::size_t index_{};
    Bounds bounds_{};
    ZoneType type_{ZoneType::objective};
    Team owner_{Team::none};
    int team_a_count_{};
    int team_b_count_{};
    float capture_value_{};
    double secure_timer_seconds_{};
    bool secured_{};
};

} // namespace siege
