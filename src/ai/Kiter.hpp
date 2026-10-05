#pragma once

#include "entities/AIBehavior.hpp"
#include "entities/MonsterAttackProfile.hpp"

namespace engine {

// Ranged kiter: maintains distance from its target instead of closing
// in. Approaches while out of range, retreats if the target gets too
// close, attacks from the band in between. Genuinely different movement
// logic from Chaser -- a Kiter is the only thing in this roster that
// ever moves *away* from its target.
class Kiter : public AIBehavior {
public:
    Kiter(MonsterAttackProfile attackProfile, int attackRange, int tooCloseRange,
          int sightRadius = kDefaultSightRadius);

    AIDecision decideAction(const Actor& self, const Map& map, Actor& player,
                             const std::vector<Actor*>& allies) override;

private:
    static constexpr int kDefaultSightRadius = 8;
    MonsterAttackProfile attackProfile_;
    int attackRange_;
    int tooCloseRange_;
    int sightRadius_;
    // Steps backed off in a row while you were close. After two, it stands
    // its ground and shoots until you leave its personal space.
    int retreats_ = 0;
};

} // namespace engine
