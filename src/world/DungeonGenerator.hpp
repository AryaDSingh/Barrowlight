#pragma once

#include <vector>

#include "core/Position.hpp"
#include "world/Map.hpp"

namespace engine {

struct DungeonGenerationParams {
    // 60x32 / 10 rooms, not the original 38x20 / 6 (Prompt 8) --
    // increased at Prompt 18 once a camera (Application::updateCamera)
    // made a dungeon bigger than one screen actually renderable.
    // Empirically re-verified after the change, not assumed to still be
    // safe just because the old tuning was careful: dungeon_test reports
    // 100% connectivity and 100% boss room placement across its existing
    // sample sizes at these dimensions -- boss placement is *more*
    // reliable than the original's ~97%, not less, since a bigger map
    // gives the last, largest room proportionally more free space to
    // land in.
    int width = 60;
    int height = 32;
    int minRoomSize = 3;
    int maxRoomSize = 5;
    int maxRooms = 10;
    int bossRoomMinSize = 7;
    int bossRoomMaxSize = 9;

    // Whether to attempt placing a boss room at all -- true by default
    // (every dungeon generated before the multi-floor system had a
    // boss). As of the floor-progression system, Application sets this
    // to false for every floor except the ones that should actually
    // have a boss fight, so most floors generate as a pure "clear it,
    // find the door" dungeon with no boss room attempted at all.
    bool includeBossRoom = true;
    bool includeVault = false;
};

struct GeneratedDungeon {
    Map map;
    bool hasVault = false;
    Position vaultCenter{}, vaultEntrance{};
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
