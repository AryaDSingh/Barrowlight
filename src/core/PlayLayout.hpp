#pragma once

namespace engine::playLayout {
// Play screen, ToME-style: a character column on the left, the map filling
// the rest, and a bottom strip with the message log and the talent hotbar.
inline constexpr unsigned int windowWidth = 1280;
inline constexpr unsigned int windowHeight = 720;
inline constexpr int tileSize = 28;

inline constexpr int sidebarWidth = 256;
inline constexpr int mapLeft = 266;
inline constexpr int mapTop = 10;
inline constexpr int mapWidth = 1008;  // 36 columns
inline constexpr int mapHeight = 532;  // 19 rows: the complete radius-8 FOV fits.

inline constexpr float bottomTop = 552.f;
inline constexpr float logX = 266.f, logY = 562.f, logWidth = 404.f, logLineHeight = 18.f;
inline constexpr int logLines = 8;

inline constexpr float hotbarX = 686.f, hotbarY = 590.f, hotbarStride = 56.f, hotbarWidth = 52.f, hotbarHeight = 52.f;
inline constexpr float pageButtonX = 1192.f, pageButtonWidth = 38.f, pageButtonHeight = 26.f;

// Status effect icons in the character column, five per row, two rows.
inline constexpr float statusX = 14.f, statusY = 270.f, statusStride = 46.f, statusSize = 40.f;
inline constexpr int statusColumns = 5, statusRows = 2;

inline constexpr float minimapY = 392.f;

// Action buttons (bag, trees, ...) at the foot of the character column.
inline constexpr float actionX = 12.f, actionY = 616.f, actionStrideX = 39.f, actionStrideY = 50.f,
                       actionWidth = 36.f, actionHeight = 44.f;
inline constexpr int actionColumns = 6;
} // namespace engine::playLayout
