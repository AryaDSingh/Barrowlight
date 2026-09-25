#include "ai/Kiter.hpp"

#include <algorithm>
#include <random>

#include "ai/AIUtils.hpp"
#include "entities/Actor.hpp"
#include "world/FieldOfView.hpp"
#include "world/Map.hpp"
#include "world/Pathfinder.hpp"

namespace engine {

Kiter::Kiter(MonsterAttackProfile attackProfile, int attackRange, int tooCloseRange,
             int sightRadius)
    : attackProfile_(attackProfile),
      attackRange_(attackRange),
      tooCloseRange_(tooCloseRange),
      sightRadius_(sightRadius) {}

AIDecision Kiter::decideAction(const Actor& self, const Map& map, Actor& player,
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

    auto makeAttack = [&]() {
        AIDecision decision;
        decision.type = AIActionType::Attack;
        decision.target = &player;
        decision.attackPower = attackProfile_.power;
        decision.scalingStat = attackProfile_.scalingStat;
        if (attackProfile_.onHitEffect.has_value()) {
            static std::mt19937 rng{std::random_device{}()};
            std::uniform_real_distribution<float> roll(0.f, 1.f);
            if (roll(rng) <= attackProfile_.onHitChance) {
                decision.effectToApply = attackProfile_.onHitEffect;
            }
        }
        return decision;
    };

    const int distSq = distanceSquared(selfPos, playerPos);

    if (distSq <= tooCloseRange_ * tooCloseRange_) {
        // Too close -- retreat directly away from the player.
        const Position retreat = retreatStep(selfPos, playerPos);
        if (map.isWalkable(retreat.x, retreat.y)) {
            AIDecision decision;
            decision.type = AIActionType::Move;
            decision.movePosition = retreat;
            return decision;
        }
        // Cornered -- fight rather than do nothing.
        return makeAttack();
    }

    if (distSq <= attackRange_ * attackRange_) {
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
