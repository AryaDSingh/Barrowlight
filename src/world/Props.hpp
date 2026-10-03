#pragma once

#include "core/Position.hpp"
#include "world/FloorTheme.hpp"

namespace engine {

// Furniture and statuary that block movement but not sight or missiles: a
// prop tile is a non-walkable, transparent "wall" in the Map, drawn as the
// prop with floor under it. Values are saved, so only ever append new kinds.
// Oil barrels, braziers and cold braziers are interactive (ApplicationSurfaces.cpp).
enum class PropKind { Barrel = 1, Crate = 2, Sacks = 3, Throne = 4, SkeletonThrone = 5, Statue = 6,
                      OilBarrel = 7, Brazier = 8, ColdBrazier = 9,
                      StonePillar = 10 }; // raised by Earth magic; crumbles in time (and on reload)
inline constexpr int kPropKindMin = 1, kPropKindMax = 10;

struct Prop {
    PropKind kind;
    Position pos; // leftmost tile of its footprint
};

// Footprint width in tiles (props are one tile deep).
inline int propWidth(PropKind kind) { return kind == PropKind::Statue ? 2 : 1; }

inline const char* propName(PropKind kind) {
    switch (kind) {
        case PropKind::Barrel: return "Barrel";
        case PropKind::Crate: return "Crate";
        case PropKind::Sacks: return "Sacks";
        case PropKind::Throne: return "Stone throne";
        case PropKind::SkeletonThrone: return "Skeleton on a throne";
        case PropKind::Statue: return "Statue";
        case PropKind::OilBarrel: return "Oil barrel";
        case PropKind::Brazier: return "Brazier";
        case PropKind::ColdBrazier: return "Cold brazier";
        case PropKind::StonePillar: return "Stone pillar";
    }
    return "";
}

} // namespace engine
