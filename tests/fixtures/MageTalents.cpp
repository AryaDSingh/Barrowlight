#include "fixtures/MageTalents.hpp"

namespace engine {

std::vector<Talent> mageTalents() {
    std::vector<Talent> talents;

    Talent arcaneBolt{
        /*name=*/"Arcane Bolt",
        /*description=*/"A quick bolt of raw arcane force -- no need to be "
                         "adjacent, just within sight.",
        /*tree=*/TalentTree::Flame,
        /*targeting=*/TargetingMode::RangedEnemyInSight,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/2,
        /*hpCost=*/0,
        /*cooldownTurns=*/1,
        /*power=*/2, // +7 from intelligence = 9
    };
    arcaneBolt.scalingStat = ScalingStat::Intelligence;
    arcaneBolt.projectile = true;
    talents.push_back(arcaneBolt);

    Talent arcaneStorm{
        /*name=*/"Arcane Storm",
        /*description=*/"Damages everything around the target. Costly, and slow "
                         "to recharge.",
        /*tree=*/TalentTree::Flame,
        /*targeting=*/TargetingMode::RangedEnemyInSight,
        /*shape=*/EffectShape::AreaAroundTarget,
        /*manaCost=*/7,
        /*hpCost=*/0,
        /*cooldownTurns=*/4,
        /*power=*/6, // +7 from intelligence = 13
        /*areaRadius=*/2,
    };
    arcaneStorm.scalingStat = ScalingStat::Intelligence;
    talents.push_back(arcaneStorm);

    Talent arcaneFocus{
        /*name=*/"Arcane Focus",
        /*description=*/"Channel raw power into your next several spells. A real "
                         "cooldown -- this isn't free damage every fight.",
        /*tree=*/TalentTree::Flame,
        /*targeting=*/TargetingMode::Self,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/4,
        /*hpCost=*/0,
        /*cooldownTurns=*/6,
    };
    arcaneFocus.effectKind = TalentEffectKind::SelfBuff;
    arcaneFocus.selfBuffEffect = StatusEffectInstance{StatusEffectType::Empowered, 5, 4};
    talents.push_back(arcaneFocus);

    Talent mindShatter{
        /*name=*/"Mind Shatter",
        /*description=*/"A jarring pulse of psychic force, ranged. Deals modest "
                         "damage but has a real chance to stun the target.",
        /*tree=*/TalentTree::Flame,
        /*targeting=*/TargetingMode::RangedEnemyInSight,
        /*shape=*/EffectShape::SingleTarget,
        /*manaCost=*/5,
        /*hpCost=*/0,
        /*cooldownTurns=*/5,
        /*power=*/3, // +7 from intelligence = 10
    };
    mindShatter.scalingStat = ScalingStat::Intelligence;
    mindShatter.onHitEffect = StatusEffectInstance{StatusEffectType::Stun, 1, 0};
    mindShatter.onHitChance = 0.6f;
    talents.push_back(mindShatter);

    talents[0].id = "mage.arcane_bolt";
    talents[1].id = "mage.arcane_storm";
    talents[2].id = "mage.arcane_focus";
    talents[3].id = "mage.mind_shatter";
    return talents;
}

std::optional<Talent> mageTalentUnlockedAtLevel(int level) {
    if (level == 2) {
        Talent blink;
        blink.id = "mage.blink";
        blink.name = "Blink";
        blink.description = "Move up to 3 visible tiles, stopping before terrain or actors.";
        blink.tree = TalentTree::Flame;
        blink.targeting = TargetingMode::Self;
        blink.shape = EffectShape::Movement;
        blink.manaCost = 4;
        blink.cooldownTurns = 4;
        blink.moveDistance = 3;
        return blink;
    }
    if (level == 4) {
        Talent meteor{
            /*name=*/"Meteor",
            /*description=*/"The single hardest-hitting spell in this kit. A "
                             "real mana cost and a long cooldown to match.",
            /*tree=*/TalentTree::Flame,
            /*targeting=*/TargetingMode::RangedEnemyInSight,
            /*shape=*/EffectShape::SingleTarget,
            /*manaCost=*/10,
            /*hpCost=*/0,
            /*cooldownTurns=*/6,
            /*power=*/11, // +7 from intelligence = 18
        };
        meteor.scalingStat = ScalingStat::Intelligence;
        meteor.id = "mage.meteor";
        return meteor;
    }
    if (level == 7) {
        Talent overload{
            /*name=*/"Overload",
            /*description=*/"A deeper channel than Arcane Focus ever granted "
                             "-- longer, and stronger.",
            /*tree=*/TalentTree::Flame,
            /*targeting=*/TargetingMode::Self,
            /*shape=*/EffectShape::SingleTarget,
            /*manaCost=*/6,
            /*hpCost=*/0,
            /*cooldownTurns=*/7,
        };
        overload.effectKind = TalentEffectKind::SelfBuff;
        overload.selfBuffEffect = StatusEffectInstance{StatusEffectType::Empowered, 6, 6};
        overload.id = "mage.overload";
        return overload;
    }
    return std::nullopt;
}

} // namespace engine
