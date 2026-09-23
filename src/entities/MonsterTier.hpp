#pragma once

namespace engine {

// Which difficulty tier a spawned monster belongs to -- Prompt 22.
enum class MonsterTier {
    Base,
    Elite,
    Nightmare,
};

// Maps the player's current character level to the tier that should be
// spawning. The original request specified Base at 1-2, Elite at 3-5,
// Nightmare at 7-8, leaving 6, 9, and 10 unstated -- each open gap is
// extended into whichever neighboring tier came right before it (Elite
// through 6, Nightmare through 10) rather than left as an arbitrary
// undefined case.
MonsterTier tierForLevel(int playerLevel);

// Stat multipliers applied at creation time (see
// MonsterFactory::createMonster). hp and damage are scaled
// independently: Elite monsters hit harder and take longer to kill,
// Nightmare monsters meaningfully more of both. Base is always exactly
// 1.0/1.0/1.0 -- identical to a monster with no tier concept at all,
// so every pre-Prompt-22 balance decision (Prompt 21's rebalance
// included) stays exactly as tuned for the common case.
float hpMultiplierForTier(MonsterTier tier);
float damageMultiplierForTier(MonsterTier tier);

// A tougher monster should be worth more XP -- scales independently
// from hp/damage since "how much this is worth" and "how dangerous
// this is" are related but not identical questions.
float xpMultiplierForTier(MonsterTier tier);

// A short prefix for the monster's display name ("", "Elite ", or
// "Nightmare "). The *only* way a tier is currently visible without
// opening the combat log -- see Application's monster rendering for the
// accompanying visual border, added specifically because a name prefix
// alone isn't visible on an unlabeled colored tile.
const char* namePrefixForTier(MonsterTier tier);

} // namespace engine
