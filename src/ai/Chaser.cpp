#include "ai/Chaser.hpp"

#include <algorithm>

#include "entities/Actor.hpp"
#include "world/FieldOfView.hpp"
#include "world/Map.hpp"
#include "world/Pathfinder.hpp"

namespace engine {

Chaser::Chaser(int sightRadius) : sightRadius_(sightRadius) {}

std::optional<Position> Chaser::decideMove(const Actor& self, const Map& map,
                                            Position targetPosition) {
    const Position selfPos = self.position();

    // Only chase if the target is actually within this actor's own
    // sight -- reuses the same shadowcasting FOV used for the player's
    // view (Prompt 6), so a Chaser won't "see" a target through walls
    // just because it's within radius distance.
    const std::vector<Position> visible = computeFieldOfView(map, selfPos, sightRadius_);
    const bool targetVisible =
        std::find_if(visible.begin(), visible.end(), [&](const Position& p) {
            return p.x == targetPosition.x && p.y == targetPosition.y;
        }) != visible.end();

    if (!targetVisible) {
        return std::nullopt;
    }

    const std::optional<std::vector<Position>> path = findPath(map, selfPos, targetPosition);

    // path->size() < 3 covers both the degenerate self == target case
    // (size 1) and already-adjacent (size 2, i.e. one step directly onto
    // the target's own tile) -- moving onto the target's tile isn't a
    // valid move since there's no attack action yet to do instead, so
    // both cases mean "stay put." findPath already returns nullopt for a
    // genuinely unreachable target.
    if (!path || path->size() < 3) {
        return std::nullopt;
    }

    return (*path)[1]; // path[0] is selfPos itself; path[1] is the first real step
}

} // namespace engine
