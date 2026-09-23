#include "entities/MarauderTalents.hpp"

namespace engine {

std::vector<Talent> marauderTalents() {
    std::vector<Talent> talents;

    // Every base `power` below already has the Marauder's own strength
    // (20, see PlayerClassFactory) baked in via physicalDamageBonus(20)
    // == 5 -- same recalibration discipline as the Spellblade's kit
    // (Prompt 14): the number written here plus the formula bonus equals
    // the intended total, not the total itself.

    talents.push_back(Talent{
        /*name=*/"Slam",
        /*description=*/"A free, spammable strike -- no mana cost at all. The "
                         "Marauder's bread and butter.",
        /*tree=*/TalentTree::Blade,
        /*targeting=*/TargetingMode::AdjacentEnemy,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/0,
        /*hpCost=*/0,
        /*cooldownTurns=*/1,
        /*power=*/4, // +5 from strength == 9
    });

    talents.push_back(Talent{
        /*name=*/"Cleave",
        /*description=*/"A wide swing hitting everything adjacent, not just one "
                         "target. Genuinely useful against more than one enemy "
                         "at once.",
        /*tree=*/TalentTree::Blade,
        /*targeting=*/TargetingMode::Self,
        /*shape=*/EffectShape::AreaAroundSelf,
        /*manaCost=*/4,
        /*hpCost=*/0,
        /*cooldownTurns=*/3,
        /*power=*/5, // +5 from strength == 10 per enemy hit
        /*areaRadius=*/1,
    });

    // Rallying Cry -- SelfBuff, not Damage. targeting=Self + shape=
    // SingleTarget resolves to the caster directly (the same resolution
    // Renewal uses, see Application::tryUseTalent), not AreaAroundSelf's
    // "nearby enemies" search. power is unused for SelfBuff (left at its
    // 0 default); the actual effect is selfBuffEffect below, applying
    // Empowered -- the same status effect the boss's own enrage uses
    // (Prompt 11), reused rather than reinvented.
    Talent rallyingCry{
        /*name=*/"Rallying Cry",
        /*description=*/"A battle shout that empowers your next several "
                         "strikes. A real cooldown -- this isn't free damage "
                         "every fight.",
        /*tree=*/TalentTree::Blade,
        /*targeting=*/TargetingMode::Self,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/3,
        /*hpCost=*/0,
        /*cooldownTurns=*/6,
    };
    rallyingCry.effectKind = TalentEffectKind::SelfBuff;
    rallyingCry.selfBuffEffect = StatusEffectInstance{StatusEffectType::Empowered, 5, 4};
    talents.push_back(rallyingCry);

    talents.push_back(Talent{
        /*name=*/"Berserker's Fury",
        /*description=*/"Spend your own blood for the hardest-hitting strike "
                         "either class has. Costs hp, not mana -- high risk, "
                         "high reward.",
        /*tree=*/TalentTree::Blade,
        /*targeting=*/TargetingMode::AdjacentEnemy,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/0,
        /*hpCost=*/8,
        /*cooldownTurns=*/4,
        /*power=*/23, // +5 from strength == 28
    });

    return talents;
}

} // namespace engine
