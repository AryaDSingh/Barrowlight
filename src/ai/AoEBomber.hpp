#pragma once

#include "entities/AIBehavior.hpp"

namespace engine {

// AoE threat: approaches to blast range, avoids getting adjacent, and
// unleashes an area attack on a cooldown once in range. Distinct from
// Kiter: its ranged option isn't spammable (cooldown-gated via its own
// TalentSet, reusing the Prompt 9 infrastructure like Support does), and
// when cornered it holds rather than fighting back with a basic attack
// -- unlike Kiter/Chaser, it doesn't have one.
//
// Targets the player only, not other monsters -- no friendly-fire/
// faction system exists (or is being built for this prompt); a real
// implementation might have the blast hit allies too and rely on the
// Bomber's own placement to avoid friendly fire, but that's more than
// this roster needs to demonstrate the archetype.
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
