#pragma once

#include <optional>
#include <vector>

#include "entities/Talent.hpp"

namespace engine {

// The Warrior's starting talent kit: 4 talents, deliberately fewer and
// simpler than the Spellblade's 9 -- this class's identity is "pure
// Strength, hp as the resource that matters," not a second sprawling
// spell list. All four are Physical (Strength-scaled); there is no
// second tree the way the Spellblade has Blade/Flame, since there's
// nothing here that scales with a different attribute.
//
//   0. Slam            -- the filler: zero mana cost (not just cheap --
//                          free), a 1-turn cooldown, spammable. The
//                          Warrior's bread and butter; strong enough to
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
// As of Prompt 23, two more are unlocked through play rather than known
// from the start -- see warriorTalentUnlockedAtLevel() below.
//
//   4. Whirlwind        -- unlocked at level 4. A genuine upgrade on
//                          Cleave, not a reskin: radius 2 instead of 1
//                          (hits a wider spread of enemies) and more
//                          damage per hit, at a real mana/cooldown cost
//                          to match.
//   5. Undying Rage      -- unlocked at level 7. A genuine upgrade on
//                          Rallying Cry: a longer, stronger Empowered.
//
// Originally "Marauder" (Prompt 15), renamed to Fighter at Prompt 19 as
// part of restructuring around three pure "base classes," one per
// attribute, then to Warrior at Prompt 26 as part of the attribute-
// system redesign. This file and its functions (fighterTalents() ->
// warriorTalents(), etc.) kept the old Fighter-era names for a while
// after that rename -- a deliberate scope cut at the time, not an
// oversight -- until being brought in line at Prompt 30. See
// ARCHITECTURE_DECISIONS.md, "Multi-class system and Marauder" (that
// section's own title wasn't retroactively renamed -- it documents
// what was decided under that name at the time) for the
// resource-economy reasoning: a free spammable basic attack, a
// small mana pool for the rest, and a cooldown-gated blood cost for the
// strongest option -- a genuinely different rhythm from the Spellblade's
// constant mana-juggling across 9 talents, not just a reskinned copy of
// it with different numbers.
std::vector<Talent> warriorTalents();

// Returns the talent unlocked at exactly this level, if any -- called
// once per level crossed on level-up (see
// Application::grantXpAndAnnounce), not just once per grantXp() call,
// since a single big XP grant can cross several levels at once (Prompt
// 20). nullopt for every level that isn't 4 or 7.
std::optional<Talent> warriorTalentUnlockedAtLevel(int level);

} // namespace engine
