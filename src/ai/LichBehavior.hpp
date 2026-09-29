#pragma once

#include "entities/AIBehavior.hpp"
#include "entities/MonsterAttackProfile.hpp"

namespace engine {

// The floor kFinalFloor boss: a ranged caster that behaves like Kiter
// (maintains distance, bolts from range, retreats if approached) with
// one addition -- periodically, instead of a bolt, it raises a Skeleton
// minion near itself. This is the first AIBehavior in the roster that
// creates a new Monster rather than only acting on itself or the
// player; see AIActionType::Summon and Application::executeAIDecision
// for the actual creation, which stays Application's responsibility
// the same way every other decision type does (AIBehavior only decides
// what to do, never mutates game state itself).
//
// Deliberately no phase structure like BossBehavior -- the summon
// mechanic itself is what makes this fight escalate (more attackers
// over time, not a bigger number at a fixed hp threshold), so a second
// escalation axis would be redundant rather than additive.
class LichBehavior : public AIBehavior {
public:
    LichBehavior(MonsterAttackProfile boltProfile, int attackRange, int tooCloseRange,
                 int maxSummons, int sightRadius = kDefaultSightRadius);

    AIDecision decideAction(const Actor& self, const Map& map, Actor& player,
                             const std::vector<Actor*>& allies) override;

private:
    static constexpr int kDefaultSightRadius = 8;

    MonsterAttackProfile boltProfile_;
    int attackRange_;
    int tooCloseRange_;
    // Capped, not unlimited -- an endless stream of skeletons would
    // make the fight a war of attrition rather than a real encounter.
    // Counts committed ritual attempts, including interruptions/fizzles.
    // Killing a skeleton never replenishes this lifetime budget.
    int maxSummons_;
    int sightRadius_;

    // Committed attempts live on Monster and persist in saves.
};

} // namespace engine
