#include "core/simulation.hpp"

#include "world/world.hpp"

namespace siege {

Simulation::Simulation(World& world) noexcept : world_(world) {}

void Simulation::update(const double fixed_delta_seconds) noexcept {
    // Phase 1 has no changing world state. Keeping the update boundary explicit
    // establishes where deterministic world rules will run in later phases.
    static_cast<void>(world_);
    static_cast<void>(fixed_delta_seconds);
    ++tick_count_;
}

unsigned long long Simulation::tick_count() const noexcept {
    return tick_count_;
}

} // namespace siege

