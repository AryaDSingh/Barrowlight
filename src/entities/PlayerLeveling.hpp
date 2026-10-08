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
// checking just once). Capped at Player::levelCap() (20, or 30 past the
// Lich): XP stops accumulating there, rather than piling up toward a
// level that will never come.
//
// Each level-up grants +1 max HP automatically (every class, no
// choice), +2 unspent attribute points (see
// Player::unspentAttributePoints), an ability point and a utility point
// (and a tree point at levels 5/10/15/25/30), and a full heal. Max HP
// grows two ways (this automatic +1, plus points put into Strength); max
// mana grows only from Intelligence the player chooses to spend.
void grantXp(Player& player, int amount);

} // namespace engine
