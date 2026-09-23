#include "entities/FighterTalents.hpp"

namespace engine {

std::vector<Talent> fighterTalents() {
    std::vector<Talent> talents;

    // Every base `power` below already has the Fighter's own strength
    // (20, see PlayerClassFactory) baked in via physicalDamageBonus(20)
    // == 5 -- same recalibration discipline as the Spellblade's kit
    // (Prompt 14): the number written here plus the formula bonus equals
    // the intended total, not the total itself.

    talents.push_back(Talent{
        /*name=*/"Slam",
        /*description=*/"A free, spammable strike -- no mana cost at all. The "
                         "Fighter's bread and butter.",
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

std::optional<Talent> fighterTalentUnlockedAtLevel(int level) {
    if (level == 4) {
        // Whirlwind -- a genuine upgrade on Cleave, not a reskin: wider
        // radius (2, not 1) hits more enemies at once, and more damage
        // per hit too (14 total vs Cleave's 10), at a real mana/cooldown
        // premium to match.
        return Talent{
            /*name=*/"Whirlwind",
            /*description=*/"A sweeping spin hitting a wider area than Cleave "
                             "ever could. Costs more to match.",
            /*tree=*/TalentTree::Blade,
            /*targeting=*/TargetingMode::Self,
            /*shape=*/EffectShape::AreaAroundSelf,
            /*manaCost=*/6,
            /*hpCost=*/0,
            /*cooldownTurns=*/5,
            /*power=*/9, // +5 from strength == 14 per enemy hit
            /*areaRadius=*/2,
        };
    }
    if (level == 7) {
        // Undying Rage -- a genuine upgrade on Rallying Cry: longer,
        // stronger Empowered (6 turns at magnitude 6, versus 5 turns at
        // magnitude 4).
        Talent undyingRage{
            /*name=*/"Undying Rage",
            /*description=*/"A deeper battle fury than Rallying Cry ever "
                             "granted -- longer, and stronger.",
            /*tree=*/TalentTree::Blade,
            /*targeting=*/TargetingMode::Self,
            /*shape=*/EffectShape::SingleTarget,
            /*manaCost=*/4,
            /*hpCost=*/0,
            /*cooldownTurns=*/7,
        };
        undyingRage.effectKind = TalentEffectKind::SelfBuff;
        undyingRage.selfBuffEffect = StatusEffectInstance{StatusEffectType::Empowered, 6, 6};
        return undyingRage;
    }
    return std::nullopt;
}

} // namespace engine
