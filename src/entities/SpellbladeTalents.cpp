#include "entities/SpellbladeTalents.hpp"

namespace engine {

std::vector<Talent> spellbladeTalents() {
    std::vector<Talent> talents;

    // --- Blade tree ---
    // Physical -- Talent::damageType defaults to Physical, so none of
    // these four explicitly set it. Every `power` below is the ORIGINAL
    // tuned value minus physicalDamageBonus(14) == 2 (the Spellblade's
    // strength, see Application's constructor) -- e.g. Quick Strike was
    // tuned to 6 damage in Prompt 9; base 4 + bonus 2 == 6 again, same
    // output, now genuinely attribute-driven instead of a flat number.
    // See ARCHITECTURE_DECISIONS.md, "Attribute-driven combat."

    talents.push_back(Talent{
        /*name=*/"Quick Strike",
        /*description=*/"A fast, efficient melee strike. Low cost, low cooldown.",
        /*tree=*/TalentTree::Blade,
        /*targeting=*/TargetingMode::AdjacentEnemy,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/2,
        /*hpCost=*/0,
        /*cooldownTurns=*/1,
        /*power=*/4, // +2 from strength == 6, matching the original tuned value
    });

    talents.push_back(Talent{
        /*name=*/"Power Strike",
        /*description=*/"A heavy melee blow. Much more damage than Quick Strike, "
                         "but a real cooldown -- can't be spammed.",
        /*tree=*/TalentTree::Blade,
        /*targeting=*/TargetingMode::AdjacentEnemy,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/6,
        /*hpCost=*/0,
        /*cooldownTurns=*/4,
        /*power=*/14, // +2 from strength == 16, matching the original tuned value
    });

    talents.push_back(Talent{
        /*name=*/"Reckless Lunge",
        /*description=*/"Spend your own blood for a devastating strike. Costs hp "
                         "as well as mana -- high risk, high reward.",
        /*tree=*/TalentTree::Blade,
        /*targeting=*/TargetingMode::AdjacentEnemy,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/2,
        /*hpCost=*/6,
        /*cooldownTurns=*/3,
        /*power=*/20, // +2 from strength == 22, matching the original tuned value
    });

    talents.push_back(Talent{
        /*name=*/"Execution",
        /*description=*/"Modest damage normally, but triples against a target "
                         "already below 30% hp. Rewards good timing, not raw power.",
        /*tree=*/TalentTree::Blade,
        /*targeting=*/TargetingMode::AdjacentEnemy,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/4,
        /*hpCost=*/0,
        /*cooldownTurns=*/3,
        /*power=*/8, // +2 from strength == 10, matching the original tuned value
        /*areaRadius=*/0,
        /*moveDistance=*/0,
        /*conditionalHpFraction=*/0.3f,
        /*conditionalMultiplier=*/3, // applies to (power + strength bonus): (8+2)*3 == 30, same as (10)*3 before
    });

    // --- Flame tree ---
    // Magic -- each of these three explicitly sets damageType (built
    // then assigned, rather than positional brace-init all the way
    // through every trailing field just to reach the last one). `power`
    // values are likewise the original minus magicDamageBonus(18) == 4
    // (the Spellblade's intelligence).

    Talent emberBolt{
        /*name=*/"Ember Bolt",
        /*description=*/"A ranged bolt of flame. No need to be adjacent -- just "
                         "within sight.",
        /*tree=*/TalentTree::Flame,
        /*targeting=*/TargetingMode::RangedEnemyInSight,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/3,
        /*hpCost=*/0,
        /*cooldownTurns=*/1,
        /*power=*/3, // +4 from intelligence == 7, matching the original tuned value
    };
    emberBolt.damageType = DamageType::Magic;
    talents.push_back(emberBolt);

    Talent fireball{
        /*name=*/"Fireball",
        /*description=*/"Explodes around the target, damaging everything nearby. "
                         "Costly and slow to recharge -- inefficient against a "
                         "single enemy, but hits everyone in the blast.",
        /*tree=*/TalentTree::Flame,
        /*targeting=*/TargetingMode::RangedEnemyInSight,
        /*shape=*/EffectShape::AreaAroundTarget,
        /*manaCost=*/8,
        /*hpCost=*/0,
        /*cooldownTurns=*/5,
        /*power=*/8, // +4 from intelligence == 12, matching the original tuned value
        /*areaRadius=*/2,
    };
    fireball.damageType = DamageType::Magic;
    talents.push_back(fireball);

    // Blink deals no damage (power stays 0, Movement shape) -- damageType
    // is left at its Physical default since nothing ever reads it here.
    talents.push_back(Talent{
        /*name=*/"Blink",
        /*description=*/"Teleport a short distance in the direction you're "
                         "facing. No damage -- pure repositioning.",
        /*tree=*/TalentTree::Flame,
        /*targeting=*/TargetingMode::Self,
        /*shape=*/EffectShape::Movement,
        /*manaCost=*/3,
        /*hpCost=*/0,
        /*cooldownTurns=*/4,
        /*power=*/0,
        /*areaRadius=*/0,
        /*moveDistance=*/4,
    });

    Talent immolate{
        /*name=*/"Immolate",
        /*description=*/"Burns everything adjacent to you. A different shape of "
                         "AoE than Fireball -- centered on yourself, not a chosen "
                         "target.",
        /*tree=*/TalentTree::Flame,
        /*targeting=*/TargetingMode::Self,
        /*shape=*/EffectShape::AreaAroundSelf,
        /*manaCost=*/7,
        /*hpCost=*/0,
        /*cooldownTurns=*/5,
        /*power=*/5, // +4 from intelligence == 9, matching the original tuned value
        /*areaRadius=*/1,
    };
    immolate.damageType = DamageType::Magic;
    talents.push_back(immolate);

    // Renewal (player-requested addition, post-Prompt 14): the
    // Spellblade's first and only healing spell -- there was previously
    // no way to recover hp at all once damaged, mana regen (Prompt 13
    // follow-up) had no hp equivalent. A 9th talent, not a replacement
    // for any of the original 8 -- key 9. Self-targeted, SingleTarget
    // shape (see tryUseTalent's targeting resolution, which treats
    // Self+SingleTarget as "affects the caster directly," distinct from
    // AreaAroundSelf's "affects nearby enemies"). `power` here means
    // heal amount, not damage -- applyTalentHeal (TalentEffects.cpp)
    // adds it to hp instead of subtracting, capped at maxHp. Always
    // Intelligence-scaled regardless of damageType (healing is life
    // magic, no Strength-scaled equivalent exists), so base 8 + 4 from
    // the Spellblade's intelligence == 12 hp, a meaningful 40% of the
    // 30 maxHp pool without being a full heal. A 6-turn cooldown (the
    // longest of any Spellblade talent) paces it deliberately -- sustain
    // this strong needs to be rationed, not spammable every fight.
    Talent renewal{
        /*name=*/"Renewal",
        /*description=*/"Restorative magic, turned on yourself. A long cooldown "
                         "keeps it from trivializing danger.",
        /*tree=*/TalentTree::Flame,
        /*targeting=*/TargetingMode::Self,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/6,
        /*hpCost=*/0,
        /*cooldownTurns=*/6,
        /*power=*/8, // heal amount here, not damage -- +4 from intelligence == 12 hp restored
    };
    renewal.damageType = DamageType::Magic;
    renewal.effectKind = TalentEffectKind::Heal;
    talents.push_back(renewal);

    return talents;
}

} // namespace engine
