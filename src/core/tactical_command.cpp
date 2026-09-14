#include "core/tactical_command.hpp"

#include "world/world.hpp"

#include <algorithm>
#include <vector>

namespace siege {

std::size_t apply_tactical_order(World& world,
                                 const std::span<const Unit::Id> unit_ids,
                                 const TacticalOrder order,
                                 const Team commanding_team) {
    std::vector<Unit*> units;
    units.reserve(unit_ids.size());
    for (const Unit::Id id : unit_ids) {
        Unit* unit = world.find_unit(id);
        if (unit != nullptr && unit->is_alive() &&
            unit->team() == commanding_team) {
            units.push_back(unit);
        }
    }
    std::ranges::sort(units, {}, &Unit::id);
    const auto unique_end = std::ranges::unique(units, {}, &Unit::id).begin();
    units.erase(unique_end, units.end());
    if (units.empty()) {
        return 0;
    }

    Vec2 shared_target{};
    if (order == TacticalOrder::regroup) {
        double sum_x = 0.0;
        double sum_y = 0.0;
        for (const Unit* unit : units) {
            sum_x += unit->position().x;
            sum_y += unit->position().y;
        }
        shared_target = {
            static_cast<float>(sum_x / static_cast<double>(units.size())),
            static_cast<float>(sum_y / static_cast<double>(units.size())),
        };
    }

    for (Unit* unit : units) {
        if (order == TacticalOrder::hold) {
            unit->set_tactical_order(order, unit->position());
        } else if (order == TacticalOrder::regroup) {
            unit->set_tactical_order(order, shared_target);
        } else {
            unit->set_tactical_order(order);
        }
    }
    return units.size();
}

} // namespace siege
