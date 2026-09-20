#include "world/DungeonGenerator.hpp"

#include <algorithm>
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

} // namespace

GeneratedDungeon generateDungeon(const DungeonGenerationParams& params, unsigned int seed) {
    Map map(params.width, params.height);
    // Map starts entirely Wall by construction (Tile's own default member
    // initializers -- see Tile.hpp), so only carved tiles need touching;
    // no separate "fill with walls" pass is needed.

    std::mt19937 rng(seed);
    std::uniform_int_distribution<int> sizeDist(params.minRoomSize, params.maxRoomSize);
    std::uniform_int_distribution<int> coinFlip(0, 1);

    std::vector<Room> rooms;
    constexpr int kPadding = 1; // minimum gap between rooms so they don't visually merge
    constexpr int kMaxAttempts = 200; // generous cap so this can't loop forever

    for (int attempt = 0;
         attempt < kMaxAttempts && static_cast<int>(rooms.size()) < params.maxRooms;
         ++attempt) {
        const int w = sizeDist(rng);
        const int h = sizeDist(rng);

        // Leave a guaranteed 1-tile border of wall around the whole map.
        const int maxX1 = params.width - w - 1;
        const int maxY1 = params.height - h - 1;
        if (maxX1 < 1 || maxY1 < 1) {
            continue; // this room size doesn't fit at all -- skip and retry
        }

        std::uniform_int_distribution<int> xDist(1, maxX1);
        std::uniform_int_distribution<int> yDist(1, maxY1);
        const int x1 = xDist(rng);
        const int y1 = yDist(rng);
        const Room room{x1, y1, x1 + w, y1 + h};

        const bool overlapsExisting =
            std::any_of(rooms.begin(), rooms.end(),
                        [&](const Room& existing) { return room.overlaps(existing, kPadding); });
        if (overlapsExisting) {
            continue;
        }

        carveRoom(map, room);

        if (!rooms.empty()) {
            const Position prevCenter = rooms.back().center();
            const Position newCenter = room.center();
            // Randomly pick the corridor's elbow direction for visual
            // variety, rather than always turning the same way.
            if (coinFlip(rng) == 0) {
                carveHorizontalCorridor(map, prevCenter.x, newCenter.x, prevCenter.y);
                carveVerticalCorridor(map, prevCenter.y, newCenter.y, newCenter.x);
            } else {
                carveVerticalCorridor(map, prevCenter.y, newCenter.y, prevCenter.x);
                carveHorizontalCorridor(map, prevCenter.x, newCenter.x, newCenter.y);
            }
        }

        rooms.push_back(room);
    }

    if (rooms.empty()) {
        throw std::runtime_error(
            "generateDungeon: failed to place any rooms -- map too small for the given "
            "room size parameters");
    }

    GeneratedDungeon result;
    result.map = std::move(map);
    result.playerStart = rooms.front().center();
    result.monsterStart = rooms.back().center();
    result.roomCount = static_cast<int>(rooms.size());
    return result;
}

} // namespace engine
