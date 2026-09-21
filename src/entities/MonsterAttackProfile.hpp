#pragma once

#include <optional>

#include "entities/DamageType.hpp"
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

    // Which attribute this attack's damage scales with (Prompt 14).
    // Defaults to Physical -- every current MonsterAttackProfile-backed
    // attack (Goblin/Spider/Ogre/Archer's basic hits, the boss's melee)
    // is a physical strike; nothing currently needs Magic here (the
    // roster's two magic-coded attacks, Bomber's Blast and the boss's
    // Warlord's Fury, are both TalentSet-driven abilities, not
    // MonsterAttackProfile-backed).
    DamageType damageType = DamageType::Physical;
};

} // namespace engine
