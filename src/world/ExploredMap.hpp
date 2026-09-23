#pragma once

#include <vector>

#include "core/Position.hpp"
#include "world/Map.hpp"

namespace engine {

enum class Visibility {
    Hidden,      // never seen
    Remembered,  // seen before, not currently visible
    Visible,     // currently visible
};

// Tracks, per tile, whether it's ever been seen and whether it's
// currently visible. Pure bookkeeping -- no FOV math of its own (see
// computeFieldOfView for that); this just remembers what that function's
// results were over time.
class ExploredMap {
public:
    // Default-constructs empty (0x0) -- same reasoning as Map's default
    // constructor (see ARCHITECTURE_DECISIONS.md): lets this be a plain
    // member, assigned its real size once the Map it's tracking exists.
    ExploredMap() = default;

    explicit ExploredMap(const Map& map);

    // Marks the given positions as currently Visible (and implicitly now
    // explored). Anything that was Visible before this call and isn't in
    // `visible` this time drops to Remembered. Call this once per turn
    // with a fresh computeFieldOfView() result.
    void update(const std::vector<Position>& visible);

    // Directly replaces every tile's visibility with `values` (row-major,
    // same indexing as internal storage). Used only by save/load to
    // restore exact fog-of-war state; normal gameplay uses update()
    // instead, never this.
    void restoreAll(std::vector<Visibility> values);

    Visibility at(int x, int y) const;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<Visibility> visibility_;
};

} // namespace engine
