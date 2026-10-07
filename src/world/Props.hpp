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
                      StonePillar = 10, // raised by Earth magic; crumbles in time (and on reload)
                      // Furniture for lived-in rooms (DungeonGenerator's vignettes).
                      Table = 11, Chair = 12, Bookcase = 13, Sarcophagus = 14, Idol = 15,
                      Furnace = 16 }; // the Ashen Foundry's: heat in the 8 tiles around it
inline constexpr int kPropKindMin = 1, kPropKindMax = 16;

struct Prop {
    PropKind kind;
    Position pos; // leftmost tile of its footprint
};

// Footprint width in tiles (props are one tile deep).
inline int propWidth(PropKind kind) { return kind == PropKind::Statue || kind == PropKind::Table ? 2 : 1; }

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
        case PropKind::Table: return "Table";
        case PropKind::Chair: return "Chair";
        case PropKind::Bookcase: return "Bookcase";
        case PropKind::Sarcophagus: return "Sarcophagus";
        case PropKind::Idol: return "Idol";
        case PropKind::Furnace: return "Furnace";
    }
    return "";
}

// What's left lying on the floor of a lived-in room: walkable, purely
// visual, saved with the floor. Values are saved: append only.
enum class DecalKind { Bones = 1, Skull = 2, Web = 3, Dirt = 4, Coins = 5, Weapon = 6, Book = 7, Debris = 8, Candle = 9, Pick = 10 };
inline constexpr int kDecalKindMax = 10;
struct Decal {
    DecalKind kind;
    Position pos;
};

} // namespace engine
