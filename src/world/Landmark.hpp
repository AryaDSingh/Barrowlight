#pragma once

#include "world/FloorTheme.hpp"

namespace engine {

// A floor's set piece: a hand-made module with an event at its altar (the S
// in its template), surrounded by procedural and hand-made cells. Values are
// saved, so only ever append new kinds.
enum class LandmarkKind { None = 0, Shrine = 1, HealingFountain = 2, BloodFont = 3, RitualCircle = 4 };
inline constexpr int kLandmarkKindCount = 5;

// Fountains are gargoyles set into a wall face, so their altar tile is drawn
// as wall; the shrine's statue and the ritual circle stand on open floor.
inline bool landmarkAltarIsFloor(LandmarkKind kind) {
    return kind == LandmarkKind::Shrine || kind == LandmarkKind::RitualCircle;
}

inline const char* landmarkName(LandmarkKind kind, FloorRegion region) {
    const bool barracks = region == FloorRegion::Barracks, sanctum = region == FloorRegion::Sanctum;
    switch (kind) {
        case LandmarkKind::Shrine: return barracks ? "Soldiers' Shrine" : sanctum ? "Ruined Altar" : "Bone Shrine";
        case LandmarkKind::HealingFountain: return barracks ? "Spring of the Watch" : sanctum ? "Sacred Spring" : "Weeping Spring";
        case LandmarkKind::BloodFont: return barracks ? "Bleeding Gargoyle" : sanctum ? "Font of Sacrifice" : "Blood Font";
        case LandmarkKind::RitualCircle: return barracks ? "Forbidden Circle" : sanctum ? "Ritual Circle" : "Lich's Circle";
        case LandmarkKind::None: break;
    }
    return "";
}

} // namespace engine
