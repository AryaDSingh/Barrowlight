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
    // Creatures of light, dark, fire and water (save format 31).
    Torchbearer,   // cultist with a torch: lights the dark, rekindles fires, burns on hit
    Gloomstalker,  // shadow beast: deadly in the dark, weak and hurt in light
    OrcFirebrand,  // throws flasks of burning oil
    DrownedOne,    // crypt undead that leaves water behind and heals in it
    // The Drowned Cathedral (save format 35).
    DeepLurker,       // hides in water, and drags you in after it
    DrownedChorister, // a singing spirit whose bolts charge the water you stand in
    TheSleeper,       // the Cathedral's boss: floods, charges the water, heals in it
    Mimic,            // a chest that bites (save format 41)
    // The Ashen Foundry (save format 43).
    OrcSmith,         // hammer melee; each hit heats you
    SlagGolem,        // slow and tough; splits into two Slaglings when it dies
    Slagling,         // small and quick; leaves the ground burning where it dies
    BellowsImp,       // fans the fires around it, and blows heat at you from range
    Forgemaster,      // the Foundry's boss
};

// Darkvision: goblinkind, orcs, beasts and the undead see without light.
// The humans among the enemies (archers, monks, clerics, the warden) need
// light to spot anyone further than arm's reach.
inline bool seesInDark(MonsterType type) {
    switch (type) {
        case MonsterType::Archer: case MonsterType::Bonecaller: case MonsterType::GraveMender:
        case MonsterType::FrostAcolyte: case MonsterType::OssuaryWarden: case MonsterType::Torchbearer: return false;
        default: return true;
    }
}

// Who leaves blood behind: the living do, bones and spirits don't.
inline bool bleeds(MonsterType type) {
    switch (type) {
        case MonsterType::Skeleton: case MonsterType::SkeletonArcher: case MonsterType::SkeletonGuard:
        case MonsterType::CryptShade: case MonsterType::CryptSentinel: case MonsterType::Lich:
        case MonsterType::Gloomstalker: case MonsterType::DrownedOne: case MonsterType::DrownedChorister: case MonsterType::TheSleeper: case MonsterType::Mimic:
        case MonsterType::SlagGolem: case MonsterType::Slagling: case MonsterType::Forgemaster: return false;
        default: return true;
    }
}

inline bool isUniqueMonster(MonsterType type) {
    return type==MonsterType::GoblinCaptain || type==MonsterType::OssuaryWarden;
}
} // namespace engine
