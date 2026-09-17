#include "core/tactical_command.hpp"

#include "world/world.hpp"

#include <algorithm>
#include <vector>

namespace siege {
namespace {

Vec2 hold_anchor_in_bounds(const Vec2 position, const Bounds& bounds) noexcept {
    const float maximum_inset =
        std::max(0.0F, std::min(bounds.width, bounds.height) * 0.5F);
    const float inset =
        std::min(default_tactical_rules.hold_anchor_inset, maximum_inset);
    const float minimum_x = bounds.x + inset;
    const float maximum_x = bounds.x + bounds.width - inset;
    const float minimum_y = bounds.y + inset;
    const float maximum_y = bounds.y + bounds.height - inset;
    return Vec2{std::clamp(position.x, minimum_x, maximum_x),
                std::clamp(position.y, minimum_y, maximum_y)};
}

} // namespace

std::size_t apply_tactical_order(World& world,
                                 const std::span<const Unit::Id> unit_ids,
                                 const TacticalOrder order,
                                 const Team commanding_team,
                                 const std::optional<Bounds> hold_bounds) {
    if (!world.match_state().active()) {
        return 0;
    }
    std::vector<Unit*> units;
    units.reserve(unit_ids.size());
    for (const Unit::Id id : unit_ids) {
        Unit* unit = world.find_unit(id);
        if (unit != nullptr && unit->is_alive() &&
            unit->team() == commanding_team &&
            unit->mobility_mode() != MobilityMode::player_path_only) {
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
            const Vec2 anchor = hold_bounds.has_value()
                ? hold_anchor_in_bounds(unit->position(), *hold_bounds)
                : unit->position();
            unit->set_tactical_order(order, anchor);
        } else if (order == TacticalOrder::regroup) {
            unit->set_tactical_order(order, shared_target);
        } else {
            unit->set_tactical_order(order);
        }
    }
    return units.size();
}

} // namespace siege
