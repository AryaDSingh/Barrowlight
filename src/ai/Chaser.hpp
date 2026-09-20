#pragma once

#include "entities/AIBehavior.hpp"
#include "entities/MonsterAttackProfile.hpp"

namespace engine {

// Melee rusher: paths toward its target via A* while it's out of sight
// or out of reach, and attacks once adjacent, using its own
// MonsterAttackProfile. Prompt 7's Chaser only ever moved; Prompt 10
// adds the attack -- this is exactly the "adjacent currently just means
// stay put" limitation flagged back then.
//
// Powers Goblin (plain profile), Spider (Poison on-hit), and Ogre (Stun
// on-hit, lower chance) via different construction arguments -- one
// class, three enemy types, per the project's founding "data + which
// behavior gets plugged in, not a new subclass" philosophy.
class Chaser : public AIBehavior {
public:
    explicit Chaser(MonsterAttackProfile attackProfile, int sightRadius = kDefaultSightRadius);

    AIDecision decideAction(const Actor& self, const Map& map, Actor& player,
                             const std::vector<Actor*>& allies) override;

private:
    static constexpr int kDefaultSightRadius = 8;
    MonsterAttackProfile attackProfile_;
    int sightRadius_;
};

} // namespace engine
