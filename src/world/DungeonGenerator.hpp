#pragma once

#include <string>
#include <vector>

#include "core/Position.hpp"
#include "world/Map.hpp"

namespace engine {

struct DungeonGenerationParams {
    // Only specific floors have a boss fight; every other floor is a pure
    // "clear it, find the door" dungeon. The boss takes the module
    // farthest from the start.
    bool includeBossRoom = true;
    // A sealed vault module, placed in neither the start nor the final cell.
    bool includeVault = false;
};

struct GeneratedDungeon {
    Map map;
    bool hasVault = false;
    Position vaultCenter{}, vaultEntrance{};
    Position playerStart;                    // the start module's first anchor
    std::vector<Position> otherRoomCenters;   // every other module's encounter anchors, nearest
                                               // module first; never the boss module. On a
                                               // non-boss floor the last one is in the farthest
                                               // module and hosts the door down.
    bool hasBossRoom = false;
    Position bossRoomCenter;                  // valid only if hasBossRoom
    int roomCount = 0;                        // modules on the floor
    std::vector<std::string> moduleNames;     // row-major, for debugging/printing
};

// Builds a floor from a 3x3 grid of hand-authored modules (see
// DungeonModules.hpp), drawn without repeats from the regular pool and
// randomly mirrored. Adjacent modules connect through 3-wide sockets: a
// random spanning tree of the grid guarantees the floor is one connected
// space, then most remaining neighbor pairs are opened too, so there are
// usually several routes between modules and monsters can arrive from
// more than one side. Sockets that lead nowhere are sealed.
//
// The player starts in a random edge cell; the cell farthest from it (by
// steps through open sockets) holds the boss or, on ordinary floors, the
// door down.
//
// Deterministic given the same seed.
GeneratedDungeon generateDungeon(const DungeonGenerationParams& params, unsigned int seed);

} // namespace engine
