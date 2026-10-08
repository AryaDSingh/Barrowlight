// A* pathfinding over the tile grid, 4-directional like the player's own
// movement, so monsters never cut a corner the player can't.

#include "world/Pathfinder.hpp"

#include <algorithm>
#include <cmath>
#include <queue>
#include <unordered_map>

namespace engine {

namespace {

int toIndex(const Map& map, Position p) {
    return p.y * map.width() + p.x;
}

Position fromIndex(const Map& map, int index) {
    return Position{index % map.width(), index / map.width()};
}

int manhattanDistance(Position a, Position b) {
    return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

struct QueueEntry {
    int fScore;
    int index;
};

// Min-heap by fScore: std::priority_queue is a max-heap by default, so
// this comparator is inverted (greater-than) to get the smallest fScore
// out first.
struct QueueEntryCompare {
    bool operator()(const QueueEntry& a, const QueueEntry& b) const {
        return a.fScore > b.fScore;
    }
};

} // namespace

std::optional<std::vector<Position>> findPath(const Map& map, Position start, Position goal) {
    if (!map.isWalkable(start.x, start.y) || !map.isWalkable(goal.x, goal.y)) {
        return std::nullopt;
    }
    if (start.x == goal.x && start.y == goal.y) {
        return std::vector<Position>{start};
    }

    const int startIndex = toIndex(map, start);
    const int goalIndex = toIndex(map, goal);

    // std::priority_queue has no decrease-key operation, so cheaper
    // routes to an already-queued tile are handled by pushing a new
    // entry and skipping stale ones (via `closed`) when popped, rather
    // than mutating the heap in place -- the standard, simplest way to
    // do this with the standard library's priority queue.
    std::unordered_map<int, int> gScore;
    std::unordered_map<int, int> cameFrom;
    std::unordered_map<int, bool> closed;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueEntryCompare> open;

    gScore[startIndex] = 0;
    open.push(QueueEntry{manhattanDistance(start, goal), startIndex});

    static constexpr int kDx[4] = {0, 0, -1, 1};
    static constexpr int kDy[4] = {-1, 1, 0, 0};

    while (!open.empty()) {
        const int currentIndex = open.top().index;
        open.pop();

        if (closed[currentIndex]) {
            continue; // stale entry, superseded by a cheaper one already processed
        }
        closed[currentIndex] = true;

        if (currentIndex == goalIndex) {
            std::vector<Position> path;
            for (int idx = goalIndex; idx != startIndex; idx = cameFrom[idx]) {
                path.push_back(fromIndex(map, idx));
            }
            path.push_back(start);
            std::reverse(path.begin(), path.end());
            return path;
        }

        const Position current = fromIndex(map, currentIndex);

        for (int dir = 0; dir < 4; ++dir) {
            const Position neighbor{current.x + kDx[dir], current.y + kDy[dir]};
            if (!map.isWalkable(neighbor.x, neighbor.y)) {
                continue;
            }

            const int neighborIndex = toIndex(map, neighbor);
            const int tentativeG = gScore[currentIndex] + 1;

            const auto existing = gScore.find(neighborIndex);
            if (existing == gScore.end() || tentativeG < existing->second) {
                gScore[neighborIndex] = tentativeG;
                cameFrom[neighborIndex] = currentIndex;
                const int f = tentativeG + manhattanDistance(neighbor, goal);
                open.push(QueueEntry{f, neighborIndex});
            }
        }
    }

    return std::nullopt; // goal unreachable
}

} // namespace engine
