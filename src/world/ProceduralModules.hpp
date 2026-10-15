#pragma once

#include <random>
#include <vector>

#include "world/DungeonModules.hpp"
#include "world/FloorTheme.hpp"

namespace engine {

// Generates one room instead of picking a hand-made one: a single grid cell,
// or a big room two cells wide or high. Styles:
//   Hall    - open floor with a loose lattice of pillars
//   Rooms   - interior walls with several doorways, so rooms are never dead ends
//   Ruins   - open ground littered with collapsed wall chunks
//   Cavern  - cellular-automaton cave, with clear lanes to every doorway
// The floor's region weights the choice (barracks favour rooms, crypts
// caverns, the sanctum halls). Every result obeys the same authoring rules
// as a hand-made module -- open doorways, full connectivity, a clear centre
// for encounters -- and is at least about half floor, so floors stay open.
enum class ProceduralStyle { Hall, Rooms, Ruins, Cavern };

const char* proceduralStyleName(ProceduralStyle style);
ProceduralStyle pickProceduralStyle(FloorRegion region, std::mt19937& rng);
// A room cellsWide x cellsHigh grid cells in size whose border is wall except
// the given doorways, each of which leads into the room.
ModuleTemplate proceduralModule(ProceduralStyle style, int cellsWide, int cellsHigh,
                                const std::vector<Doorway>& doorways, std::mt19937& rng);
// One cell with the standard central sockets on all four sides.
ModuleTemplate proceduralModule(ProceduralStyle style, std::mt19937& rng);

} // namespace engine
