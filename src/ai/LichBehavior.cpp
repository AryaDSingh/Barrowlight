#include "ai/LichBehavior.hpp"

#include <algorithm>

#include "ai/AIUtils.hpp"
#include "entities/Actor.hpp"
#include "entities/Monster.hpp"
#include "world/FieldOfView.hpp"
#include "world/Map.hpp"
#include "world/LineOfFire.hpp"
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
    // Hex uses the normal talent cooldown so saving cannot refresh it. The
    // chosen opponent must still be visible and in clear casting range.
    if (distSq<=attackRange_*attackRange_ && hasLineOfFire(map,selfPos,playerPos) &&
        self.talents().knownTalents().size()>1 && self.talents().isReady(1) &&
        !player.statusEffects().has(StatusEffectType::Doom) && !player.statusEffects().has(StatusEffectType::ManaDrain)) {
        const bool doom=self.stats().hp*10<=self.stats().maxHp*6 || player.stats().maxMana==0;
        AIDecision hex;
        hex.type=AIActionType::UseAbility; hex.target=&player; hex.abilityIndex=1;
        // Doom ticks once before the player regains control, leaving four
        // complete player actions to cleanse or prepare healing.
        hex.effectToApply=StatusEffectInstance{doom?StatusEffectType::Doom:StatusEffectType::ManaDrain,
            doom?5:4,doom?10+self.stats().intelligence/4:2};
        hex.announcement=self.name()+(doom?" invokes Doom! C: Cleanse before the countdown expires.":
            " invokes Mana Drain! C: Cleanse removes the curse.");
        return hex;
    }

    if (distSq <= tooCloseRange_ * tooCloseRange_) {
        const Position retreat = retreatStep(selfPos, playerPos);
        if (map.isWalkable(retreat.x, retreat.y) &&
            std::none_of(allies.begin(),allies.end(),[&](const auto* ally) {
                return ally->stats().hp>0 && ally->position().x==retreat.x && ally->position().y==retreat.y;
            })) {
            AIDecision decision;
            decision.type = AIActionType::Move;
            decision.movePosition = retreat;
            return decision;
        }
        if (hasLineOfFire(map,selfPos,playerPos)) return makeBolt(); // only shoot through clear terrain
    }

    if (distSq <= attackRange_ * attackRange_ && hasLineOfFire(map,selfPos,playerPos)) {
        // In range: prefer summoning over bolting whenever it's ready
        // and there's an open spot to summon into, up to the cap --
        // the Lich's whole identity is "more attackers over time," so
        // this should win over a plain bolt whenever it's actually
        // available, not be a rare surprise.
        const auto* monster=dynamic_cast<const Monster*>(&self);
        if (monster && monster->summonsCommitted < maxSummons_ && !self.talents().knownTalents().empty() &&
            self.talents().isReady(0)) {
            const std::optional<Position> spot = findSummonSpot(map, selfPos, playerPos, allies);
            if (spot.has_value()) {
                AIDecision decision;
                decision.type = AIActionType::Summon;
                decision.movePosition = *spot;
                decision.summonType = monster->summonsCommitted==1?MonsterType::SkeletonArcher:MonsterType::SkeletonGuard;
                decision.abilityIndex = 0; // starts this same cooldown, see executeAIDecision
                decision.announcement = self.name() + " begins a summoning ritual!";
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
