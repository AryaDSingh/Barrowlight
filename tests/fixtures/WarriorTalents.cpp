#include "fixtures/WarriorTalents.hpp"

namespace engine {

std::vector<Talent> warriorTalents() {
    std::vector<Talent> talents;

    talents.push_back(Talent{
        /*name=*/"Slam",
        /*description=*/"A free, spammable strike -- no mana cost at all. The "
                         "Warrior's bread and butter.",
        /*tree=*/TalentTree::Blade,
        /*targeting=*/TargetingMode::AdjacentEnemy,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/0,
        /*hpCost=*/0,
        /*cooldownTurns=*/1,
        /*power=*/4, // +5 from strength = 9
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
        /*power=*/5, // +5 from strength = 10
        /*areaRadius=*/1,
    });

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
        /*power=*/23, // +5 from strength = 28
    });

    talents[0].id = "warrior.slam";
    talents[1].id = "warrior.cleave";
    talents[2].id = "warrior.rallying_cry";
    talents[3].id = "warrior.berserkers_fury";
    return talents;
}

std::optional<Talent> warriorTalentUnlockedAtLevel(int level) {
    if (level == 4) {
        Talent whirlwind{
            /*name=*/"Whirlwind",
            /*description=*/"A sweeping spin hitting a wider area than Cleave "
                             "ever could. Costs more to match.",
            /*tree=*/TalentTree::Blade,
            /*targeting=*/TargetingMode::Self,
            /*shape=*/EffectShape::AreaAroundSelf,
            /*manaCost=*/6,
            /*hpCost=*/0,
            /*cooldownTurns=*/5,
            /*power=*/9, // +5 from strength = 14
            /*areaRadius=*/2,
        };
        whirlwind.id = "warrior.whirlwind";
        return whirlwind;
    }
    if (level == 7) {
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
        undyingRage.id = "warrior.undying_rage";
        return undyingRage;
    }
    return std::nullopt;
}

} // namespace engine
