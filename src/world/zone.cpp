#include "world/zone.hpp"

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

} // namespace siege

