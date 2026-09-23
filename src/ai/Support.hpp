#pragma once

#include "entities/AIBehavior.hpp"

namespace engine {

// Support/buffer: doesn't attack the player at all. Finds a nearby ally
// that isn't already Empowered and buffs it (a flat damage bonus for
// that ally's own attacks) on a cooldown. Deliberately stationary -- no
// movement logic whatsoever, unlike everything else in this roster --
// which is the point: a support unit that chased the player would just
// be a worse Chaser, not a distinct archetype.
//
// Reuses TalentSet purely for its cooldown tracking (one Talent entry,
// index 0) -- the buff's own magnitude/duration are this class's
// constructor parameters, not stored in the Talent itself, since
// Talent's other fields are damage-application-oriented and this isn't
// a damage ability.
class Support : public AIBehavior {
public:
    Support(int buffMagnitude, int buffDuration, int buffRadius);

    AIDecision decideAction(const Actor& self, const Map& map, Actor& player,
                             const std::vector<Actor*>& allies) override;

private:
    int buffMagnitude_;
    int buffDuration_;
    int buffRadius_;
};

} // namespace engine
