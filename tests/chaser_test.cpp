// Chaser, the melee rusher: it paths toward a target it can see and attacks
// once adjacent. Hand-traced step by step on fixed ASCII maps. No SFML.

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "ai/Chaser.hpp"
#include "entities/Monster.hpp"
#include "entities/Player.hpp"
#include "world/Map.hpp"

using namespace engine;

namespace {
bool g_allOk = true;

void check(bool condition, const std::string& description) {
    g_allOk &= condition;
    std::cout << (condition ? "[ok] " : "[FAIL] ") << description << '\n';
}
} // namespace

int main() {
    const std::vector<std::string> rows = {
        "###########",
        "#@........#",
        "#.........#",
        "#.........#",
        "#.........#",
        "###########",
    };
    Position unused; // parseAsciiMap needs the '@'; not otherwise used here
    Map map = parseAsciiMap(rows, unused);

    MonsterAttackProfile profile;
    profile.power = 5;

    // Case 1: target well outside sight radius -- expect Wait.
    {
        Monster goblin(MonsterType::Goblin, "Goblin", 'g', Position{1, 1}, Stats{},
                        std::make_unique<Chaser>(profile, /*sightRadius=*/5));
        Player farTarget(Position{9, 3}, Stats{}, TalentSet{}); // distance ~8.25, beyond radius 5
        const AIDecision decision = goblin.ai()->decideAction(goblin, map, farTarget, {});
        check(decision.type == AIActionType::Wait, "target beyond sight radius -> Wait");
    }

    // Case 2: target in a straight line, within sight -- goblin should
    // step toward it one tile per call, then Attack once adjacent rather
    // than trying to move onto the target's own tile.
    {
        Monster goblin(MonsterType::Goblin, "Goblin", 'g', Position{1, 1}, Stats{},
                        std::make_unique<Chaser>(profile, /*sightRadius=*/5));
        Player target(Position{5, 1}, Stats{}, TalentSet{});
        const std::vector<Position> expectedSteps = {{2, 1}, {3, 1}, {4, 1}};

        std::cout << "\nChasing target (" << target.position().x << ',' << target.position().y
                   << ") from (" << goblin.position().x << ',' << goblin.position().y << "):\n";

        for (std::size_t i = 0; i < expectedSteps.size(); ++i) {
            const AIDecision decision = goblin.ai()->decideAction(goblin, map, target, {});
            const bool moved = decision.type == AIActionType::Move;
            const bool matchesExpected = moved &&
                                          decision.movePosition.x == expectedSteps[i].x &&
                                          decision.movePosition.y == expectedSteps[i].y;
            check(matchesExpected,
                  "step " + std::to_string(i) + " moves toward target as expected");
            if (moved) {
                goblin.setPosition(decision.movePosition);
            }
        }

        const AIDecision finalDecision = goblin.ai()->decideAction(goblin, map, target, {});
        const bool attacksWhenAdjacent = finalDecision.type == AIActionType::Attack &&
                                          finalDecision.target == &target &&
                                          finalDecision.attackPower == 5;
        check(attacksWhenAdjacent,
              "attacks (doesn't move onto the target's tile) once adjacent, using the profile's power");
    }

    std::cout << "\n" << (g_allOk ? "All Chaser checks passed." : "Some Chaser checks FAILED.")
               << '\n';
    return g_allOk ? 0 : 1;
}
