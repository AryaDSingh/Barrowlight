// Standalone sanity check for Chaser specifically -- findPath and
// computeFieldOfView each have their own tests (pathfinder_test,
// fov_test), but Chaser's own logic (visibility gate -> pathfind ->
// stop-when-adjacent) combining them hasn't been tested in isolation
// until now, and it's the actual named deliverable of this prompt. No
// SFML, no window -- same pattern as the other tests.

#include <iostream>
#include <memory>
#include <vector>

#include "ai/Chaser.hpp"
#include "entities/Monster.hpp"
#include "world/Map.hpp"

using namespace engine;

int main() {
    const std::vector<std::string> rows = {
        "###########",
        "#@........#",
        "#.........#",
        "#.........#",
        "#.........#",
        "###########",
    };
    Position unused; // parseAsciiMap needs the '@' above; the resulting
                      // position isn't otherwise used in this test.
    Map map = parseAsciiMap(rows, unused);

    bool allOk = true;

    // Case 1: target well outside sight radius -- expect no move at all.
    {
        Monster goblin("Goblin", 'g', Position{1, 1}, Stats{}, std::make_unique<Chaser>(5));
        const Position farTarget{9, 3}; // distance ~8.25, beyond radius 5
        const std::optional<Position> move = goblin.ai()->decideMove(goblin, map, farTarget);
        const bool ok = !move.has_value();
        allOk &= ok;
        std::cout << (ok ? "[ok] " : "[FAIL] ")
                  << "target beyond sight radius -> stays put\n";
    }

    // Case 2: target in a straight line, within sight -- goblin should
    // step toward it one tile per call, then stop once adjacent rather
    // than stepping onto the target's own tile (no attack action exists
    // yet -- see ARCHITECTURE_DECISIONS.md).
    {
        Monster goblin("Goblin", 'g', Position{1, 1}, Stats{}, std::make_unique<Chaser>(5));
        const Position target{5, 1};
        // Hand-computed expected path: (1,1)->(2,1)->(3,1)->(4,1), then
        // stop (adjacent to (5,1), one tile away).
        const std::vector<Position> expectedSteps = {{2, 1}, {3, 1}, {4, 1}};

        std::cout << "\nChasing target (" << target.x << ',' << target.y << ") from ("
                  << goblin.position().x << ',' << goblin.position().y << "):\n";

        for (std::size_t i = 0; i < expectedSteps.size(); ++i) {
            const std::optional<Position> move = goblin.ai()->decideMove(goblin, map, target);
            const bool moved = move.has_value();
            const bool matchesExpected =
                moved && move->x == expectedSteps[i].x && move->y == expectedSteps[i].y;
            allOk &= matchesExpected;

            std::cout << "  step " << i << ": "
                      << (moved ? "moved to (" + std::to_string(move->x) + "," +
                                       std::to_string(move->y) + ")"
                                : "stayed put")
                      << (matchesExpected ? " [ok]" : " [FAIL]") << '\n';

            if (moved) {
                goblin.setPosition(*move);
            }
        }

        // One more call: should now be adjacent and stay put rather than
        // stepping onto the target's tile.
        const std::optional<Position> finalMove = goblin.ai()->decideMove(goblin, map, target);
        const bool staysPutWhenAdjacent = !finalMove.has_value();
        allOk &= staysPutWhenAdjacent;
        std::cout << "  final (adjacent): "
                  << (staysPutWhenAdjacent ? "[ok] stayed put, did not step onto target's tile"
                                            : "[FAIL] tried to move onto the target's tile")
                  << '\n';
    }

    std::cout << "\n" << (allOk ? "All Chaser checks passed." : "Some Chaser checks FAILED.")
              << '\n';

    return allOk ? 0 : 1;
}
