#pragma once

#include <optional>
#include <vector>

#include "entities/Talent.hpp"

namespace engine {

// Originally "Archer" (Prompt 16), renamed to Thief at Prompt 19 as
// part of restructuring around three pure "base classes," one per
// attribute. The Thief's starting talent kit: 4 talents, the same
// "deliberately smaller than the Spellblade's 9" discipline established
// for the Fighter (Prompt 15, itself originally "Marauder," also
// renamed at Prompt 19). Pure Dexterity -- every talent is Physical
// (Strength-scaled, not Magic), since this class's Intelligence sits
// below baseline (a real, if modest, penalty) and there's no reason to
// touch a stat that actively works against you. Dexterity itself isn't
// a damage stat in this project's formula system (see
// AttributeFormulas.hpp) -- it governs dodge chance, capped at the
// Thief's own 30%, and nothing else -- so this class's identity comes
// from avoiding hits entirely, not out-damaging what it takes.
//
//   0. Quick Shot  -- the filler: ranged (no adjacency needed), cheap,
//                     fast. The Thief's bread and butter, same role
//                     Slam plays for the Fighter and Ember Bolt plays
//                     for the Spellblade, just ranged instead of melee.
//   1. Volley      -- ranged AoE: fires into a cluster rather than one
//                     target, the same AreaAroundTarget shape Fireball
//                     already uses, re-themed as arrows instead of fire.
//   2. Steady Aim  -- SelfBuff: applies Empowered, the same status
//                     effect Rallying Cry (Fighter) and the boss's own
//                     enrage both already use -- reused, not reinvented.
//   3. Vault Kick  -- the signature move: a melee-range kick that deals
//                     modest damage and launches the caster backwards,
//                     away from the target -- see Talent::retreatDistance
//                     for how the knockback itself works. Damage isn't
//                     the point; disengaging from melee range while
//                     still getting a hit in is.
//
// As of Prompt 23, two more are unlocked through play rather than known
// from the start -- see thiefTalentUnlockedAtLevel() below.
//
//   4. Piercing Shot -- unlocked at level 4. Reuses the Spellblade's
//                       Execution conditional-multiplier mechanic
//                       (Prompt 9) for the first time outside that
//                       class: modest ranged damage normally, triple
//                       against a target already below 30% hp.
//   5. Adrenaline     -- unlocked at level 7. A genuine upgrade on
//                       Steady Aim: a longer, stronger Empowered.
//
// See ARCHITECTURE_DECISIONS.md, "Archer and the vault mechanic" (that
// section's own title wasn't retroactively renamed -- it documents what
// was decided under that name at the time), for why Dexterity doesn't
// scale damage in this project (a real design question this class
// raised) and the retreat-direction reasoning.
std::vector<Talent> thiefTalents();

// Returns the talent unlocked at exactly this level, if any -- see
// FighterTalentUnlockedAtLevel's comment (the same reasoning applies
// here unchanged). nullopt for every level that isn't 4 or 7.
std::optional<Talent> thiefTalentUnlockedAtLevel(int level);

} // namespace engine
