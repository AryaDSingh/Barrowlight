#pragma once

namespace engine {

// Plain data -- an Actor's numeric attributes. Deliberately still no
// methods, even now that real combat formulas exist (AttributeFormulas.hpp):
// those formulas interpret Stats' fields from the outside rather than
// living as Stats methods, the same separation-of-concerns reasoning as
// TalentEffects being kept separate from Talent.
//
// strength/dexterity/intelligence default to 0, not some implicit
// "average" value -- the attribute-system redesign has no baseline
// concept at all (a class's starting spread is a hand-picked low value
// like 2 or 6, and every point counts at full value from zero). A
// freshly-constructed Actor with every attribute left untouched
// contributes exactly zero bonus anywhere, the same property the old
// baseline-10 model achieved by subtracting 10 first -- this achieves
// it more directly, by having nothing to subtract at all.
struct Stats {
    int maxHp = 10;
    int hp = 10;
    int maxMana = 0;
    int mana = 0;
    int strength = 0;     // scales Strength-tagged talents, via AttributeFormulas::abilityDamageBonus
    int dexterity = 0;    // dodge chance + crit chance, via AttributeFormulas
    int intelligence = 0; // scales Intelligence-tagged talents, via AttributeFormulas

    // Baseline is 100. The TurnScheduler adds it to the actor's energy each
    // tick, so higher speed acts more often (Hasted and Slowed change it).
    int speed = 100;
};

} // namespace engine
