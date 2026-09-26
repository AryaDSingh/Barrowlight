#pragma once

#include <vector>

#include "entities/PlayerClass.hpp"
#include "entities/Talent.hpp"
#include "entities/TalentSet.hpp"

namespace engine {

// Whether `cls` is eligible for the Fighter/Sorcerer hybrid path at all
// -- Prompt 24. True for exactly Fighter and Sorcerer, false for Thief
// and Spellblade. Fighter (pure Strength) and Sorcerer (pure
// Intelligence) each represent half of Spellblade's own Str+Int
// identity, so either growing into the other reads as a coherent
// hybrid; Thief (pure Dexterity) shares neither stat, so "a rogue
// suddenly gains both melee and magic aptitude" wouldn't follow the
// same logic. Spellblade itself has no opposing class to draw from.
bool isHybridEligible(PlayerClass cls);

// The class whose kit `cls` draws its hybrid picks from -- Sorcerer for
// Fighter, Fighter for Sorcerer. Only meaningful when
// isHybridEligible(cls) is true; returns `cls` itself otherwise (a
// harmless, inert default -- callers should always check
// isHybridEligible() first, same as every other tier/level lookup in
// this project checks its own preconditions rather than relying on a
// sentinel return value to signal "not applicable").
PlayerClass hybridPoolClass(PlayerClass cls);

// Every talent in `cls`'s full kit -- its starting talents plus
// level-gated unlocks (Mage: 2/4/7; others: 4/7, see
// PlayerClassFactory::talentUnlockedAtLevel) -- regardless of what
// level a character actually is. Used as the hybrid pool's source list;
// a Fighter specced into the hybrid path can eventually pick every
// Sorcerer ability that exists, not just the ones a level-appropriate
// Sorcerer would currently have unlocked.
std::vector<Talent> fullKitForClass(PlayerClass cls);

// The abilities still available to pick from the hybrid pool, given
// what's already known -- every talent in fullKitForClass(hybridPoolClass(cls))
// whose stable ID doesn't already appear in `known`. Ordered the same way
// the opposing class's own kit is ordered, so the choice screen
// presents the same stable sequence across repeated picks rather than
// shuffling. Mage now has seven talents; six hybrid picks do not exhaust that pool.
std::vector<Talent> availableHybridPicks(PlayerClass cls, const TalentSet& known);

// The inverse of availableHybridPicks(): every talent from the hybrid
// pool that *has* already been picked (appears in both the pool and
// `known`). Identity comparison uses IDs; saves now persist the entire learned kit.
std::vector<Talent> pickedHybridTalents(PlayerClass cls, const TalentSet& known);

} // namespace engine
