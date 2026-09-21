#pragma once

#include <vector>

#include "entities/Talent.hpp"

namespace engine {

// The Marauder's full talent kit: 4 talents, deliberately fewer and
// simpler than the Spellblade's 9 -- this class's identity is "pure
// Strength, hp as the resource that matters," not a second sprawling
// spell list. All four are Physical (Strength-scaled); there is no
// second tree the way the Spellblade has Blade/Flame, since there's
// nothing here that scales with a different attribute.
//
//   0. Slam            -- the filler: zero mana cost (not just cheap --
//                          free), a 1-turn cooldown, spammable. The
//                          Marauder's bread and butter; strong enough to
//                          be worth using every turn on its own, since
//                          it costs nothing to do so.
//   1. Cleave          -- AreaAroundSelf, hits everything adjacent, not
//                          just one target -- genuinely useful now that
//                          fights can involve more than one enemy at
//                          once (Prompt 10's roster). Small mana cost
//                          from an otherwise-small pool.
//   2. Rallying Cry     -- SelfBuff: applies Empowered to the caster,
//                          the same status effect the boss's own enrage
//                          uses (Prompt 11) -- reused, not reinvented.
//   3. Berserker's Fury -- risk vs reward, blood magic: costs a real
//                          chunk of the caster's own hp (not mana) for
//                          the hardest-hitting single-target strike in
//                          either class's kit. The same "spend your own
//                          blood" precedent Reckless Lunge set for the
//                          Spellblade (Prompt 9), leaned into harder.
//
// See ARCHITECTURE_DECISIONS.md, "Multi-class system and Marauder," for
// the resource-economy reasoning: a free spammable basic attack, a
// small mana pool for the rest, and a cooldown-gated blood cost for the
// strongest option -- a genuinely different rhythm from the Spellblade's
// constant mana-juggling across 9 talents, not just a reskinned copy of
// it with different numbers.
std::vector<Talent> marauderTalents();

} // namespace engine
