#pragma once

#include "world/FloorTheme.hpp"

namespace engine {

// A floor's set piece: a hand-made module with an event at its altar (the S
// in its template), surrounded by procedural and hand-made cells. Values are
// saved, so only ever append new kinds.
enum class LandmarkKind { None = 0, Shrine = 1 };
inline constexpr int kLandmarkKindCount = 2;

inline const char* landmarkName(LandmarkKind kind, FloorRegion region) {
    switch (kind) {
        case LandmarkKind::Shrine:
            return region == FloorRegion::Barracks ? "Soldiers' Shrine"
                 : region == FloorRegion::Sanctum ? "Ruined Altar" : "Bone Shrine";
        case LandmarkKind::None: break;
    }
    return "";
}

} // namespace engine
