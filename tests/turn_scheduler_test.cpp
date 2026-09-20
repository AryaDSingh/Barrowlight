// Standalone sanity check for TurnScheduler. No SFML, no window -- same
// pattern as entity_smoke_test (see ARCHITECTURE_DECISIONS.md). Registers
// three Monsters with different speeds, runs a fixed number of turns, and
// prints who acted each time, so the speed -> frequency relationship can
// be checked before anything real depends on it being correct.

#include <iostream>
#include <memory>

#include "core/TurnScheduler.hpp"
#include "entities/Monster.hpp"
#include "ai/NullAIBehavior.hpp"

int main() {
    using namespace engine;

    Stats slowStats;
    slowStats.speed = 50;
    Monster slowpoke("Slowpoke", 's', Position{0, 0}, slowStats,
                      std::make_unique<NullAIBehavior>());

    Stats normalStats;
    normalStats.speed = 100;
    Monster normal("Normal", 'n', Position{1, 0}, normalStats,
                    std::make_unique<NullAIBehavior>());

    Stats fastStats;
    fastStats.speed = 200;
    Monster zippy("Zippy", 'z', Position{2, 0}, fastStats,
                   std::make_unique<NullAIBehavior>());

    TurnScheduler scheduler;
    scheduler.add(slowpoke);
    scheduler.add(normal);
    scheduler.add(zippy);

    constexpr int kTurnsToRun = 20;
    int slowCount = 0;
    int normalCount = 0;
    int zippyCount = 0;

    std::cout << "Turn scheduler test -- speeds 50 / 100 / 200\n";
    std::cout << "---------------------------------------------\n";

    for (int turn = 1; turn <= kTurnsToRun; ++turn) {
        Actor& actor = scheduler.nextTurn();
        std::cout << "Turn " << turn << ": " << actor.name() << '\n';

        if (&actor == &slowpoke) {
            ++slowCount;
        } else if (&actor == &normal) {
            ++normalCount;
        } else if (&actor == &zippy) {
            ++zippyCount;
        }
    }

    std::cout << "\nActed counts over " << kTurnsToRun << " turns:\n";
    std::cout << "  Slowpoke (speed 50):  " << slowCount << '\n';
    std::cout << "  Normal   (speed 100): " << normalCount << '\n';
    std::cout << "  Zippy    (speed 200): " << zippyCount << '\n';
    std::cout << "\nExpect roughly a 1:2:4 ratio -- Zippy acting about twice\n";
    std::cout << "as often as Normal, and about four times as often as\n";
    std::cout << "Slowpoke, once enough turns have passed.\n";

    return 0;
}
