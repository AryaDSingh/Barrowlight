#pragma once

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
                          BloodAltar = 12 };
inline constexpr int kLandmarkKindCount = 13;
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
        case LandmarkKind::None: break;
    }
    return "";
}

} // namespace engine
