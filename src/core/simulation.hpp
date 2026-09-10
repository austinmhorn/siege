#pragma once

namespace siege {

class World;

class Simulation {
public:
    explicit Simulation(World& world) noexcept;

    void update(double fixed_delta_seconds) noexcept;
    [[nodiscard]] unsigned long long tick_count() const noexcept;

private:
    World& world_;
    unsigned long long tick_count_{};
};

} // namespace siege

