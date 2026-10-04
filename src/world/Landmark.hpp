#pragma once

#include <algorithm>
#include <cstdint>

#include "entities/MonsterType.hpp"

#include "world/FloorTheme.hpp"

namespace engine {

// A floor's set piece: a hand-made module with an event at its altar (the S
// in its template), surrounded by procedural and hand-made cells. Values are
// saved, so only ever append new kinds.
enum class LandmarkKind { None = 0, Shrine = 1, HealingFountain = 2, BloodFont = 3, RitualCircle = 4,
                          TreasureHoard = 5, PrisonerCage = 6, ChampionPit = 7,
                          // Very rare, deep floors only; each leads to a unique item.
                          SealedTomb = 8, PalePeddler = 9, ChainedDemon = 10,
                          // An uncommon find on any floor: where the lantern comes from.
                          LamplighterRest = 11,
                          // From floor 4: a Vampire Lord sleeps beside it; slay him and
                          // offer your blood to learn Blood Magic.
                          BloodAltar = 12,
                          // Path of Exile's strongbox: open it and foes burst out
                          // around you while it spills its loot.
                          Strongbox = 13,
                          // Path of Exile's Breach: a rift that pours out creatures
                          // until its keeper falls or it closes.
                          Breach = 14,
                          // Path of Exile's Essence: a monster sealed in a crystal you
                          // strike three times to free it.
                          Essence = 15 };
// Events you simply touch, with no menu: what happens shows what they are.
inline bool touchedLandmark(LandmarkKind kind) {
    return kind == LandmarkKind::Strongbox || kind == LandmarkKind::Breach || kind == LandmarkKind::Essence;
}
inline constexpr int kCrystalStrikes = 3;
// Landmarks with one thing to do: touching them does it, no menu.
inline bool singleUseLandmark(LandmarkKind kind) {
    return kind == LandmarkKind::HealingFountain || kind == LandmarkKind::BloodFont || kind == LandmarkKind::RitualCircle ||
           kind == LandmarkKind::LamplighterRest || kind == LandmarkKind::SealedTomb || kind == LandmarkKind::BloodAltar;
}
inline constexpr int kLandmarkKindCount = 16;
inline constexpr int kBreachTurns = 14;

// The power sealed in a crystal: what its captive does, and the affix its loot
// bears. Named only for how it looks.
enum class Essence : int { None = 0, Flame = 1, Frost = 2, Storms = 3, Haste = 4 };
inline constexpr int kEssenceKinds = 4;
struct EssenceInfo { const char* crystal; const char* look; const char* affix; std::uint8_t r, g, b; };
inline const EssenceInfo& essenceInfo(Essence essence) {
    static const EssenceInfo kInfo[]{
        {"Crystal", "", "", 255, 255, 255},
        {"Smouldering Crystal", "Smouldering", "embers", 255, 130, 60},
        {"Rimed Crystal", "Rimed", "frost", 130, 200, 255},
        {"Crackling Crystal", "Crackling", "storms", 190, 180, 255},
        {"Restless Crystal", "Restless", "nimble", 150, 255, 170}};
    return kInfo[std::clamp(static_cast<int>(essence), 0, kEssenceKinds)];
}
// A dark, red-veined crystal: its captive comes out a nightmare.
inline constexpr const char* kWeepingCrystal = "Weeping Crystal";
struct Captive { Essence essence; MonsterType type; bool corrupted; };
// A strongbox's kind, fixed by where it stands (Application::strongboxVariant).
enum class StrongboxVariant { Armourer, Arcanist, Gilded };
// Global floor from which the Blood Altar can appear.
inline constexpr int kBloodAltarFloor = 4;
inline bool rareLandmark(LandmarkKind kind) {
    return kind == LandmarkKind::SealedTomb || kind == LandmarkKind::PalePeddler || kind == LandmarkKind::ChainedDemon;
}
// Global floor from which the rare events can appear, and their chance per ordinary floor.
inline constexpr int kRareEventFloor = 7;
inline constexpr float kRareEventChance = 0.08f;

// The rare events' champions (Monster::eventChampion); saved, append only.
// The trial guardians (entities/Ascendancy.hpp) use the same marker.
inline constexpr int kChampionRevenant = 1, kChampionDemon = 2, kChampionStoneWarden = 3, kChampionFallenSaint = 4,
                     kChampionVampire = 5, kEventChampionKinds = 5;
inline bool trialGuardianChampion(int champion) { return champion == kChampionStoneWarden || champion == kChampionFallenSaint; }
inline const char* championName(int champion) {
    switch (champion) {
        case kChampionRevenant: return "The Risen King";
        case kChampionDemon: return "The Unchained Demon";
        case kChampionStoneWarden: return "The Stone Warden";
        case kChampionFallenSaint: return "The Fallen Saint";
        case kChampionVampire: return "The Vampire Lord";
        default: return "";
    }
}

// Fountains are gargoyles set into a wall face, so their altar tile is drawn
// as wall; the shrine's statue and the ritual circle stand on open floor.
inline bool landmarkAltarIsFloor(LandmarkKind kind) {
    return kind != LandmarkKind::HealingFountain && kind != LandmarkKind::BloodFont && kind != LandmarkKind::None;
}

inline const char* landmarkName(LandmarkKind kind, FloorRegion region) {
    const bool barracks = region == FloorRegion::Barracks, sanctum = region == FloorRegion::Sanctum;
    switch (kind) {
        case LandmarkKind::Shrine: return barracks ? "Soldiers' Shrine" : sanctum ? "Ruined Altar" : "Bone Shrine";
        case LandmarkKind::HealingFountain: return barracks ? "Spring of the Watch" : sanctum ? "Sacred Spring" : "Weeping Spring";
        case LandmarkKind::BloodFont: return barracks ? "Bleeding Gargoyle" : sanctum ? "Font of Sacrifice" : "Blood Font";
        case LandmarkKind::RitualCircle: return barracks ? "Forbidden Circle" : sanctum ? "Ritual Circle" : "Lich's Circle";
        case LandmarkKind::TreasureHoard: return barracks ? "Quartermaster's Hoard" : sanctum ? "Tithe Hoard" : "Grave Goods";
        case LandmarkKind::PrisonerCage: return barracks ? "Deserter's Cage" : sanctum ? "Heretic's Cage" : "Gibbet Cage";
        case LandmarkKind::ChampionPit: return barracks ? "Proving Pit" : sanctum ? "Trial Circle" : "Bone Arena";
        case LandmarkKind::SealedTomb: return barracks ? "Tomb of the Last Captain" : sanctum ? "Tomb of the Fallen Saint" : "Tomb of the First King";
        case LandmarkKind::PalePeddler: return "The Pale Peddler";
        case LandmarkKind::ChainedDemon: return "The Chained Demon";
        case LandmarkKind::LamplighterRest: return "Lamplighter's Rest";
        case LandmarkKind::BloodAltar: return "The Blood Altar";
        case LandmarkKind::Strongbox: return "Strongbox";
        case LandmarkKind::Breach: return "Breach";
        case LandmarkKind::Essence: return "Essence";
        case LandmarkKind::None: break;
    }
    return "";
}

} // namespace engine
