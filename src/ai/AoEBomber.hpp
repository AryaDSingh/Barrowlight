#pragma once

#include "entities/AIBehavior.hpp"

namespace engine {

// AoE threat: approaches to blast range, avoids getting adjacent, and
// unleashes an area attack on a cooldown once in range. Distinct from
// Kiter: its ranged option isn't spammable (cooldown-gated via its own
// TalentSet, the same cooldown bookkeeping the player uses), and
// when cornered it holds rather than fighting back with a basic attack
// -- unlike Kiter/Chaser, it doesn't have one.
//
// Aims at the player. (The committed blast itself, resolved by
// Application, hurts whatever stands in it, other monsters included.)
class AoEBomber : public AIBehavior {
public:
    AoEBomber(int blastPower, int blastRange, int tooCloseRange,
              int sightRadius = kDefaultSightRadius);

    AIDecision decideAction(const Actor& self, const Map& map, Actor& player,
                             const std::vector<Actor*>& allies) override;

private:
    static constexpr int kDefaultSightRadius = 8;
    int blastPower_;
    int blastRange_;
    int tooCloseRange_;
    int sightRadius_;
};

} // namespace engine
