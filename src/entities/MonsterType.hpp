#pragma once

namespace engine {

// Extracted into its own header rather than living in MonsterFactory.hpp
// (where it started) -- Monster now needs to remember its own type (so
// save/load can reconstruct it via createMonster()), and MonsterFactory.hpp
// already includes Monster.hpp, so defining the enum there would create a
// circular include.
enum class MonsterType {
    Goblin,        // Chaser, plain melee
    Spider,        // Chaser, melee + Poison on-hit
    Ogre,          // Chaser, melee + chance to Stun on-hit
    Archer,        // Kiter, ranged, maintains distance
    Shaman,        // Support, buffs allies, never attacks
    Bomber,        // AoEBomber, ranged AoE on a cooldown
    GoblinWarlord, // BossBehavior, phase-based set-piece fight
};

} // namespace engine
