#pragma once

namespace engine {

enum class TileType {
    Wall,
    Floor,
    Door, // a floor-transition door -- walkable/transparent exactly like Floor (see Tile's own
          // comment); Application is the only thing that treats it specially, by checking
          // tileAt(...).type after a move rather than needing a new field here
    Chasm, // a drop into darkness: not walkable, but you can see (and push things) across it
};

// A single grid cell. `walkable` and `transparent` are stored explicitly
// rather than derived from `type` on every lookup -- they happen to
// correlate 1:1 for the two types that exist today, but won't always (a
// broken wall, a chasm that looks like floor, etc., later), so keeping
// them separate costs two bools and keeps the door open: a chasm, for
// one, is transparent but not walkable.
struct Tile {
    TileType type = TileType::Wall;
    bool walkable = false;
    bool transparent = false;
};

} // namespace engine
