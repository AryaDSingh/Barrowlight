#pragma once

namespace engine {

// Plain data -- an Actor's numeric attributes. Deliberately still no
// methods, even now that real combat formulas exist (Prompt 14,
// AttributeFormulas.hpp): those formulas interpret Stats' fields from
// the outside rather than living as Stats methods, the same
// separation-of-concerns reasoning as TalentEffects being kept separate
// from Talent. strength/dexterity/intelligence baseline at 10 --
// AttributeFormulas' bonuses are calibrated to that exact baseline, so
// changing these defaults would silently shift every derived bonus too.
struct Stats {
    int maxHp = 10;
    int hp = 10;
    int maxMana = 0;
    int mana = 0;
    int strength = 10;     // physical damage, via AttributeFormulas::physicalDamageBonus
    int dexterity = 10;    // dodge chance, via AttributeFormulas::dodgeChance
    int intelligence = 10; // magic damage + max mana, via AttributeFormulas

    // Baseline is 100. The TurnScheduler (Prompt 4) will consume this to
    // decide act frequency -- higher speed acts more often. Not used by
    // anything yet.
    int speed = 100;
};

} // namespace engine
