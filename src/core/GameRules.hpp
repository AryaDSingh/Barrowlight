#pragma once

// Constants shared by more than one Application*.cpp translation unit.
namespace engine {

// The multi-floor dungeon progression: floors 1 through kFinalFloor,
// each a separate generated dungeon reached by walking through the
// previous floor's door. Only kFirstBossFloor and kFinalFloor generate
// with a boss room at all -- see regenerateLevel(). kFinalFloor's boss
// currently reuses GoblinWarlord as a placeholder (see that function's
// own comment) -- the actual Lich is separate, later work.
inline constexpr int kFirstBossFloor = 5;
inline constexpr int kFinalFloor = 10;

// Relative to wherever the executable is launched from -- same
// reasoning as avoiding data/ file loading elsewhere in this project
// (see ARCHITECTURE_DECISIONS.md): resolving the executable's own
// directory needs platform-specific APIs this project has deliberately
// avoided needing so far.
inline constexpr const char* kSaveFilePath = "savegame.txt";

} // namespace engine
