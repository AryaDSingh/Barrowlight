#pragma once

#include "entities/Talent.hpp"

namespace engine {

class Actor;

// Applies `talent`'s damage to `target`'s Stats::hp directly, including
// the conditional/execute bonus (Execution) if the target qualifies.
// Deliberately the one place damage gets applied -- kept out of Talent
// (pure data) and TalentSet (cooldown bookkeeping) so each stays focused
// on one job.
//
// Doesn't touch cooldowns, mana/hp costs, or targeting -- those are the
// caller's responsibility (Application resolves who's affected, since
// it's the one thing that currently knows about every Actor in the
// level; for an AreaAroundTarget/AreaAroundSelf talent, this gets called
// once per affected Actor).
void applyTalentDamage(const Talent& talent, Actor& target);

} // namespace engine
