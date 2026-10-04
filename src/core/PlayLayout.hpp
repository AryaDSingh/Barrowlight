#pragma once

namespace engine::playLayout {
// Full-screen menus are designed at 1280x720 and centred (letterboxed).
inline constexpr unsigned int windowWidth = 1280;
inline constexpr unsigned int windowHeight = 720;
inline constexpr int tileSize = 28;

// The play screen is 720 units tall and as wide as the window's shape (at
// least 1280), so an ultrawide monitor simply shows more dungeon. The map
// fills all of it; the HUD floats over its edges (Application::fitView sets
// these).
inline float screenWidth = 1280.f;
inline float mapLeft = 0.f, mapTop = 0.f, mapWidth = 1280.f, mapHeight = 720.f;

// --- HUD: corners and orbs ---------------------------------------------------
// Top left: portrait, class and level, then status icons and a few lines.
inline constexpr float portraitSize = 52.f, hudMargin = 10.f;
inline constexpr float statusX = 10.f, statusY = 70.f, statusStride = 34.f, statusSize = 30.f;
inline constexpr int statusColumns = 8, statusRows = 2;
// Bottom corners: the life and mana orbs.
inline constexpr float orbRadius = 54.f, orbMargin = 14.f;
// Bottom centre: one row of nine talent slots, an experience strip above it.
inline constexpr float hotbarStride = 50.f, hotbarWidth = 46.f, hotbarHeight = 46.f, hotbarY = 662.f;
inline constexpr float xpStripY = 653.f, xpStripHeight = 5.f;
// Beside the hotbar: two rows of small action icons.
inline constexpr float actionStride = 34.f, actionSize = 30.f, actionY = 646.f;
inline constexpr int actionColumns = 6;
// Top right: the minimap.
inline constexpr float minimapWidth = 210.f, minimapHeight = 136.f;
// Left, above the life orb: the log, which fades when nothing happens.
inline constexpr float logX = 12.f, logBottom = 588.f, logWidth = 440.f, logLineHeight = 17.f;
inline constexpr int logLines = 7;
inline constexpr float logFadeSeconds = 10.f;
} // namespace engine::playLayout
