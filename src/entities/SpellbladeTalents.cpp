#include "entities/SpellbladeTalents.hpp"

namespace engine {

std::vector<Talent> spellbladeTalents() {
    std::vector<Talent> talents;

    // --- Blade tree ---

    talents.push_back(Talent{
        /*name=*/"Quick Strike",
        /*description=*/"A fast, efficient melee strike. Low cost, low cooldown.",
        /*tree=*/TalentTree::Blade,
        /*targeting=*/TargetingMode::AdjacentEnemy,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/2,
        /*hpCost=*/0,
        /*cooldownTurns=*/1,
        /*power=*/6,
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
        /*power=*/16,
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
        /*power=*/22,
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
        /*power=*/10,
        /*areaRadius=*/0,
        /*moveDistance=*/0,
        /*conditionalHpFraction=*/0.3f,
        /*conditionalMultiplier=*/3,
    });

    // --- Flame tree ---

    talents.push_back(Talent{
        /*name=*/"Ember Bolt",
        /*description=*/"A ranged bolt of flame. No need to be adjacent -- just "
                         "within sight.",
        /*tree=*/TalentTree::Flame,
        /*targeting=*/TargetingMode::RangedEnemyInSight,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/3,
        /*hpCost=*/0,
        /*cooldownTurns=*/1,
        /*power=*/7,
    });

    talents.push_back(Talent{
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
        /*power=*/12,
        /*areaRadius=*/2,
    });

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

    talents.push_back(Talent{
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
        /*power=*/9,
        /*areaRadius=*/1,
    });

    return talents;
}

} // namespace engine
