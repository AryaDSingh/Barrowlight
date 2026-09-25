#include "entities/ThiefTalents.hpp"

namespace engine {

std::vector<Talent> thiefTalents() {
    std::vector<Talent> talents;

    // Every base `power` below already has the Thief's own strength
    // (14, see PlayerClassFactory) baked in via physicalDamageBonus(14)
    // == 2 -- same recalibration discipline as every other class's kit
    // (Prompt 14): the number written here plus the formula bonus
    // equals the intended total, not the total itself. All four are
    // left at ScalingStat::Strength (the default) -- Intelligence sits
    // below baseline for this class, so Magic-typed damage would
    // actively be worse, not just unused upside.

    talents.push_back(Talent{
        /*name=*/"Quick Shot",
        /*description=*/"A fast, efficient shot -- no need to be adjacent, just "
                         "within sight.",
        /*tree=*/TalentTree::Blade,
        /*targeting=*/TargetingMode::RangedEnemyInSight,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/2,
        /*hpCost=*/0,
        /*cooldownTurns=*/1,
        /*power=*/6, // +2 from strength == 8
    });

    talents.push_back(Talent{
        /*name=*/"Volley",
        /*description=*/"A spread of arrows into a cluster, not just one target. "
                         "Costly and slow to recharge -- inefficient against a "
                         "single enemy, but hits everyone nearby.",
        /*tree=*/TalentTree::Blade,
        /*targeting=*/TargetingMode::RangedEnemyInSight,
        /*shape=*/EffectShape::AreaAroundTarget,
        /*manaCost=*/6,
        /*hpCost=*/0,
        /*cooldownTurns=*/4,
        /*power=*/6, // +2 from strength == 8 per enemy hit
        /*areaRadius=*/2,
    });

    // Steady Aim -- SelfBuff, not Damage. targeting=Self + shape=
    // SingleTarget resolves to the caster directly (the same
    // resolution Renewal and Rallying Cry both already use), not
    // AreaAroundSelf's "nearby enemies" search.
    Talent steadyAim{
        /*name=*/"Steady Aim",
        /*description=*/"Take a breath, steady your hands. Empowers your next "
                         "several shots. A real cooldown -- this isn't free "
                         "damage every fight.",
        /*tree=*/TalentTree::Blade,
        /*targeting=*/TargetingMode::Self,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/3,
        /*hpCost=*/0,
        /*cooldownTurns=*/6,
    };
    steadyAim.effectKind = TalentEffectKind::SelfBuff;
    steadyAim.selfBuffEffect = StatusEffectInstance{StatusEffectType::Empowered, 5, 4};
    talents.push_back(steadyAim);

    // Vault Kick -- the signature move. AdjacentEnemy + SingleTarget, so
    // it goes through the ordinary Damage path in
    // Application::tryUseTalent (applyTalentDamage, dodge and all), then
    // that same code checks retreatDistance afterward and moves the
    // caster away from the target regardless of whether the kick itself
    // landed -- the retreat is the caster's own follow-through motion,
    // not an on-hit effect that a dodge would block. Modest damage on
    // purpose: the point is disengaging from melee range with a hit
    // landed along the way, not a big single-target nuke.
    Talent vaultKick{
        /*name=*/"Vault Kick",
        /*description=*/"Kick an adjacent enemy and vault backwards away from "
                         "them. Modest damage -- the point is the escape, not "
                         "the hit.",
        /*tree=*/TalentTree::Blade,
        /*targeting=*/TargetingMode::AdjacentEnemy,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/3,
        /*hpCost=*/0,
        /*cooldownTurns=*/4,
        /*power=*/4, // +2 from strength == 6 -- deliberately modest
    };
    vaultKick.retreatDistance = 3;
    talents.push_back(vaultKick);

    return talents;
}

std::optional<Talent> thiefTalentUnlockedAtLevel(int level) {
    if (level == 4) {
        // Piercing Shot -- reworked during the attribute-system redesign
        // from its original conditional-multiplier mechanic (which
        // reused Execution's "triples below 30% hp" design, the first
        // time that specific mechanic was used outside the Spellblade)
        // into something that leans into the new global crit system
        // instead: an inherent +20% crit chance and +50% increased crit
        // damage, so a Piercing Shot crit deals 2.0x rather than the
        // normal 1.5x. Base total (10, still noticeably higher than
        // Quick Shot's 6) unchanged from the original design; cooldown
        // raised from 4 to 7 to match the new mechanic's higher ceiling
        // (an inherent, always-on crit-chance boost is a stronger
        // baseline guarantee than a conditional multiplier that only
        // ever mattered against a specific, narrow hp window).
        Talent piercingShot{
            /*name=*/"Piercing Shot",
            /*description=*/"A precise shot with a real chance to land a "
                             "devastating critical hit. A long cooldown to "
                             "match.",
            /*tree=*/TalentTree::Blade,
            /*targeting=*/TargetingMode::RangedEnemyInSight,
            /*shape=*/EffectShape::SingleTarget,
            /*manaCost=*/5,
            /*hpCost=*/0,
            /*cooldownTurns=*/7,
            /*power=*/10, // +1 from strength (2/5 * Signature-tier 2.5, truncated to 1) == 11 --
                          // confirmed live, not just calculated: this is the exact total a
                          // real Piercing Shot dealt during verification
        };
        piercingShot.bonusCritChance = 0.2f;
        piercingShot.bonusCritDamageMultiplier = 0.5f;
        return piercingShot;
    }
    if (level == 7) {
        // Adrenaline -- a genuine upgrade on Steady Aim: longer,
        // stronger Empowered (6 turns at magnitude 6, versus 5 turns at
        // magnitude 4).
        Talent adrenaline{
            /*name=*/"Adrenaline",
            /*description=*/"A deeper focus than Steady Aim ever granted -- "
                             "longer, and stronger.",
            /*tree=*/TalentTree::Blade,
            /*targeting=*/TargetingMode::Self,
            /*shape=*/EffectShape::SingleTarget,
            /*manaCost=*/4,
            /*hpCost=*/0,
            /*cooldownTurns=*/7,
        };
        adrenaline.effectKind = TalentEffectKind::SelfBuff;
        adrenaline.selfBuffEffect = StatusEffectInstance{StatusEffectType::Empowered, 6, 6};
        return adrenaline;
    }
    return std::nullopt;
}

} // namespace engine
