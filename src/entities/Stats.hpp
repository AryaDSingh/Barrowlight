#pragma once

namespace engine {

// Plain data -- an Actor's numeric attributes. Deliberately no methods:
// combat math and derived stats (damage formulas, resistances, whatever
// the talent system ends up needing) don't get designed until there's
// actual combat to write formulas for. Adding behavior here now would
// mean guessing at that design early and probably guessing wrong.
struct Stats {
    int maxHp = 10;
    int hp = 10;
    int maxMana = 0;
    int mana = 0;
    int strength = 10;
    int dexterity = 10;

    // Baseline is 100. The TurnScheduler (Prompt 4) will consume this to
    // decide act frequency -- higher speed acts more often. Not used by
    // anything yet.
    int speed = 100;
};

} // namespace engine
