#pragma once
#include "core/Position.hpp"
#include "entities/MonsterType.hpp"
namespace engine {
inline bool enemyTank(MonsterType t) {
    return t==MonsterType::GoblinBulwark || t==MonsterType::CryptSentinel || t==MonsterType::SkeletonGuard || t==MonsterType::Ogre;
}
inline bool enemyHealer(MonsterType t) { return t==MonsterType::GoblinMedic || t==MonsterType::GraveMender; }
inline bool enemyAmbusher(MonsterType t) { return t==MonsterType::GoblinStalker || t==MonsterType::CryptShade; }
inline bool enemyBackline(MonsterType t) {
    return enemyHealer(t) || t==MonsterType::Archer || t==MonsterType::SkeletonArcher || t==MonsterType::Shaman ||
        t==MonsterType::Bonecaller || t==MonsterType::GoblinSlinger || t==MonsterType::FrostAcolyte || t==MonsterType::OrcFirebrand || t==MonsterType::DrownedChorister;
}
// Roaming threats (core/ApplicationRoaming.cpp). Saved; append only.
//   Patrol: a pack that walks the whole floor, room to room.
//   Champion: a wandering brute with an escort, named on arrival.
//   Hunter: sent after you when you linger; always knows where you are.
// Nemesis (format 46): a foe you fled, named, that follows you down and hunts you.
enum class Roam : int { None = 0, Patrol = 1, Champion = 2, Hunter = 3, Nemesis = 4 };
inline constexpr int kRoamKinds = 4;
struct EnemyTactics {
    Position home{}, lastKnown{};
    int alert=0, patrol=0, retreat=0, heals=3;
    bool retreated=false, concealed=false;
};
}
