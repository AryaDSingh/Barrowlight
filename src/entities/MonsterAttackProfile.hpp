#pragma once

#include <optional>

#include "entities/AttributeFormulas.hpp"
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

    // Which attribute this attack's damage scales with. Defaults to
    // Strength -- every current MonsterAttackProfile-backed attack
    // (Goblin/Spider/Ogre/Archer's basic hits, the boss's melee) is a
    // physical strike; nothing currently needs Dexterity or
    // Intelligence here (the roster's two magic-coded attacks, Bomber's
    // Blast and the boss's Warlord's Fury, are both TalentSet-driven
    // abilities, not MonsterAttackProfile-backed). Treated as Filler
    // tier when computing its damage bonus (see
    // AttributeFormulas::abilityDamageBonus) -- these attacks have no
    // cooldown concept at all, and "every eligible turn" is the closest
    // existing tier to that.
    ScalingStat scalingStat = ScalingStat::Strength;
};

} // namespace engine
