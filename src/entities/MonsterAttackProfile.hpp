#pragma once

#include <optional>

#include "entities/StatusEffects.hpp"

namespace engine {

// A simple, uncooldowned attack: flat damage, optionally with a chance
// to apply a status effect on hit. Used by Chaser-based monsters (a
// plain melee hit every eligible turn), not TalentSet -- those monsters
// have exactly one move and no reason to track a cooldown for it. The
// more elaborate archetypes (Support, AoEBomber) reuse TalentSet/Talent
// instead, since they genuinely need a cooldown-gated special move.
struct MonsterAttackProfile {
    int power = 0;
    std::optional<StatusEffectInstance> onHitEffect; // e.g. Poison, Stun
    float onHitChance = 1.f; // 1.0 = always applies onHitEffect when present
};

} // namespace engine
