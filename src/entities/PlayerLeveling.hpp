#pragma once

#include "entities/Player.hpp"

namespace engine {

// How much XP it takes to go from `currentLevel` to `currentLevel + 1`
// -- not cumulative from level 1, just the cost of this one step.
// Deliberately simple, not a final, tuned curve: 20 * currentLevel, so
// each level costs 20 more than the last (20, 40, 60, ..., 180 to go
// from 9 to 10). Pure and fully testable; the actual XP-granting/
// level-up side effects live in grantXp() below, not here.
int xpForNextLevel(int currentLevel);

// Applies `amount` XP to `player`, handling however many level-ups it
// triggers (a single large XP grant -- e.g., the boss's reward -- can
// cross more than one threshold at once, so this loops rather than
// checking just once). Capped at level 10: XP stops accumulating once
// there, rather than piling up toward a level that will never come.
//
// Each level-up grants +1 max HP automatically (every class, no
// choice), +2 unspent attribute points (see
// Player::unspentAttributePoints), and a full heal to current hp/mana.
// Rewritten from the original flat "+3 maxHp/+2 maxMana, no player
// input at all" placeholder -- max HP growth now happens two ways (this
// automatic +1, plus however many points a player chooses to put into
// Strength), and max mana no longer grows automatically at all -- it
// only grows from Intelligence points a player actually chooses to
// spend. See ARCHITECTURE_DECISIONS.md for the full attribute-system
// redesign this is part of.
void grantXp(Player& player, int amount);

} // namespace engine
