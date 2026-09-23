#pragma once

namespace engine {

// Which playable class the person is currently controlling. Own tiny
// header, same reasoning as MonsterType.hpp (Prompt 12): PlayerClassFactory.hpp
// needs it, and so does anything that has to remember "which class is
// this" without pulling in the whole factory (e.g. SaveGameState).
//
// As of Prompt 19: restructured around three pure "base classes," one
// per attribute -- Fighter/Sorcerer/Thief are the only classes offered
// on the selection screen now. Spellblade, the original Str+Int hybrid
// (Prompt 9), still exists in full -- PlayerClassFactory still knows
// its stats and talents, save/load still round-trips it correctly if a
// save happens to reference it -- but it's deliberately not offered as
// a starting option anymore. Reserved for a future "unlock" once the
// leveling system (Prompt 20+) has a real mechanism for granting it;
// not deleted, not silently dropped, just not part of "3 base classes"
// right now. Enum values are NOT reordered when renamed (Marauder's old
// ordinal position keeps Fighter's value 1, Archer's keeps Thief's
// value 2) specifically so any existing save file's encoded integer
// still means the same class it always did -- only Sorcerer, genuinely
// new, is appended at the end rather than inserted anywhere that would
// shift an existing value.
enum class PlayerClass {
    Spellblade, // Str+Int hybrid melee/caster -- Prompt 9. Reserved for a future unlock,
                // not currently offered on the selection screen (see Prompt 19 above).
    Fighter,    // pure Strength, hp-as-resource berserker -- originally "Marauder" (Prompt 15),
                // renamed at Prompt 19 as part of the 3-base-class restructuring.
    Thief,      // pure Dexterity, ranged/evasive -- originally "Archer" (Prompt 16), renamed
                // at Prompt 19. Not to be confused with MonsterType::Archer, the Kiter-AI
                // enemy -- separate enums, no code collision, but worth knowing about.
    Sorcerer,   // pure Intelligence, the third base class -- new at Prompt 19, completing
                // the one-pure-class-per-attribute trio alongside Fighter and Thief.
};

} // namespace engine
