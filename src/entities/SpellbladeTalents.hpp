#pragma once

#include <vector>

#include "entities/Talent.hpp"

namespace engine {

// The Spellblade's full talent kit: 8 talents across two trees.
//
// Blade (melee, must be adjacent -- positional risk, cheap/efficient):
//   0. Quick Strike   -- cheap, fast, modest damage. The filler.
//   1. Power Strike    -- cooldown vs damage trade-off against Quick
//                         Strike: much more damage, can't be spammed.
//   2. Reckless Lunge   -- risk vs reward: costs the caster's own hp (not
//                          just mana) for very high single-target damage.
//   3. Execution        -- tactical/sequencing: modest damage normally,
//                          but a large multiplier against a target
//                          already below 30% hp. Rewards good timing
//                          over raw stats.
//
// Flame (ranged/AoE, mana-hungry, safer positioning but costlier):
//   4. Ember Bolt       -- the tree's filler: ranged, no adjacency needed.
//   5. Fireball         -- AoE vs single-target trade-off against Ember
//                          Bolt: hits an area around the target, at a
//                          much higher mana cost and cooldown.
//   6. Blink            -- pure utility: no damage, relocates the caster.
//                          A different kind of decision than "deal
//                          damage," not a worse one.
//   7. Immolate         -- a second, differently-shaped AoE: centered on
//                          the caster rather than a chosen target.
//
// See ARCHITECTURE_DECISIONS.md for why this is a C++ data table rather
// than an external file, and why none of these rely on ongoing
// buffs/debuffs (StatusEffects doesn't exist yet -- Prompt 10).
std::vector<Talent> spellbladeTalents();

} // namespace engine
