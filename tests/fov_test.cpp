// Standalone sanity check for computeFieldOfView. No SFML, no window --
// same pattern as the other tests (see ARCHITECTURE_DECISIONS.md). Builds
// a small room with a wall pillar splitting it, computes FOV from a fixed
// origin, and prints the result as an ASCII grid so shadow-casting
// correctness can be checked by eye: tiles directly behind the pillar
// (same row, far side) should be hidden, while tiles with a clear
// diagonal path around it should be visible.

#include <iostream>
#include <string>
#include <vector>

#include "world/FieldOfView.hpp"
#include "world/Map.hpp"

using namespace engine;

int main() {
    // 11x7 room, single-tile-wide wall pillar at x=5, spanning y=2..4.
    // Player ('@') sits left of the pillar, same row as its middle.
    const std::vector<std::string> rows = {
        "###########",
        "#.........#",
        "#....#....#",
        "#..@.#....#",
        "#....#....#",
        "#.........#",
        "###########",
    };

    Position origin;
    Map map = parseAsciiMap(rows, origin);

    constexpr int kRadius = 8;
    std::vector<Position> visible = computeFieldOfView(map, origin, kRadius);

    // Build a lookup grid from the (possibly duplicate-containing) result.
    std::vector<std::vector<bool>> isVisible(
        static_cast<std::size_t>(map.height()),
        std::vector<bool>(static_cast<std::size_t>(map.width()), false));
    for (const Position& p : visible) {
        isVisible[static_cast<std::size_t>(p.y)][static_cast<std::size_t>(p.x)] = true;
    }

    std::cout << "FOV test -- origin (" << origin.x << ',' << origin.y
              << "), radius " << kRadius << "\n";
    std::cout << "@ = origin   # = wall (visible)   ' ' = wall (not visible)\n";
    std::cout << "* = floor (visible)   . = floor (not visible)\n\n";

    int visibleFloorCount = 0;
    int hiddenFloorCount = 0;

    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            const bool vis = isVisible[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
            const bool isWall = map.tileAt(x, y).type == TileType::Wall;

            if (x == origin.x && y == origin.y) {
                std::cout << '@';
            } else if (isWall) {
                std::cout << (vis ? '#' : ' ');
            } else {
                std::cout << (vis ? '*' : '.');
                vis ? ++visibleFloorCount : ++hiddenFloorCount;
            }
        }
        std::cout << '\n';
    }

    std::cout << "\nVisible floor tiles: " << visibleFloorCount << '\n';
    std::cout << "Hidden floor tiles (out of radius or blocked): "
              << hiddenFloorCount << '\n';

    // Sanity checks, not a substitute for eyeballing the grid above: the
    // pillar should cast SOME shadow (not everything visible) but the
    // room should not be entirely dark either (not everything hidden).
    const bool someHidden = hiddenFloorCount > 0;
    const bool someVisible = visibleFloorCount > 0;
    std::cout << "\n"
              << (someVisible ? "[ok] " : "[FAIL] ") << "some floor tiles are visible\n"
              << (someHidden ? "[ok] " : "[FAIL] ")
              << "some floor tiles are hidden (pillar is actually casting a shadow)\n";

    return 0;
}
