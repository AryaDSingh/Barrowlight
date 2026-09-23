#include "world/DungeonGenerator.hpp"

#include <algorithm>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace engine {

namespace {

struct Room {
    int x1, y1, x2, y2; // half-open: [x1,x2) x [y1,y2)

    Position center() const { return Position{(x1 + x2) / 2, (y1 + y2) / 2}; }

    bool overlaps(const Room& other, int padding) const {
        return x1 - padding < other.x2 && x2 + padding > other.x1 &&
               y1 - padding < other.y2 && y2 + padding > other.y1;
    }
};

void carveRoom(Map& map, const Room& room) {
    for (int y = room.y1; y < room.y2; ++y) {
        for (int x = room.x1; x < room.x2; ++x) {
            map.setTile(x, y, Tile{TileType::Floor, true, true});
        }
    }
}

void carveHorizontalCorridor(Map& map, int x1, int x2, int y) {
    for (int x = std::min(x1, x2); x <= std::max(x1, x2); ++x) {
        map.setTile(x, y, Tile{TileType::Floor, true, true});
    }
}

void carveVerticalCorridor(Map& map, int y1, int y2, int x) {
    for (int y = std::min(y1, y2); y <= std::max(y1, y2); ++y) {
        map.setTile(x, y, Tile{TileType::Floor, true, true});
    }
}

void carveCorridorBetween(Map& map, std::mt19937& rng, Position prevCenter, Position newCenter) {
    std::uniform_int_distribution<int> coinFlip(0, 1);
    // Randomly pick the corridor's elbow direction for visual variety,
    // rather than always turning the same way.
    if (coinFlip(rng) == 0) {
        carveHorizontalCorridor(map, prevCenter.x, newCenter.x, prevCenter.y);
        carveVerticalCorridor(map, prevCenter.y, newCenter.y, newCenter.x);
    } else {
        carveVerticalCorridor(map, prevCenter.y, newCenter.y, prevCenter.x);
        carveHorizontalCorridor(map, prevCenter.x, newCenter.x, newCenter.y);
    }
}

// Attempts to place one room with a size in [minSize, maxSize], avoiding
// overlap with anything in `avoid`. On success, carves it, connects it
// to `connectTo` (if provided) with an L-shaped corridor, and returns the
// placed Room. Extracted once the boss room (Prompt 11) needed the exact
// same placement logic as the main loop, just with a different size
// range and connection target -- a second independent use is exactly
// the "worth deduplicating" signal.
std::optional<Room> tryPlaceRoom(Map& map, std::mt19937& rng, int minSize, int maxSize,
                                  int mapWidth, int mapHeight, int padding,
                                  const std::vector<Room>& avoid,
                                  const Room* connectTo) {
    std::uniform_int_distribution<int> sizeDist(minSize, maxSize);
    const int w = sizeDist(rng);
    const int h = sizeDist(rng);

    // Leave a guaranteed 1-tile border of wall around the whole map.
    const int maxX1 = mapWidth - w - 1;
    const int maxY1 = mapHeight - h - 1;
    if (maxX1 < 1 || maxY1 < 1) {
        return std::nullopt;
    }

    std::uniform_int_distribution<int> xDist(1, maxX1);
    std::uniform_int_distribution<int> yDist(1, maxY1);
    const int x1 = xDist(rng);
    const int y1 = yDist(rng);
    const Room room{x1, y1, x1 + w, y1 + h};

    const bool overlapsExisting = std::any_of(
        avoid.begin(), avoid.end(),
        [&](const Room& existing) { return room.overlaps(existing, padding); });
    if (overlapsExisting) {
        return std::nullopt;
    }

    carveRoom(map, room);
    if (connectTo != nullptr) {
        carveCorridorBetween(map, rng, connectTo->center(), room.center());
    }
    return room;
}

} // namespace

