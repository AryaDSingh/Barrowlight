#include "ai/BossBehavior.hpp"

#include <algorithm>

#include "ai/AIUtils.hpp"
#include "entities/Actor.hpp"
#include "world/FieldOfView.hpp"
#include "world/Map.hpp"
#include "world/LineOfFire.hpp"
#include "world/Pathfinder.hpp"

namespace engine {

namespace {
// Effectively permanent for the rest of any realistic fight -- "enraged"
// is meant to be a one-way state, not something that should wear off
// mid-fight and need reapplying.
constexpr int kEnrageDuration = 999;
} // namespace

BossBehavior::BossBehavior(MonsterAttackProfile meleeProfile, int blastPower, int blastRange,
                            int tooCloseRange, int enrageBonus, int sightRadius)
    : meleeProfile_(meleeProfile),
      blastPower_(blastPower),
      blastRange_(blastRange),
      tooCloseRange_(tooCloseRange),
      enrageBonus_(enrageBonus),
      sightRadius_(sightRadius) {}

int BossBehavior::currentPhase(const Actor& self) const {
    if (self.stats().maxHp <= 0) {
        return 1;
    }
    const float frac =
        static_cast<float>(self.stats().hp) / static_cast<float>(self.stats().maxHp);
    if (frac <= kPhase3Threshold) {
        return 3;
    }
    if (frac <= kPhase2Threshold) {
        return 2;
    }
    return 1;
}

AIDecision BossBehavior::decideAction(const Actor& self, const Map& map, Actor& player,
                                       const std::vector<Actor*>& /*allies*/) {
    const int phase = currentPhase(self);

    std::string announcement;
    if (phase != announcedPhase_) {
        announcedPhase_ = phase;
        if (phase == 2) {
            announcement = self.name() + " staggers, then lashes out with wild magic!";
        } else if (phase == 3) {
            announcement = self.name() + " roars with fury, entering a desperate rage!";
        }
    }

    AIDecision decision;
    switch (phase) {
        case 2:
            decision = decidePhase2(self, map, player);
            break;
        case 3:
            decision = decidePhase3(self, map, player);
            break;
        default:
            decision = decidePhase1(self, map, player);
            break;
    }
    decision.announcement = announcement;
    return decision;
}

AIDecision BossBehavior::decidePhase1(const Actor& self, const Map& map, Actor& player) {
    return meleeApproach(self, map, player);
}

AIDecision BossBehavior::decidePhase2(const Actor& self, const Map& map, Actor& player) {
    const Position selfPos=self.position(), target=player.position();
    if (distanceSquared(selfPos,target)<=blastRange_*blastRange_ &&
        hasLineOfFire(map,selfPos,target) && !self.talents().knownTalents().empty() && self.talents().isReady(0)) {
        AIDecision decision;
        decision.type=AIActionType::UseAbility; decision.target=&player;
        decision.abilityIndex=0; decision.attackPower=blastPower_;
        decision.scalingStat=ScalingStat::Intelligence;
        return decision;
    }
    // Keep contesting space while Fury recharges instead of idling at range
    // or repeatedly trying to retreat into the Warlord's own guards.
    return meleeApproach(self,map,player);
}

AIDecision BossBehavior::decidePhase3(const Actor& self, const Map& map, Actor& player) {
    if (!enraged_) {
        enraged_ = true;
        AIDecision decision;
        decision.type = AIActionType::SelfBuff;
        decision.effectToApply =
            StatusEffectInstance{StatusEffectType::Empowered, kEnrageDuration, enrageBonus_};
        return decision;
    }
    return meleeApproach(self, map, player);
}

AIDecision BossBehavior::meleeApproach(const Actor& self, const Map& map, Actor& player) {
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
        decision.attackPower = meleeProfile_.power;
        decision.scalingStat = meleeProfile_.scalingStat;
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
