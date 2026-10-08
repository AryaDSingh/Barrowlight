#pragma once

#include <memory>

#include "core/Position.hpp"
#include "entities/Monster.hpp"
#include "entities/MonsterTier.hpp"
#include "entities/MonsterType.hpp"

namespace engine {

// Constructs a fully-configured Monster of the given type at `position`
// -- stats, glyph, and AIBehavior (parameterized with this type's own
// numbers) all live here in one place, so a new enemy type means one case
// here. This is the project's founding rule at work: a monster is data
// plus which behaviour objects get plugged in, never a new subclass. Over
// fifty enemy types are built from a handful of AIBehavior classes.
//
// `tier` defaults to Base. Elite/Nightmare scale hp and damage (see
// MonsterTier.hpp) and prepend a name prefix; bosses ignore `tier`, since
// they are tuned as set pieces and scaling them further would make the
// climactic fight absurd rather than harder.
std::unique_ptr<Monster> createMonster(MonsterType type, Position position,
                                        MonsterTier tier = MonsterTier::Base);

// How much XP defeating a monster of this type and tier grants the
// player --
// createMonster() calls this itself and sets it on the returned Monster
// via Actor::setXpReward(), so every caller automatically gets a
// monster with the right reward already attached; nothing else needs
// to know this function exists. Exposed separately (not just inlined
// into createMonster()) so it's independently testable and so anything
// that only needs the reward value, not a whole Monster, can call it
// directly.
int xpRewardForType(MonsterType type, MonsterTier tier = MonsterTier::Base);

} // namespace engine
