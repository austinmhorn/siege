#include "world/zone.hpp"

#include <algorithm>

namespace siege {

Zone::Zone(const std::size_t index, const Bounds bounds, const ZoneType type,
           const Team owner) noexcept
    : index_(index), bounds_(bounds), type_(type), owner_(owner) {}

std::size_t Zone::index() const noexcept {
    return index_;
}

const Bounds& Zone::bounds() const noexcept {
    return bounds_;
}

ZoneType Zone::type() const noexcept {
    return type_;
}

Team Zone::owner() const noexcept {
    return owner_;
}

int Zone::team_a_count() const noexcept { return team_a_count_; }

int Zone::team_b_count() const noexcept { return team_b_count_; }

int Zone::pressure() const noexcept { return team_a_count_ - team_b_count_; }

float Zone::capture_value() const noexcept { return capture_value_; }

void Zone::clear_presence() noexcept {
    team_a_count_ = 0;
    team_b_count_ = 0;
}

void Zone::add_presence(const Team team) noexcept {
    if (team == Team::team_a) {
        ++team_a_count_;
    } else if (team == Team::team_b) {
        ++team_b_count_;
    }
}

void Zone::advance_capture(const float amount) noexcept {
    if (type_ == ZoneType::objective) {
        capture_value_ = std::clamp(capture_value_ + amount, -100.0F, 100.0F);
    }
}

void Zone::set_owner(const Team owner) noexcept {
    if (type_ == ZoneType::objective) {
        owner_ = owner;
    }
}

} // namespace siege
