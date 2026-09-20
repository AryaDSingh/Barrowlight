#pragma once

#include <vector>

#include "core/Position.hpp"
#include "world/Map.hpp"

namespace engine {

struct DungeonGenerationParams {
    int width = 38;
    int height = 20;
    int minRoomSize = 3;
    int maxRoomSize = 5;
    // 6, not 10 (Prompt 8's original default) or 7 -- empirically tuned
    // (see ARCHITECTURE_DECISIONS.md) alongside the smaller room sizes
    // and boss room range below to keep boss room placement reliable
    // (~97% across 300 seeds) while still populating most of the 6-type
    // monster roster (Prompt 10) most of the time.
    int maxRooms = 6;
    int bossRoomMinSize = 7;
    int bossRoomMaxSize = 9;
};

struct GeneratedDungeon {
    Map map;
    Position playerStart;                    // room 0's center
    std::vector<Position> otherRoomCenters;   // every regular room's center (not the boss
                                               // room), in placement order -- lets the caller
                                               // populate multiple rooms with different
                                               // monsters (Prompt 10)
    bool hasBossRoom = false;                 // false only if no space could be found at all
                                               // (rare; see generateDungeon)
    Position bossRoomCenter;                  // valid only if hasBossRoom
    int roomCount = 0;                        // includes the boss room, if any
};

// Generates a dungeon: random non-overlapping rectangular rooms, each
// connected to the previous one (in placement order) by an L-shaped
// corridor. Connecting each new room to the prior one guarantees the
// whole dungeon is one connected component, the same way a linked list
// is -- no separate graph/connectivity pass needed afterward. Verified
// explicitly in dungeon_test via flood-fill across multiple seeds, not
// just assumed because the carving logic looks simple.
//
// After the regular rooms, one more attempt is made to place a
// deliberately larger boss room, connected to whichever regular room was
// placed last -- topologically the far end of the level from the
// player's start, matching "a set-piece at the end of the level
// sequence" (Prompt 11). This can fail to find space (hasBossRoom will
// be false) if the regular rooms happened to fill the map too densely;
// verified empirically across many seeds in dungeon_test, not assumed to
// always succeed.
//
// Deterministic given the same seed -- same inputs always produce the
// same layout, which is what makes dungeon_test's checks reproducible
// and is also how Application's regenerate key gets a fresh layout: a
// new seed, not a different algorithm.
GeneratedDungeon generateDungeon(const DungeonGenerationParams& params, unsigned int seed);

} // namespace engine
