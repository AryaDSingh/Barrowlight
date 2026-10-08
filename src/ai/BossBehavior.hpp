#pragma once

#include "entities/AIBehavior.hpp"
#include "entities/MonsterAttackProfile.hpp"

namespace engine {

// Phase-based boss fight: aggressive melee while healthy, a
// cooldown-gated AoE blast (reusing TalentSet, the same way AoEBomber
// does) once wounded, and a one-time self-Empower (the ordinary Empowered
// status, no special mechanic) before
// going all-in once nearly dead.
//
// Phase is always re-derived from current hp fraction, not tracked as
// separate state -- the only deliberate exceptions are `enraged_`
// (whether the one-time enrage trigger has already fired) and
// `announcedPhase_` (so phase-transition flavor text prints once, not
// every turn spent in a phase). Both are narrow, single-purpose state,
// not a departure from the stateless-by-default pattern the rest of the
// roster follows.
class BossBehavior : public AIBehavior {
public:
    BossBehavior(MonsterAttackProfile meleeProfile, int blastPower, int blastRange,
                 int tooCloseRange, int enrageBonus, int sightRadius = kDefaultSightRadius);

    AIDecision decideAction(const Actor& self, const Map& map, Actor& player,
                             const std::vector<Actor*>& allies) override;

    int announcedPhase() const { return announcedPhase_; }
    bool enraged() const { return enraged_; }
    void restoreState(int phase, bool enraged) { announcedPhase_=phase; enraged_=enraged; }

private:
    static constexpr int kDefaultSightRadius = 10;
    static constexpr float kPhase2Threshold = 0.6f;
    static constexpr float kPhase3Threshold = 0.3f;

    int currentPhase(const Actor& self) const;
    AIDecision decidePhase1(const Actor& self, const Map& map, Actor& player);
    AIDecision decidePhase2(const Actor& self, const Map& map, Actor& player);
    AIDecision decidePhase3(const Actor& self, const Map& map, Actor& player);
    // Shared by phase 1 and (after the enrage trigger) phase 3 -- same
    // approach-then-melee shape in both; phase 3 simply hits harder
    // because Empowered adds to whatever attackPower is dealt
    // (Application's executeAIDecision), not because this logic changes.
    AIDecision meleeApproach(const Actor& self, const Map& map, Actor& player);

    MonsterAttackProfile meleeProfile_;
    int blastPower_;
    int blastRange_;
    int tooCloseRange_;
    int enrageBonus_;
    int sightRadius_;

    int announcedPhase_ = 1;
    bool enraged_ = false;
};

} // namespace engine
