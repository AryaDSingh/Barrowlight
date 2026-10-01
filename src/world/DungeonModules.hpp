#pragma once

#include <string>
#include <vector>

namespace engine {

// A hand-authored chunk of dungeon. A floor is a 3x3 grid of these (see
// generateDungeon), in the spirit of Dark and Darker's module layouts:
// every module has the same footprint and the same four connection
// "sockets", so any module can sit in any grid cell next to any other.
//
// Authoring rules (enforced by validateModules(), run in dungeon_test):
//   * exactly kModuleWidth x kModuleHeight characters;
//   * the border is wall except the sockets, which are floor: columns
//     9-11 of the top/bottom rows and rows 5-7 of the left/right columns.
//     The generator seals any socket that doesn't lead to a neighbor;
//   * every floor tile is reachable from every socket;
//   * the 3x3 around each encounter anchor is floor.
//
// Characters:
//   #  wall          .  floor
//   A  floor + encounter anchor (a monster pack may spawn around it). If a
//      module has no A, its center tile is the anchor.
//   V  vault gate: wall until the player opens it (vault module only)
//   C  vault cache: center of the sealed 5x5 vault room (vault module only)
//   S  landmark altar: a solid object the player interacts with from beside
//      it (landmark modules only). Landmarks get no encounter unless an A
//      asks for one.
struct ModuleTemplate {
    std::string name;
    std::vector<std::string> rows;
};

inline constexpr int kModuleWidth = 21;
inline constexpr int kModuleHeight = 13;
inline constexpr int kModuleGrid = 3; // modules per side of the floor

// The pool regular grid cells draw from, without repeats on one floor, so
// it must hold at least kModuleGrid * kModuleGrid modules.
const std::vector<ModuleTemplate>& regularModules();
// Its 7x7 center is guaranteed floor (the boss spawns there).
const ModuleTemplate& bossModule();
// Holds the sealed vault room; its gate is exactly 3 tiles from the cache.
const ModuleTemplate& vaultModule();
// Set pieces with an event (each has one S). Index matches LandmarkKind - 1.
const std::vector<ModuleTemplate>& landmarkModules();

// One message per broken authoring rule across every module; empty when
// all modules are valid.
std::vector<std::string> validateModules();
// The same rules for one regular module (used to check procedural cells).
std::vector<std::string> validateRegularModule(const ModuleTemplate& module);

} // namespace engine
