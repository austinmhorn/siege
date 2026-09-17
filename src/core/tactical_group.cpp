#include "core/tactical_group.hpp"

#include "world/world.hpp"

#include <algorithm>
#include <vector>

namespace siege {
namespace {

std::vector<Unit*> eligible_units(World& world,
                                  const std::span<const Unit::Id> unit_ids,
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
    return units;
}

std::vector<const Unit*> eligible_units(
    const World& world, const std::span<const Unit::Id> unit_ids,
    const Team commanding_team) {
    std::vector<const Unit*> units;
    units.reserve(unit_ids.size());
    for (const Unit::Id id : unit_ids) {
        const Unit* unit = world.find_unit(id);
        if (unit != nullptr && unit->is_alive() &&
            unit->team() == commanding_team) {
            units.push_back(unit);
        }
    }
    std::ranges::sort(units, {}, &Unit::id);
    const auto unique_end = std::ranges::unique(units, {}, &Unit::id).begin();
    units.erase(unique_end, units.end());
    return units;
}

} // namespace

bool can_group_selected_units(const World& world,
                              const std::span<const Unit::Id> unit_ids,
                              const Team commanding_team) {
    return world.match_state().active() &&
           eligible_units(world, unit_ids, commanding_team).size() >= 2;
}

bool can_ungroup_selected_units(const World& world,
                                const std::span<const Unit::Id> unit_ids,
                                const Team commanding_team) {
    if (!world.match_state().active()) {
        return false;
    }
    const auto units = eligible_units(world, unit_ids, commanding_team);
    return std::ranges::any_of(units, [](const Unit* unit) {
        return unit->group_id().has_value();
    });
}

std::optional<Unit::GroupId> group_selected_units(
    World& world, const std::span<const Unit::Id> unit_ids,
    const Team commanding_team) {
    if (!world.match_state().active()) {
        return std::nullopt;
    }
    std::vector<Unit*> units = eligible_units(world, unit_ids, commanding_team);
    if (units.size() < 2) {
        return std::nullopt;
    }

    const Unit::GroupId group_id = world.allocate_tactical_group_id();
    for (Unit* unit : units) {
        unit->set_group_id(group_id);
    }
    cleanup_tactical_groups(world);
    return group_id;
}

std::size_t ungroup_selected_units(
    World& world, const std::span<const Unit::Id> unit_ids,
    const Team commanding_team) {
    if (!world.match_state().active()) {
        return 0;
    }
    const std::vector<Unit*> units =
        eligible_units(world, unit_ids, commanding_team);
    std::size_t changed = 0;
    for (Unit* unit : units) {
        if (unit->group_id().has_value()) {
            unit->clear_group_id();
            ++changed;
        }
    }
    cleanup_tactical_groups(world);
    return changed;
}

std::vector<Unit::Id> tactical_group_members(const World& world,
                                             const Unit::GroupId group_id) {
    std::vector<Unit::Id> members;
    for (const Unit& unit : world.units()) {
        if (unit.is_alive() && unit.group_id() == group_id) {
            members.push_back(unit.id());
        }
    }
    std::ranges::sort(members);
    return members;
}

void cleanup_tactical_groups(World& world) {
    std::vector<Unit::GroupId> living_group_ids;
    living_group_ids.reserve(world.units().size());
    for (Unit& unit : world.units()) {
        if (!unit.is_alive()) {
            unit.clear_group_id();
        } else if (unit.group_id().has_value()) {
            living_group_ids.push_back(*unit.group_id());
        }
    }
    std::ranges::sort(living_group_ids);

    for (Unit& unit : world.units()) {
        if (!unit.group_id().has_value()) {
            continue;
        }
        const auto [first, last] = std::ranges::equal_range(
            living_group_ids, *unit.group_id());
        if (std::distance(first, last) < 2) {
            unit.clear_group_id();
        }
    }
}

} // namespace siege
