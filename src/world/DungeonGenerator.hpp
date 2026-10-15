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
    bool cathedral = false; // the Drowned Cathedral: its rooms stay as they are
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

// A lived-in room: the goblins' mess, armoury and bedding, a spider nest, a
// shaman's idol, a library, an ossuary, or grave-robbers digging in the crypts.
enum class Vignette { None, Mess, Armoury, Dormitory, Nest, Shrine, Library, Ossuary, GraveDig };

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
    int roomCount = 0;                        // rooms on the floor (a big room counts once)
    std::vector<std::string> moduleNames;     // per grid cell, row-major ("Solid Rock" where
                                               // there is none), for debugging/printing
    std::vector<int> cellRooms;               // per grid cell: which room it belongs to, or -1
                                               // for solid rock; a big room's two cells share one
    std::vector<Position> doorways;           // the tiles of every doorway between rooms
    LandmarkKind landmark = LandmarkKind::None;
    Position landmarkAltar{};                 // the solid altar tile; valid if landmark != None
    std::vector<std::pair<LandmarkKind, Position>> extraLandmarks; // a second and third event, with their altars
    // Blocking furniture and statues, already marked in `map` as solid but
    // see-through tiles. Never placed where it would cut off any floor.
    std::vector<Prop> props;
    // What each encounter room is for (aligned with otherRoomCenters), and
    // the things left lying about in it. EncounterPlan fills a dressed room
    // with the crowd that belongs there.
    std::vector<Vignette> roomVignettes;
    std::vector<Decal> decals;
};

// Builds a floor from a 3x3 grid of module-sized cells (see
// DungeonModules.hpp). Up to two cells are solid rock, so a floor can be an
// L, a U or a ring, and up to two pairs of neighbouring cells merge into big
// rooms. Most floors are built around a landmark set piece in a random
// cell; every other room is either generated (see ProceduralModules.hpp) or
// a hand-made module drawn without repeats, randomly mirrored. Neighbouring
// rooms connect through 3-wide doorways: a random spanning tree guarantees
// the floor is one connected space, then most remaining neighbor pairs are
// opened too, so there are usually several routes and monsters can arrive
// from more than one side. A doorway may sit off the middle of its edge;
// where it opens onto a hand-made room's wall, the shortest passage in is
// carved. Set pieces keep their doorways in the middle.
//
// The player starts in a random cell at the edge of the grid; the room
// farthest from it (by steps through doorways) holds the boss or, on
// ordinary floors, the door down.
//
// Deterministic given the same seed.
GeneratedDungeon generateDungeon(const DungeonGenerationParams& params, unsigned int seed);

// Whether a floor ends in a boss fight: the Warlord (5), the Lich (10 and
// 20) and each side dungeon's last floor. Every other floor is explored for
// its exit.
bool bossFloor(int floor);

// The parameters the game generates a floor with: its region, boss room,
// vault and event chances. Shared by the game and tools/dungeon_viewer, so a
// floor seen in the viewer is the floor the game would build from that seed.
DungeonGenerationParams floorGenerationParams(int floor, bool bloodMagicUnlocked);

} // namespace engine
