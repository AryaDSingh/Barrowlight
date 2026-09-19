#pragma once

namespace engine {

enum class TileType {
    Wall,
    Floor,
};

// A single grid cell. `walkable` is stored explicitly rather than derived
// from `type` on every lookup -- they happen to correlate 1:1 for the two
// types that exist today, but won't always (a broken wall, a chasm that
// looks like floor, etc., later), so keeping them separate now avoids a
// bigger refactor later for the cost of one bool now.
struct Tile {
    TileType type = TileType::Wall;
    bool walkable = false;
};

} // namespace engine
