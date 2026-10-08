// AoEBomber: keeps its distance and throws a cooldown-gated area blast (see AoEBomber.hpp).

#include "ai/AoEBomber.hpp"

#include <algorithm>

#include "ai/AIUtils.hpp"
#include "entities/Actor.hpp"
#include "world/FieldOfView.hpp"
#include "world/Map.hpp"
#include "world/LineOfFire.hpp"
#include "world/Pathfinder.hpp"

namespace engine {

AoEBomber::AoEBomber(int blastPower, int blastRange, int tooCloseRange, int sightRadius)
    : blastPower_(blastPower),
      blastRange_(blastRange),
      tooCloseRange_(tooCloseRange),
      sightRadius_(sightRadius) {}

AIDecision AoEBomber::decideAction(const Actor& self, const Map& map, Actor& player,
                                    const std::vector<Actor*>& /*allies*/) {
    const Position selfPos = self.position();
    const Position playerPos = player.position();

    const std::vector<Position> visible = computeFieldOfView(map, selfPos, sightRadius_);
    const bool targetVisible =
        std::find_if(visible.begin(), visible.end(), [&](const Position& p) {
            return p.x == playerPos.x && p.y == playerPos.y;
        }) != visible.end();

    if (!targetVisible) {
        return AIDecision{};
    }

    const int distSq = distanceSquared(selfPos, playerPos);

    if (distSq <= tooCloseRange_ * tooCloseRange_) {
        const Position retreat = retreatStep(selfPos, playerPos);
        if (map.isWalkable(retreat.x, retreat.y)) {
            AIDecision decision;
            decision.type = AIActionType::Move;
            decision.movePosition = retreat;
            return decision;
        }
        return AIDecision{}; // cornered -- holds rather than fighting; it has no basic attack
    }

    if (distSq <= blastRange_ * blastRange_ && hasLineOfFire(map,selfPos,playerPos)) {
        if (!self.talents().knownTalents().empty() && self.talents().isReady(0)) {
            AIDecision decision;
            decision.type = AIActionType::UseAbility;
            decision.target = &player;
            decision.abilityIndex = 0;
            decision.attackPower = blastPower_;
            decision.scalingStat = ScalingStat::Intelligence; // an explosive blast -- magic-coded, not a physical hit
            return decision;
        }
        return AIDecision{}; // in range but still recharging
    }

    // Too far -- approach, same pathfinding approach as Chaser/Kiter.
    const std::optional<std::vector<Position>> path = findPath(map, selfPos, playerPos);
    if (!path || path->size() < 2) {
        return AIDecision{};
    }

    AIDecision decision;
    decision.type = AIActionType::Move;
    decision.movePosition = (*path)[1];
    return decision;
}

} // namespace engine
