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
struct EnemyTactics {
    Position home{}, lastKnown{};
    int alert=0, patrol=0, retreat=0, heals=3;
    bool retreated=false, concealed=false;
};
}
