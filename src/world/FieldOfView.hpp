#pragma once

#include <vector>

#include "core/Position.hpp"
#include "world/Map.hpp"

namespace engine {

// Computes the set of tiles visible from `origin` within `radius`, using
// recursive shadowcasting -- a tile blocks sight if its Tile::transparent
// is false. `origin` itself is always included (if in bounds).
//
// Pure function: doesn't know about players, doesn't mutate the Map,
// doesn't remember anything between calls. What to do with the result
// (render it, remember it) is the caller's job -- see ExploredMap for the
// "remembered but not currently visible" tracking this feeds.
//
// May contain duplicate positions (adjacent octants can both reach a
// tile on their shared boundary) -- harmless for how this is consumed
// (ExploredMap::update just marks each one visible, idempotently), so
// not worth the extra code to deduplicate.
std::vector<Position> computeFieldOfView(const Map& map, Position origin, int radius);

} // namespace engine
