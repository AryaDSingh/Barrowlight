#pragma once

#include <memory>

#include "core/Position.hpp"
#include "entities/Monster.hpp"
#include "entities/MonsterTier.hpp"
#include "entities/MonsterType.hpp"

namespace engine {

// Constructs a fully-configured Monster of the given type at `position`
// -- stats, glyph, and AIBehavior (parameterized with this type's own
// numbers) all live here in one place, so adding a 7th enemy type means
// adding one case here, not touching Application. This is the project's
// founding "data + which behavior objects get plugged in, not a new
// subclass" philosophy (Prompt 0) finally proven out with a real roster:
// 6 enemy types built from just 4 AIBehavior classes.
//
// `tier` (Prompt 22) defaults to Base -- every pre-Prompt-22 call site
// (including every existing test) keeps working completely unchanged,
// spawning exactly the Prompt 21-rebalanced numbers with no scaling
// applied at all. Elite/Nightmare scale both hp and damage from that
// same rebalanced baseline (see MonsterTier.hpp) and prepend a name
// prefix; the boss (GoblinWarlord) deliberately ignores `tier` entirely
// -- it's already a separately-tuned "hardest fight in the game," and
// scaling it further at high levels risks making the climactic fight
// absurd rather than harder.
std::unique_ptr<Monster> createMonster(MonsterType type, Position position,
                                        MonsterTier tier = MonsterTier::Base);

// How much XP defeating a monster of this type and tier grants the
// player (Prompt 20, extended for tiers at Prompt 22) --
// createMonster() calls this itself and sets it on the returned Monster
// via Actor::setXpReward(), so every caller automatically gets a
// monster with the right reward already attached; nothing else needs
// to know this function exists. Exposed separately (not just inlined
// into createMonster()) so it's independently testable and so anything
// that only needs the reward value, not a whole Monster, can call it
// directly.
int xpRewardForType(MonsterType type, MonsterTier tier = MonsterTier::Base);

} // namespace engine
