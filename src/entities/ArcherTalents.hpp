#pragma once

#include <vector>

#include "entities/Talent.hpp"

namespace engine {

// The Archer's full talent kit: 4 talents, the same "deliberately
// smaller than the Spellblade's 9" discipline established for the
// Marauder (Prompt 15). Pure Dexterity -- every talent is Physical
// (Strength-scaled, not Magic), since this class's Intelligence sits
// below baseline (a real, if modest, penalty) and there's no reason to
// touch a stat that actively works against you. Dexterity itself isn't
// a damage stat in this project's formula system (see
// AttributeFormulas.hpp) -- it governs dodge chance, capped at the
// Archer's own 30%, and nothing else -- so this class's identity comes
// from avoiding hits entirely, not out-damaging what it takes.
//
//   0. Quick Shot  -- the filler: ranged (no adjacency needed), cheap,
//                     fast. The Archer's bread and butter, same role
//                     Slam plays for the Marauder and Ember Bolt plays
//                     for the Spellblade, just ranged instead of melee.
//   1. Volley      -- ranged AoE: fires into a cluster rather than one
//                     target, the same AreaAroundTarget shape Fireball
//                     already uses, re-themed as arrows instead of fire.
//   2. Steady Aim  -- SelfBuff: applies Empowered, the same status
//                     effect Rallying Cry (Marauder) and the boss's own
//                     enrage both already use -- reused, not reinvented.
//   3. Vault Kick  -- the signature move: a melee-range kick that deals
//                     modest damage and launches the caster backwards,
//                     away from the target -- see Talent::retreatDistance
//                     for how the knockback itself works. Damage isn't
//                     the point; disengaging from melee range while
//                     still getting a hit in is.
//
// See ARCHITECTURE_DECISIONS.md, "Archer and the vault mechanic," for
// why Dexterity doesn't scale damage in this project (a real design
// question this class raised) and the retreat-direction reasoning.
std::vector<Talent> archerTalents();

} // namespace engine
