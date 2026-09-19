#pragma once

namespace engine {

enum class TileType {
    Wall,
    Floor,
};

// A single grid cell. `walkable` and `transparent` are stored explicitly
// rather than derived from `type` on every lookup -- they happen to
// correlate 1:1 for the two types that exist today, but won't always (a
// broken wall, a chasm that looks like floor, etc., later), so keeping
// them separate now avoids a bigger refactor later for the cost of two
// bools now. `transparent` is the field explicitly deferred from Prompt
// 5 ("held off on transparent until Prompt 6 needs it") -- this is that
// moment.
struct Tile {
    TileType type = TileType::Wall;
    bool walkable = false;
    bool transparent = false;
};

} // namespace engine
