#include "ai/Chaser.hpp"

#include <algorithm>
#include <random>

#include "ai/AIUtils.hpp"
#include "entities/Actor.hpp"
#include "world/FieldOfView.hpp"
#include "world/Map.hpp"
#include "world/Pathfinder.hpp"

namespace engine {

Chaser::Chaser(MonsterAttackProfile attackProfile, int sightRadius)
    : attackProfile_(attackProfile), sightRadius_(sightRadius) {}

AIDecision Chaser::decideAction(const Actor& self, const Map& map, Actor& player,
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

    if (isAdjacent(selfPos, playerPos)) {
        AIDecision decision;
        decision.type = AIActionType::Attack;
        decision.target = &player;
        decision.attackPower = attackProfile_.power;

        if (attackProfile_.onHitEffect.has_value()) {
            static std::mt19937 rng{std::random_device{}()};
            std::uniform_real_distribution<float> roll(0.f, 1.f);
            if (roll(rng) <= attackProfile_.onHitChance) {
                decision.effectToApply = attackProfile_.onHitEffect;
            }
        }
        return decision;
    }

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
