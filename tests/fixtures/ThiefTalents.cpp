#include "fixtures/ThiefTalents.hpp"

namespace engine {

std::vector<Talent> thiefTalents() {
    std::vector<Talent> talents;

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
        /*power=*/6, // +2 from strength = 8
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
        /*power=*/6, // +2 from strength = 8
        /*areaRadius=*/2,
    });

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
        /*power=*/4, // +2 from strength = 6
    };
    vaultKick.retreatDistance = 3;
    talents.push_back(vaultKick);

    talents[0].projectile = true; // Quick Shot
    talents[1].projectile = true; // Volley bursts around its first impact.
    talents[0].id = "thief.quick_shot";
    talents[1].id = "thief.volley";
    talents[2].id = "thief.steady_aim";
    talents[3].id = "thief.vault_kick";
    return talents;
}

std::optional<Talent> thiefTalentUnlockedAtLevel(int level) {
    if (level == 4) {
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
        };
        piercingShot.bonusCritChance = 0.2f;
        piercingShot.projectile = true;
        piercingShot.bonusCritDamageMultiplier = 0.5f;
        piercingShot.id = "thief.piercing_shot";
        return piercingShot;
    }
    if (level == 7) {
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
        adrenaline.id = "thief.adrenaline";
        return adrenaline;
    }
    return std::nullopt;
}

} // namespace engine
