#pragma once

#include "world/unit.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace siege {

class World;

[[nodiscard]] bool can_group_selected_units(
    const World& world, std::span<const Unit::Id> unit_ids,
    Team commanding_team);

[[nodiscard]] bool can_ungroup_selected_units(
    const World& world, std::span<const Unit::Id> unit_ids,
    Team commanding_team);

[[nodiscard]] std::optional<Unit::GroupId> group_selected_units(
    World& world, std::span<const Unit::Id> unit_ids,
    Team commanding_team);

[[nodiscard]] std::size_t ungroup_selected_units(
    World& world, std::span<const Unit::Id> unit_ids,
    Team commanding_team);

[[nodiscard]] std::vector<Unit::Id> tactical_group_members(
    const World& world, Unit::GroupId group_id);

void cleanup_tactical_groups(World& world);

} // namespace siege
