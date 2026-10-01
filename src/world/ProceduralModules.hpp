#pragma once

#include <random>

#include "world/DungeonModules.hpp"
#include "world/FloorTheme.hpp"

namespace engine {

// Generates one module-sized cell (see DungeonModules.hpp) instead of
// picking a hand-made one. Styles:
//   Hall    - open floor with a loose lattice of pillars
//   Rooms   - interior walls with several doorways, so rooms are never dead ends
//   Ruins   - open ground littered with collapsed wall chunks
//   Cavern  - cellular-automaton cave, with clear lanes to every socket
// The floor's region weights the choice (barracks favour rooms, crypts
// caverns, the sanctum halls). Every result obeys the same authoring rules
// as a hand-made module -- sockets, full connectivity, a clear centre for
// encounters -- and is at least about half floor, so floors stay open.
enum class ProceduralStyle { Hall, Rooms, Ruins, Cavern };

const char* proceduralStyleName(ProceduralStyle style);
ProceduralStyle pickProceduralStyle(FloorRegion region, std::mt19937& rng);
ModuleTemplate proceduralModule(ProceduralStyle style, std::mt19937& rng);

} // namespace engine
