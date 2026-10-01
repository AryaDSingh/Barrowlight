#pragma once

#include <cstdint>

namespace engine {
enum class FloorRegion { Barracks, Sanctum, Crypts };
struct ThemeColor { std::uint8_t r, g, b; };
struct FloorTheme {
    FloorRegion region;
    const char* name;
    const char* description;
    ThemeColor wall, floor, door;
};

// Derived from the already-saved floor number; no extra save state or RNG.
inline FloorTheme floorTheme(int floor) {
    if (floor<=3) return {FloorRegion::Barracks,"Barracks",
        "Warm stone halls. Patrols guard the route below.",
        {62,48,42},{111,91,72},{190,65,45}};
    if (floor<=6) return {FloorRegion::Sanctum,"Ruined Sanctum",
        "Broad, violet halls shelter shamans and their guards.",
        {47,41,66},{87,78,115},{200,75,135}};
    if (floor>10) return {FloorRegion::Crypts,"Deep Crypts",
        "Beyond the broken seal. Undead legions guard the Lich's final sanctum.",
        {28,28,49},{66,61,91},{160,90,195}};
    return {FloorRegion::Crypts,"Crypts",
        "Cold, narrow chambers. The dead stir near the Lich.",
        {31,49,53},{65,89,93},{65,180,155}};
}
} // namespace engine
