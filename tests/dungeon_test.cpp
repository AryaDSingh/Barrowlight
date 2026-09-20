// Standalone sanity check for generateDungeon. No SFML, no window -- same
// pattern as the other tests. The single property that actually matters
// for "a connected, playable Map" is full connectivity -- verified here
// via flood-fill across several different seeds, not just assumed
// because the "connect each room to the previous one" logic looks
// simple. Also prints one example layout, which is literally what the
// prompt asked for (a quick way to inspect a generated layout) -- a
// console printout, complementing the in-game regenerate key.

#include <iostream>
#include <queue>
#include <vector>

#include "world/DungeonGenerator.hpp"

using namespace engine;

namespace {

int countReachableFloorTiles(const Map& map, Position start) {
    std::vector<std::vector<bool>> visited(
        static_cast<std::size_t>(map.height()),
        std::vector<bool>(static_cast<std::size_t>(map.width()), false));

    std::queue<Position> frontier;
    frontier.push(start);
    visited[static_cast<std::size_t>(start.y)][static_cast<std::size_t>(start.x)] = true;

    static constexpr int kDx[4] = {0, 0, -1, 1};
    static constexpr int kDy[4] = {-1, 1, 0, 0};

    int count = 0;
    while (!frontier.empty()) {
        const Position p = frontier.front();
        frontier.pop();
        ++count;

        for (int dir = 0; dir < 4; ++dir) {
            const Position n{p.x + kDx[dir], p.y + kDy[dir]};
            if (map.isWalkable(n.x, n.y) &&
                !visited[static_cast<std::size_t>(n.y)][static_cast<std::size_t>(n.x)]) {
                visited[static_cast<std::size_t>(n.y)][static_cast<std::size_t>(n.x)] = true;
                frontier.push(n);
            }
        }
    }
    return count;
}

int countTotalFloorTiles(const Map& map) {
    int count = 0;
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            if (map.tileAt(x, y).walkable) {
                ++count;
            }
        }
    }
    return count;
}

void printDungeon(const Map& map, Position playerStart, Position monsterStart) {
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            if (x == playerStart.x && y == playerStart.y) {
                std::cout << '@';
            } else if (x == monsterStart.x && y == monsterStart.y) {
                std::cout << 'g';
            } else {
                std::cout << (map.tileAt(x, y).type == TileType::Wall ? '#' : '.');
            }
        }
        std::cout << '\n';
    }
}

} // namespace

int main() {
    const DungeonGenerationParams params; // defaults

    std::cout << "Dungeon generation test\n";
    std::cout << "-----------------------\n\n";

    bool allConnected = true;
    for (unsigned int seed = 1; seed <= 10; ++seed) {
        const GeneratedDungeon dungeon = generateDungeon(params, seed);
        const int reachable = countReachableFloorTiles(dungeon.map, dungeon.playerStart);
        const int total = countTotalFloorTiles(dungeon.map);
        const bool connected = reachable == total;
        allConnected &= connected;

        std::cout << "seed " << seed << ": " << dungeon.roomCount << " rooms, " << reachable
                  << "/" << total << " floor tiles reachable from player start "
                  << (connected ? "[ok]" : "[FAIL]") << '\n';
    }

    std::cout << "\n"
              << (allConnected ? "All seeds fully connected." : "Some seeds FAILED connectivity.")
              << "\n\n";

    const GeneratedDungeon example = generateDungeon(params, /*seed=*/42);
    std::cout << "Example layout (seed 42), @ = player start, g = monster start:\n\n";
    printDungeon(example.map, example.playerStart, example.monsterStart);

    return allConnected ? 0 : 1;
}
