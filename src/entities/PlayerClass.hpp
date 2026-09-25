#pragma once

namespace engine {

// Which playable class the person is currently controlling. Own tiny
// header, same reasoning as MonsterType.hpp (Prompt 12): PlayerClassFactory.hpp
// needs it, and so does anything that has to remember "which class is
// this" without pulling in the whole factory (e.g. SaveGameState).
//
// Renamed again as part of the full attribute-system redesign: Fighter
// became Warrior, Sorcerer became Mage (Thief keeps its name). Not
// preserving old enum ordinals this time the way the Prompt 19 rename
// carefully did -- that rename kept save compatibility because only the
// class *names* changed; this one changes what every Str/Dex/Int value
// actually means (no more baseline-10 model), so an old save's numbers
// would be silently wrong under the new formulas even if the class enum
// still decoded correctly. The save format version bump for this
// redesign rejects old saves outright regardless of ordinal value, so
// there's nothing to preserve here.
enum class PlayerClass {
    Spellblade, // Str+Int hybrid melee/caster -- Prompt 9. Reserved for a future unlock,
                // not currently offered on the selection screen. Its stats are still on the
                // old baseline-10 model -- a known, harmless inconsistency, since there is
                // currently no way to actually reach this class in play; fix whenever its
                // unlock mechanism is actually built, not before.
    Warrior,    // pure Strength -- originally "Fighter" (Prompt 19), renamed again as part
                // of the attribute-system redesign.
    Thief,      // pure Dexterity, ranged/evasive. Not to be confused with MonsterType::Archer,
                // the Kiter-AI enemy -- separate enums, no code collision.
    Mage,       // pure Intelligence -- originally "Sorcerer" (Prompt 19), renamed again as
                // part of the attribute-system redesign.
};

} // namespace engine
