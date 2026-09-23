#pragma once

#include <optional>
#include <vector>

#include "core/Position.hpp"
#include "world/Map.hpp"

namespace engine {

// Finds the shortest walkable path from `start` to `goal` using A* with a
// Manhattan-distance heuristic and 4-directional movement (no diagonals
// -- matches how player movement already works, see Application::
// tryMovePlayer; a monster that could cut corners player movement can't
// would be an inconsistent rule the player can't see). Returns the path
// including both `start` and `goal`, or std::nullopt if no walkable path
// exists (goal unreachable, or start/goal themselves aren't walkable).
//
// Pure function, like computeFieldOfView -- no knowledge of actors, no
// map mutation, nothing remembered between calls.
std::optional<std::vector<Position>> findPath(const Map& map, Position start, Position goal);

} // namespace engine
