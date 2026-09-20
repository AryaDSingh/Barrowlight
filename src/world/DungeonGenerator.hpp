#pragma once

#include <vector>

#include "core/Position.hpp"
#include "world/Map.hpp"

namespace engine {

struct DungeonGenerationParams {
    int width = 38;
    int height = 20;
    int minRoomSize = 4;
    int maxRoomSize = 7;
    int maxRooms = 10;
};

struct GeneratedDungeon {
    Map map;
    Position playerStart;         // room 0's center
    std::vector<Position> otherRoomCenters; // every other room's center, in placement order --
                                             // lets the caller populate multiple rooms with
                                             // different monsters (Prompt 10), not just one
    int roomCount = 0;
};

// Generates a dungeon: random non-overlapping rectangular rooms, each
// connected to the previous one (in placement order) by an L-shaped
// corridor. Connecting each new room to the prior one guarantees the
// whole dungeon is one connected component, the same way a linked list
// is -- no separate graph/connectivity pass needed afterward. Verified
// explicitly in dungeon_test via flood-fill across multiple seeds, not
// just assumed because the carving logic looks simple.
//
// Deterministic given the same seed -- same inputs always produce the
// same layout, which is what makes dungeon_test's checks reproducible
// and is also how Application's regenerate key gets a fresh layout: a
// new seed, not a different algorithm.
GeneratedDungeon generateDungeon(const DungeonGenerationParams& params, unsigned int seed);

} // namespace engine
