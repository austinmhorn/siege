#include "client/unit_selection.hpp"

#include "world/world.hpp"

#include <algorithm>

namespace siege {
namespace {

bool inside_inclusive(const Vec2 position, const Vec2 first,
                      const Vec2 second) noexcept {
    const float left = std::min(first.x, second.x);
    const float right = std::max(first.x, second.x);
    const float top = std::min(first.y, second.y);
    const float bottom = std::max(first.y, second.y);
    return position.x >= left && position.x <= right && position.y >= top &&
           position.y <= bottom;
}

} // namespace

void UnitSelection::replace_from_rectangle(const World& world, const Vec2 first,
                                           const Vec2 second) {
    ids_.clear();
    for (const Unit& unit : world.units()) {
        if (unit.is_alive() && unit.team() == Team::team_a &&
            inside_inclusive(unit.position(), first, second)) {
            ids_.push_back(unit.id());
        }
    }
}

void UnitSelection::prune(const World& world) {
    std::erase_if(ids_, [&world](const Unit::Id id) {
        const Unit* unit = world.find_unit(id);
        return unit == nullptr || !unit->is_alive() ||
               unit->team() != Team::team_a;
    });
}

void UnitSelection::clear() noexcept {
    ids_.clear();
}

bool UnitSelection::contains(const Unit::Id id) const noexcept {
    return std::find(ids_.begin(), ids_.end(), id) != ids_.end();
}

const std::vector<Unit::Id>& UnitSelection::ids() const noexcept {
    return ids_;
}

std::size_t UnitSelection::size() const noexcept {
    return ids_.size();
}

} // namespace siege
