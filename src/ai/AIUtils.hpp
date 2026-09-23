#pragma once

#include <cstdlib>

#include "core/Position.hpp"

namespace engine {

// Small geometry helpers shared by every AIBehavior that reasons about
// distance to a target. Chaser, Kiter, and AoEBomber each had their own
// near-identical copies of these; extracted here once BossBehavior
// needed a third independent copy -- the usual "rule of three" signal
// that duplication is worth removing.

inline bool isAdjacent(Position a, Position b) {
    const int dx = std::abs(a.x - b.x);
    const int dy = std::abs(a.y - b.y);
    return (dx + dy) == 1;
}

inline int distanceSquared(Position a, Position b) {
    const int dx = a.x - b.x;
    const int dy = a.y - b.y;
    return dx * dx + dy * dy;
}

inline int sign(int v) {
    return (v > 0) - (v < 0);
}

// One step directly away from `awayFrom`, starting at `self`.
inline Position retreatStep(Position self, Position awayFrom) {
    return Position{self.x + sign(self.x - awayFrom.x), self.y + sign(self.y - awayFrom.y)};
}

} // namespace engine
