#pragma once

#include <cstdint>

namespace engine {

// Ground surfaces, Divinity-style: one per floor tile, reacting to the
// elements. Fire ignites oil and melts ice; cold freezes water and puts
// out fire; lightning electrifies connected water. Values are saved, so
// only ever append new kinds.
enum class SurfaceType : std::uint8_t { None = 0, Oil = 1, Water = 2, Fire = 3, Ice = 4, Electrified = 5, Blood = 6, Acid = 7, Gas = 8 };
inline constexpr int kSurfaceTypeMax = 8;
// Water and blood both carry lightning and freeze.
inline bool conducts(SurfaceType t) { return t == SurfaceType::Water || t == SurfaceType::Blood || t == SurfaceType::Electrified; }

struct SurfaceTile {
    SurfaceType type = SurfaceType::None;
    int turns = 0; // fire and electrified water burn out; the rest last
};

// What touches the ground when an ability lands.
enum class Element { None, Fire, Ice, Lightning, Arrow };

// How long burning ground lasts: oil feeds it for longer.
inline constexpr int kOilFireTurns = 6, kSpilledFireTurns = 4, kElectrifiedTurns = 3;

inline const char* surfaceName(SurfaceType type) {
    switch (type) {
        case SurfaceType::Oil: return "Oil";
        case SurfaceType::Water: return "Water";
        case SurfaceType::Fire: return "Burning ground";
        case SurfaceType::Ice: return "Ice";
        case SurfaceType::Electrified: return "Electrified water";
        case SurfaceType::Blood: return "Blood";
        case SurfaceType::None: break;
    }
    return "";
}

inline const char* surfaceHint(SurfaceType type) {
    switch (type) {
        case SurfaceType::Oil: return "Fire ignites it into a blaze that spreads along the slick.";
        case SurfaceType::Water: return "Lightning electrifies the whole pool; cold freezes it.";
        case SurfaceType::Fire: return "Burns whoever stands in it and lights the dark. Cold puts it out.";
        case SurfaceType::Ice: return "Chills whoever stands on it. Fire melts it back to water.";
        case SurfaceType::Electrified: return "Shocks and hurts whoever stands in it.";
        case SurfaceType::Blood: return "Carries lightning like water; cold freezes it.";
        case SurfaceType::None: break;
    }
    return "";
}

} // namespace engine
