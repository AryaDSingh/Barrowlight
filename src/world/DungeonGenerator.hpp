#pragma once

#include <string>
#include <vector>

#include "core/Position.hpp"
#include "world/FloorTheme.hpp"
#include "world/Landmark.hpp"
#include "world/Map.hpp"
#include "world/Props.hpp"

namespace engine {

struct DungeonGenerationParams {
    // Only specific floors have a boss fight; every other floor is a pure
    // "clear it, find the door" dungeon. The boss takes the module
    // farthest from the start.
    bool includeBossRoom = true;
    // A sealed vault module, placed in neither the start nor the final cell.
    bool includeVault = false;
    // Chooses the procedural cell styles and the landmark's local name.
    FloorRegion region = FloorRegion::Barracks;
    // Chance that a non-boss floor is built around a landmark set piece.
    float landmarkChance = 0.9f;
    // Once a floor has a landmark: the chance of a second, and (given a
    // second) of a third, each in its own cell and of a different kind.
    float secondLandmarkChance = 0.4f, thirdLandmarkChance = 0.1f;
    // Chance that the floor instead holds one of the very rare events
    // (deep floors only; 0 elsewhere, which draws nothing extra).
    float rareEventChance = 0.f;
    // Chance that an ordinary landmark is the Blood Altar instead (0 draws nothing).
    float bloodAltarChance = 0.f;
    // Share of ordinary cells built procedurally; the rest are hand-made.
    float proceduralShare = 0.5f;
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
    LandmarkKind landmark = LandmarkKind::None;
    Position landmarkAltar{};                 // the solid altar tile; valid if landmark != None
    std::vector<std::pair<LandmarkKind, Position>> extraLandmarks; // a second and third event, with their altars
    // Blocking furniture and statues, already marked in `map` as solid but
    // see-through tiles. Never placed where it would cut off any floor.
    std::vector<Prop> props;
};

// Builds a floor from a 3x3 grid of module-sized cells (see
// DungeonModules.hpp). Most floors are built around a landmark set piece in
// a random cell; every other ordinary cell is either generated (see
// ProceduralModules.hpp) or a hand-made module drawn without repeats from
// the regular pool, randomly mirrored. Adjacent modules connect through 3-wide sockets: a
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
