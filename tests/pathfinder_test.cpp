// Standalone sanity check for findPath. No SFML, no window -- same
// pattern as the other tests (see ARCHITECTURE_DECISIONS.md). Builds a
// room split by a long wall with a gap only on one side, so the shortest
// path is forced into a real detour -- then prints it as an ASCII grid to
// check by eye that it actually routes around the wall rather than
// (incorrectly) cutting through it.

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "world/Map.hpp"
#include "world/Pathfinder.hpp"

using namespace engine;

namespace {

void printPath(const Map& map, const std::vector<Position>& path, Position start, Position goal) {
    std::vector<std::vector<char>> grid(
        static_cast<std::size_t>(map.height()),
        std::vector<char>(static_cast<std::size_t>(map.width()), ' '));

    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            grid[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] =
                map.tileAt(x, y).type == TileType::Wall ? '#' : '.';
        }
    }
    for (const Position& p : path) {
        grid[static_cast<std::size_t>(p.y)][static_cast<std::size_t>(p.x)] = '+';
    }
    grid[static_cast<std::size_t>(start.y)][static_cast<std::size_t>(start.x)] = 'S';
    grid[static_cast<std::size_t>(goal.y)][static_cast<std::size_t>(goal.x)] = 'G';

    for (const auto& row : grid) {
        for (char c : row) {
            std::cout << c;
        }
        std::cout << '\n';
    }
}

} // namespace

int main() {
    // Wall across row 3 spans x=1..8, leaving a gap only at x=9. Start is
    // top-left, goal is bottom-left -- the shortest path MUST detour all
    // the way to x=9 and back, not take a direct route.
    const std::vector<std::string> rows = {
        "###########",
        "#@........#",
        "#.........#",
        "#########.#",
        "#.........#",
        "#.........#",
        "###########",
    };

    Position start;
    Map map = parseAsciiMap(rows, start);
    const Position goal{1, 5};

    const std::optional<std::vector<Position>> path = findPath(map, start, goal);

    std::cout << "Pathfinder test -- start (" << start.x << ',' << start.y
              << "), goal (" << goal.x << ',' << goal.y << ")\n";
    std::cout << "S = start   G = goal   + = path   # = wall\n\n";

    if (!path) {
        std::cout << "[FAIL] no path found (a path should exist through the gap at x=9)\n";
        return 1;
    }

    printPath(map, *path, start, goal);

    const int manhattan = std::abs(goal.x - start.x) + std::abs(goal.y - start.y);
    std::cout << "\nPath length: " << path->size() << " tiles\n";
    std::cout << "Direct Manhattan distance: " << manhattan
              << " (the wall forces a detour, so the real path should be well over this)\n";

    const bool detoured = static_cast<int>(path->size()) > manhattan + 4;
    std::cout << (detoured ? "[ok] " : "[FAIL] ")
              << "path is meaningfully longer than the direct distance, confirming it "
                 "actually routed around the wall\n";

    // Also confirm an unreachable goal is correctly reported, not just
    // the "found a path" case.
    const std::vector<std::string> sealedRows = {
        "#####",
        "#@#.#",
        "#####",
    };
    Position sealedStart;
    Map sealedMap = parseAsciiMap(sealedRows, sealedStart);
    const std::optional<std::vector<Position>> noPath =
        findPath(sealedMap, sealedStart, Position{3, 1});

    std::cout << "\n"
              << (!noPath.has_value() ? "[ok] " : "[FAIL] ")
              << "correctly reports no path when the goal is sealed off by walls\n";

    return 0;
}
