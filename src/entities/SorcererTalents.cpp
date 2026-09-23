#include "entities/SorcererTalents.hpp"

namespace engine {

std::vector<Talent> sorcererTalents() {
    std::vector<Talent> talents;

    // Every base `power` below already has the Sorcerer's own
    // intelligence (24, see PlayerClassFactory) baked in via
    // magicDamageBonus(24) == 7 -- same recalibration discipline as
    // every other class's kit: the number written here plus the
    // formula bonus equals the intended total, not the total itself.
    // Each Damage-kind talent below explicitly sets damageType to
    // Magic -- Physical, not Magic, is Talent's actual struct default
    // (a real bug caught by sorcerer_test on the first build: leaving
    // it unset would have scaled every one of these off the Sorcerer's
    // dump-stat Strength instead of its dominant Intelligence).

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
        /*power=*/2, // +7 from intelligence == 9
    };
    arcaneBolt.damageType = DamageType::Magic;
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
        /*power=*/6, // +7 from intelligence == 13 per enemy hit
        /*areaRadius=*/2,
    };
    arcaneStorm.damageType = DamageType::Magic;
    talents.push_back(arcaneStorm);

    // Arcane Focus -- SelfBuff, not Damage, so damageType is irrelevant
    // (left at its Physical default, same as Blink's own non-damage
    // Movement talent). targeting=Self + shape=SingleTarget resolves to
    // the caster directly (the same resolution Renewal/Rallying Cry/
    // Steady Aim all already use), not AreaAroundSelf's "nearby
    // enemies" search.
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

    // Mind Shatter -- the signature move. Modest damage; the real value
    // is the on-hit Stun (see Talent::onHitEffect/onHitChance), the
    // first player talent to apply a status effect to its *target*
    // rather than the caster. Only triggers on a successful hit, never
    // guaranteed even then (60% -- real, not overwhelming, counterplay
    // exists on the enemy's side via its own dodge chance already
    // gating whether the hit lands at all).
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
        /*power=*/3, // +7 from intelligence == 10
    };
    mindShatter.damageType = DamageType::Magic;
    mindShatter.onHitEffect = StatusEffectInstance{StatusEffectType::Stun, 1, 0};
    mindShatter.onHitChance = 0.6f;
    talents.push_back(mindShatter);

    return talents;
}

std::optional<Talent> sorcererTalentUnlockedAtLevel(int level) {
    if (level == 4) {
        // Meteor -- the single hardest-hitting talent in the kit, this
        // class's answer to the Fighter's Berserker's Fury. A mana/
        // cooldown cost rather than an hp one, matching how this
        // class's whole kit already spends mana instead of blood.
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
            /*power=*/11, // +7 from intelligence == 18
        };
        meteor.damageType = DamageType::Magic;
        return meteor;
    }
    if (level == 7) {
        // Overload -- a genuine upgrade on Arcane Focus: longer,
        // stronger Empowered (6 turns at magnitude 6, versus 5 turns at
        // magnitude 4).
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
        return overload;
    }
    return std::nullopt;
}

} // namespace engine
