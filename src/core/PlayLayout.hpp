#pragma once

namespace engine::playLayout {
inline constexpr unsigned int windowWidth = 1280;
inline constexpr unsigned int windowHeight = 720;
inline constexpr int tileSize = 28;
inline constexpr int mapWidth = 896;
inline constexpr int mapTop = 88;
inline constexpr int mapHeight = 504; // 18 rows: the complete radius-8 FOV fits.
inline constexpr float hotbarX=10.f, hotbarY=660.f, hotbarStride=78.f, hotbarWidth=74.f, hotbarHeight=52.f;
inline constexpr float statusY=594.f, statusStride=78.f, statusHeight=24.f;
} // namespace engine::playLayout
