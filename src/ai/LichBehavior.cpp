#include "ai/LichBehavior.hpp"

#include <algorithm>

#include "ai/AIUtils.hpp"
#include "entities/Actor.hpp"
#include "world/FieldOfView.hpp"
#include "world/Map.hpp"
#include "world/Pathfinder.hpp"

namespace engine {

namespace {
// Looks for an open tile adjacent to `around` -- walkable, and not
// already occupied by the player or another ally. Returns the first one
// found (the 8 neighbors are checked in a fixed order, not randomized --
// there's no gameplay reason a particular side should be preferred, and
// determinism here makes this trivially easy to reason about and test).
// std::nullopt if every neighbor is blocked.
std::optional<Position> findSummonSpot(const Map& map, Position around, Position playerPos,
                                        const std::vector<Actor*>& allies) {
    static constexpr int kOffsets[8][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0},
                                            {1, 0},   {-1, 1}, {0, 1},  {1, 1}};
    for (const auto& offset : kOffsets) {
        const Position candidate{around.x + offset[0], around.y + offset[1]};
        if (!map.isWalkable(candidate.x, candidate.y)) {
            continue;
        }
        if (candidate.x == playerPos.x && candidate.y == playerPos.y) {
            continue;
        }
        const bool blockedByAlly =
            std::any_of(allies.begin(), allies.end(), [&](const Actor* ally) {
                return ally->position().x == candidate.x && ally->position().y == candidate.y;
            });
        if (!blockedByAlly) {
            return candidate;
        }
    }
    return std::nullopt;
}
} // namespace

LichBehavior::LichBehavior(MonsterAttackProfile boltProfile, int attackRange, int tooCloseRange,
                            int maxSummons, int sightRadius)
    : boltProfile_(boltProfile),
      attackRange_(attackRange),
      tooCloseRange_(tooCloseRange),
      maxSummons_(maxSummons),
      sightRadius_(sightRadius) {}

AIDecision LichBehavior::decideAction(const Actor& self, const Map& map, Actor& player,
                                       const std::vector<Actor*>& allies) {
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

    auto makeBolt = [&]() {
        AIDecision decision;
        decision.type = AIActionType::Attack;
        decision.target = &player;
        decision.attackPower = boltProfile_.power;
        decision.scalingStat = boltProfile_.scalingStat;
        return decision;
    };

    const int distSq = distanceSquared(selfPos, playerPos);

    if (distSq <= tooCloseRange_ * tooCloseRange_) {
        const Position retreat = retreatStep(selfPos, playerPos);
        if (map.isWalkable(retreat.x, retreat.y)) {
            AIDecision decision;
            decision.type = AIActionType::Move;
            decision.movePosition = retreat;
            return decision;
        }
        return makeBolt(); // cornered -- fight rather than do nothing
    }

    if (distSq <= attackRange_ * attackRange_) {
        // In range: prefer summoning over bolting whenever it's ready
        // and there's an open spot to summon into, up to the cap --
        // the Lich's whole identity is "more attackers over time," so
        // this should win over a plain bolt whenever it's actually
        // available, not be a rare surprise.
        if (summonsUsed_ < maxSummons_ && !self.talents().knownTalents().empty() &&
            self.talents().isReady(0)) {
            const std::optional<Position> spot = findSummonSpot(map, selfPos, playerPos, allies);
            if (spot.has_value()) {
                summonsUsed_ += 1;
                AIDecision decision;
                decision.type = AIActionType::Summon;
                decision.movePosition = *spot;
                decision.summonType = MonsterType::Skeleton;
                decision.abilityIndex = 0; // starts this same cooldown, see executeAIDecision
                decision.announcement = self.name() + " raises a skeleton from the ground!";
                return decision;
            }
            // Ready and under the cap, but genuinely no open tile to
            // summon into right now -- fall through to a normal bolt
            // rather than wasting the turn; the cooldown is
            // deliberately left untouched (see executeAIDecision) so
            // this same attempt can succeed next turn if a tile opens
            // up.
        }
        return makeBolt();
    }

    // Too far -- approach, same pathfinding approach as Kiter/Chaser.
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
