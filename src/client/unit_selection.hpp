#pragma once

#include "core/math.hpp"
#include "world/unit.hpp"

#include <cstddef>
#include <vector>

namespace siege {

class World;

// Client-owned selection state. Rectangle inputs are world coordinates and
// rectangle edges are inclusive, making reversed drags and boundaries
// deterministic without introducing SDL types into the query.
class UnitSelection {
public:
    void replace_from_rectangle(const World& world, Vec2 first, Vec2 second);
    void prune(const World& world);
    void clear() noexcept;

    [[nodiscard]] bool contains(Unit::Id id) const noexcept;
    [[nodiscard]] const std::vector<Unit::Id>& ids() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;

private:
    std::vector<Unit::Id> ids_;
};

} // namespace siege
