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
    Monster slowpoke(MonsterType::Goblin, "Slowpoke", 's', Position{0, 0}, slowStats,
                      std::make_unique<NullAIBehavior>());

    Stats normalStats;
    normalStats.speed = 100;
    Monster normal(MonsterType::Goblin, "Normal", 'n', Position{1, 0}, normalStats,
                    std::make_unique<NullAIBehavior>());

    Stats fastStats;
    fastStats.speed = 200;
    Monster zippy(MonsterType::Goblin, "Zippy", 'z', Position{2, 0}, fastStats,
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

    // Speed statuses: Hasted speeds a creature up, Slowed and Chill slow it.
    bool allOk = true;
    const auto check = [&](bool condition, const char* what) {
        std::cout << (condition ? "[ok] " : "[FAIL] ") << what << '\n';
        allOk = allOk && condition;
    };
    const auto actsPer = [&](StatusEffectInstance effect) {
        Stats a; Stats b;
        Monster mover(MonsterType::Goblin, "Mover", 'm', Position{0, 0}, a, std::make_unique<NullAIBehavior>());
        Monster plain(MonsterType::Goblin, "Plain", 'p', Position{1, 0}, b, std::make_unique<NullAIBehavior>());
        effect.turnsRemaining = 10000;
        mover.statusEffects().apply(effect);
        TurnScheduler s; s.add(mover); s.add(plain);
        int moves = 0, plains = 0;
        for (int i = 0; i < 300; ++i) { Actor& next = s.nextTurn(); if (&next == &mover) ++moves; else ++plains; }
        return static_cast<double>(moves) / plains;
    };
    const double hasted = actsPer({StatusEffectType::Hasted, 1, 50});
    const double slowed = actsPer({StatusEffectType::Slowed, 1, 50});
    const double chilled = actsPer({StatusEffectType::Chill, 1, 20});
    const double frozen = actsPer({StatusEffectType::Slowed, 1, 95});
    check(hasted > 1.45 && hasted < 1.55, "Hasted 50: acts half again as often");
    check(slowed > 0.45 && slowed < 0.55, "Slowed 50: acts half as often");
    check(chilled > 0.75 && chilled < 0.85, "Chill 20: acts a fifth less often");
    check(frozen > 0.2 && frozen < 0.3, "Nothing slows below a quarter speed");
    std::cout << '\n' << (allOk ? "All turn scheduler checks passed." : "Some turn scheduler checks FAILED.") << '\n';
    return allOk ? 0 : 1;
}