GeneratedDungeon generateDungeon(const DungeonGenerationParams& params, unsigned int seed) {
    Map map(params.width, params.height);
    // Map starts entirely Wall by construction (Tile's own default member
    // initializers -- see Tile.hpp), so only carved tiles need touching;
    // no separate "fill with walls" pass is needed.

    std::mt19937 rng(seed);

    std::vector<Room> rooms;
    constexpr int kPadding = 1; // minimum gap between rooms so they don't visually merge
    constexpr int kMaxAttempts = 200; // generous cap so this can't loop forever
    constexpr int kBossRoomAttempts = 100; // generous too -- a failed boss room means no boss

    for (int attempt = 0;
         attempt < kMaxAttempts && static_cast<int>(rooms.size()) < params.maxRooms;
         ++attempt) {
        const std::optional<Room> placed =
            tryPlaceRoom(map, rng, params.minRoomSize, params.maxRoomSize, params.width,
                         params.height, kPadding, rooms, rooms.empty() ? nullptr : &rooms.back());
        if (placed.has_value()) {
            rooms.push_back(*placed);
        }
    }

    if (rooms.empty()) {
        throw std::runtime_error(
            "generateDungeon: failed to place any rooms -- map too small for the given "
            "room size parameters");
    }

    // One more attempt, after all regular rooms: a deliberately larger
    // boss room, connected to whichever regular room was placed last --
    // topologically the far end of the level from the player's start.
    // Tried last (not first) specifically so it ends up at the end of
    // the room chain, matching "a set-piece at the end of the level
    // sequence" -- trying it first would place it right next to the
    // player's own starting room instead.
    std::optional<Room> bossRoom;
    for (int attempt = 0; attempt < kBossRoomAttempts && !bossRoom.has_value(); ++attempt) {
        bossRoom = tryPlaceRoom(map, rng, params.bossRoomMinSize, params.bossRoomMaxSize,
                                 params.width, params.height, kPadding, rooms, &rooms.back());
    }

    // Extra "shortcut" connections between nearby regular rooms that
    // aren't already chain-adjacent (Prompt 21 follow-up to Prompt 18's
    // bigger dungeons) -- the chain above is a *connectivity guarantee*,
    // not meant to be the only route through. Without this, the dungeon
    // is topologically a single corridor no matter how large the map
    // gets: reaching room 8 always means passing through rooms 2-7 in
    // order, which reads as "one long hallway" rather than "one
    // connected space" once the map is big enough for that to matter --
    // confirmed live, not just reasoned about (a real BFS-verified
    // walkthrough on the bigger 60x32 map). Regular rooms only, not the
    // boss room -- it should stay reachable only by the full chain,
    // preserving "the far end of the level, reached at the end of the
    // sequence" (Prompt 11). Capped and probabilistic, not "connect
    // every nearby pair": a fully connected grid would remove the sense
    // of distinct areas entirely, which isn't the goal either.
    constexpr int kMaxExtraConnections = 6;
    constexpr int kExtraConnectionMaxDistanceSq = 15 * 15;
    constexpr float kExtraConnectionChance = 0.4f;
    std::uniform_real_distribution<float> extraConnectionRoll(0.f, 1.f);
    int extraConnectionsAdded = 0;
    for (std::size_t i = 0; i < rooms.size() && extraConnectionsAdded < kMaxExtraConnections;
         ++i) {
        // j starts at i+2: j == i+1 is already chain-connected, nothing
        // extra to add there.
        for (std::size_t j = i + 2;
             j < rooms.size() && extraConnectionsAdded < kMaxExtraConnections; ++j) {
            const Position centerI = rooms[i].center();
            const Position centerJ = rooms[j].center();
            const int dx = centerI.x - centerJ.x;
            const int dy = centerI.y - centerJ.y;
            const int distanceSq = dx * dx + dy * dy;
            if (distanceSq <= kExtraConnectionMaxDistanceSq &&
                extraConnectionRoll(rng) < kExtraConnectionChance) {
                carveCorridorBetween(map, rng, centerI, centerJ);
                ++extraConnectionsAdded;
            }
        }
    }

    GeneratedDungeon result;
    result.map = std::move(map);
    result.playerStart = rooms.front().center();
    for (std::size_t i = 1; i < rooms.size(); ++i) {
        result.otherRoomCenters.push_back(rooms[i].center());
    }
    result.hasBossRoom = bossRoom.has_value();
    if (bossRoom.has_value()) {
        result.bossRoomCenter = bossRoom->center();
    }
    result.roomCount = static_cast<int>(rooms.size()) + (bossRoom.has_value() ? 1 : 0);
    return result;
}

} // namespace engine
