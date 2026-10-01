#pragma once

#include "world/FloorTheme.hpp"

namespace engine {

// A floor's set piece: a hand-made module with an event at its altar (the S
// in its template), surrounded by procedural and hand-made cells. Values are
// saved, so only ever append new kinds.
enum class LandmarkKind { None = 0, Shrine = 1, HealingFountain = 2, BloodFont = 3, RitualCircle = 4,
                          TreasureHoard = 5, PrisonerCage = 6, ChampionPit = 7,
                          // Very rare, deep floors only; each leads to a unique item.
                          SealedTomb = 8, PalePeddler = 9, ChainedDemon = 10 };
inline constexpr int kLandmarkKindCount = 11;
inline bool rareLandmark(LandmarkKind kind) {
    return kind == LandmarkKind::SealedTomb || kind == LandmarkKind::PalePeddler || kind == LandmarkKind::ChainedDemon;
}
// Global floor from which the rare events can appear, and their chance per ordinary floor.
inline constexpr int kRareEventFloor = 7;
inline constexpr float kRareEventChance = 0.08f;

// The rare events' champions (Monster::eventChampion); saved, append only.
inline constexpr int kChampionRevenant = 1, kChampionDemon = 2, kEventChampionKinds = 2;
inline const char* championName(int champion) {
    return champion == kChampionDemon ? "The Unchained Demon" : champion == kChampionRevenant ? "The Risen King" : "";
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
        case LandmarkKind::None: break;
    }
    return "";
}

} // namespace engine
