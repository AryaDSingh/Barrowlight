#pragma once

#include <memory>

#include "core/Position.hpp"
#include "entities/Monster.hpp"

namespace engine {

enum class MonsterType {
    Goblin, // Chaser, plain melee
    Spider, // Chaser, melee + Poison on-hit
    Ogre,   // Chaser, melee + chance to Stun on-hit
    Archer, // Kiter, ranged, maintains distance
    Shaman, // Support, buffs allies, never attacks
    Bomber, // AoEBomber, ranged AoE on a cooldown
};

// Constructs a fully-configured Monster of the given type at `position`
// -- stats, glyph, and AIBehavior (parameterized with this type's own
// numbers) all live here in one place, so adding a 7th enemy type means
// adding one case here, not touching Application. This is the project's
// founding "data + which behavior objects get plugged in, not a new
// subclass" philosophy (Prompt 0) finally proven out with a real roster:
// 6 enemy types built from just 4 AIBehavior classes.
std::unique_ptr<Monster> createMonster(MonsterType type, Position position);

} // namespace engine
