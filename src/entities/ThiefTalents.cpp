#include "entities/ThiefTalents.hpp"

namespace engine {

std::vector<Talent> thiefTalents() {
    std::vector<Talent> talents;

    // Every base `power` below already has the Thief's own strength
    // (14, see PlayerClassFactory) baked in via physicalDamageBonus(14)
    // == 2 -- same recalibration discipline as every other class's kit
    // (Prompt 14): the number written here plus the formula bonus
    // equals the intended total, not the total itself. All four are
    // left at DamageType::Physical (the default) -- Intelligence sits
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
        // Piercing Shot -- reuses Execution's exact conditional-
        // multiplier mechanic (Prompt 9: modest damage normally, but a
        // real multiplier against a target already below 30% hp) for
        // the first time outside the Spellblade. Base total (12) is
        // noticeably higher than Quick Shot's (8), on a real cooldown
        // to match.
        Talent piercingShot{
            /*name=*/"Piercing Shot",
            /*description=*/"Modest damage normally, but triples against a "
                             "target already below 30% hp. Rewards good "
                             "timing, not raw power.",
            /*tree=*/TalentTree::Blade,
            /*targeting=*/TargetingMode::RangedEnemyInSight,
            /*shape=*/EffectShape::SingleTarget,
            /*manaCost=*/5,
            /*hpCost=*/0,
            /*cooldownTurns=*/4,
            /*power=*/10, // +2 from strength == 12
            /*areaRadius=*/0,
            /*moveDistance=*/0,
            /*conditionalHpFraction=*/0.3f,
            /*conditionalMultiplier=*/3,
        };
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
