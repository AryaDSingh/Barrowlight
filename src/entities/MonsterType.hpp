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
    GoblinWarlord, // BossBehavior, phase-based set-piece fight -- floor kFirstBossFloor
    Lich,          // LichBehavior, ranged + summons Skeleton minions -- floor kFinalFloor
    Skeleton,      // Chaser, plain melee -- Crypt inhabitant or Lich summon
    GoblinRaider,
    SkeletonArcher,
    SkeletonGuard,
    Bonecaller,
    GoblinCaptain, // named encounter: Grik the Packleader
    OssuaryWarden, // named encounter: Veyra the Ashkeeper
    GoblinBulwark, GoblinMedic, GoblinStalker, GoblinSlinger,
    CryptSentinel, GraveMender, CryptShade, FrostAcolyte,
};

inline bool isUniqueMonster(MonsterType type) {
    return type==MonsterType::GoblinCaptain || type==MonsterType::OssuaryWarden;
}
} // namespace engine
