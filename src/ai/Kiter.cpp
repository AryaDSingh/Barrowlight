#include "ai/Kiter.hpp"

#include <algorithm>
#include <random>

#include "ai/AIUtils.hpp"
#include "entities/Actor.hpp"
#include "entities/AttributeFormulas.hpp"
#include "world/FieldOfView.hpp"
#include "world/Map.hpp"
#include "world/LineOfFire.hpp"
#include "world/Pathfinder.hpp"

namespace engine {

Kiter::Kiter(MonsterAttackProfile attackProfile, int attackRange, int tooCloseRange,
             int sightRadius)
    : attackProfile_(attackProfile),
      attackRange_(attackRange),
      tooCloseRange_(tooCloseRange),
      sightRadius_(sightRadius) {}

AIDecision Kiter::decideAction(const Actor& self, const Map& map, Actor& player,
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

    auto makeAttack = [&]() {
        AIDecision decision;
        decision.type = AIActionType::Attack;
        decision.target = &player;
        decision.attackPower = attackProfile_.power;
        decision.scalingStat = attackProfile_.scalingStat;
        if (attackProfile_.onHitEffect.has_value()) {
            std::uniform_real_distribution<float> roll(0.f, 1.f);
            if (roll(combatRng()) <= attackProfile_.onHitChance) {
                decision.effectToApply = attackProfile_.onHitEffect;
            }
        }
        return decision;
    };

    const int distSq = distanceSquared(selfPos, playerPos);

    if (distSq <= tooCloseRange_ * tooCloseRange_) {
        // Too close -- retreat directly away from the player.
        Position retreat=selfPos;
        int best=distSq;
        for(const Position p:{Position{selfPos.x+1,selfPos.y},Position{selfPos.x-1,selfPos.y},Position{selfPos.x,selfPos.y+1},Position{selfPos.x,selfPos.y-1}}) {
            if(!map.isWalkable(p.x,p.y) || std::any_of(allies.begin(),allies.end(),[&](const Actor* a){return a && a->stats().hp>0 && a->position().x==p.x && a->position().y==p.y;})) continue;
            const int distance=distanceSquared(p,playerPos);
            if(distance>best) { best=distance; retreat=p; }
        }
        if(best>distSq) {
            AIDecision decision; decision.type=AIActionType::Move;
            decision.movePosition=retreat; return decision;
        }
        // Cornered -- fight rather than do nothing.
        if (hasLineOfFire(map,selfPos,playerPos)) return makeAttack();
    }

    if (distSq <= attackRange_ * attackRange_ && hasLineOfFire(map,selfPos,playerPos)) {
        return makeAttack();
    }

    // Too far -- approach, same pathfinding approach as Chaser.
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
