#include "entities/TalentEffects.hpp"

#include <algorithm>

#include "entities/Actor.hpp"
#include "entities/ArmourTalents.hpp"
#include "entities/HiddenCombat.hpp"
#include "entities/AttributeFormulas.hpp"

namespace engine {

TalentDamageEstimate estimateTalentDamage(const Talent& talent,
    const Actor& attacker, const Actor& target) {
    const int statValue = statValueForScalingStat(talent.scalingStat, attacker.stats().strength,
                                                    attacker.stats().dexterity,
                                                    attacker.stats().intelligence);
    int damage = talent.power + abilityDamageBonus(talent.scalingStat, statValue,
        talent.scalingCooldown >= 0 ? talent.scalingCooldown : talent.cooldownTurns);

    // Same Empowered check executeAIDecision already applies to monster
    // attacks (Prompt 11) -- missing here until Prompt 15's Rallying Cry
    // surfaced it live: the Marauder's own self-buff talent was granting
    // Empowered correctly, but nothing on the player-attack path ever
    // consulted it, so the buff silently did nothing. Folded in before
    // the conditional multiplier below, same reasoning as the attribute
    // bonus: an execute is meant to amplify the attacker's full output,
    // buffs included, not just the talent's flat listed number.
    if (attacker.statusEffects().has(StatusEffectType::Empowered)) {
        damage += attacker.statusEffects().magnitudeOf(StatusEffectType::Empowered);
    }

    if (talent.conditionalHpFraction > 0.f && target.stats().maxHp > 0) {
        const float targetHpFraction =
            static_cast<float>(target.stats().hp) / static_cast<float>(target.stats().maxHp);
        if (targetHpFraction <= talent.conditionalHpFraction) {
            damage *= talent.conditionalMultiplier;
        }
    }

    const auto& kit=attacker.talents();
    const auto* weapon=attacker.inventory().equipped(EquipmentSlot::Weapon);
    const auto kind=weapon && weapon->definition() ? weapon->definition()->weaponKind : WeaponKind::None;
    if (attacker.statusEffects().has(StatusEffectType::Concealed)) damage+=kit.passiveValue(PassiveKind::Ambush);
    if (target.statusEffects().has(StatusEffectType::Chill)) damage+=kit.passiveValue(PassiveKind::Frostbite);
    if (kind==WeaponKind::OneHanded && attacker.statusEffects().has(StatusEffectType::Guard) && talent.targeting==TargetingMode::AdjacentEnemy)
        damage+=kit.passiveValue(PassiveKind::Riposte);
    // Ascendancy passives.
    if (attacker.talents().passiveValue(PassiveKind::LastStand) && attacker.stats().hp*3<=attacker.stats().maxHp)
        damage+=kit.passiveValue(PassiveKind::LastStand);
    if (target.stats().maxHp>0 && target.stats().hp*2<target.stats().maxHp) damage+=kit.passiveValue(PassiveKind::KillerInstinct);
    // Hybrid ascendancies.
    if (attacker.stats().maxMana>0 && attacker.stats().mana*2>attacker.stats().maxMana) damage+=kit.passiveValue(PassiveKind::Righteous);
    if (isSpell(talent) && attacker.statusEffects().has(StatusEffectType::Concealed)) damage+=kit.passiveValue(PassiveKind::HiddenCasting);
    if (target.stats().maxHp>0 && target.stats().hp*4<target.stats().maxHp) damage+=kit.passiveValue(PassiveKind::Finisher);
    // Gear: Cruel adds to every attack; Execution to finishing blows.
    damage+=attacker.inventory().affixTotal(BonusStat::FlatDamage);
    damage+=target.statusEffects().magnitudeOf(StatusEffectType::Sundered); // the Mace's broken guard
    if (target.stats().maxHp>0 && target.stats().hp*10<=target.stats().maxHp*3) damage+=attacker.inventory().affixTotal(BonusStat::Execution);
    if (isSpell(talent) && (target.statusEffects().has(StatusEffectType::Burn) || target.statusEffects().has(StatusEffectType::Chill) ||
        target.statusEffects().has(StatusEffectType::Shock))) damage+=kit.passiveValue(PassiveKind::Attunement);
    if (talent.committedBloodlust>=0) damage+=talent.committedBloodlust;
    else if (kind==WeaponKind::TwoHanded && attacker.stats().hp*2<=attacker.stats().maxHp) damage+=kit.passiveValue(PassiveKind::Bloodlust);
    if (isSpell(talent) && (target.statusEffects().has(StatusEffectType::Burn) ||
        target.statusEffects().has(StatusEffectType::Chill) || target.statusEffects().has(StatusEffectType::Shock)))
        damage+=armourPassive(attacker,PassiveKind::Spellweave);
    damage+=talent.committedRhythm>=0?talent.committedRhythm:(isMeleeAttack(talent) && hasMeleeWeapon(attacker)?attacker.statusEffects().magnitudeOf(StatusEffectType::BattleRhythm):0);
    int percent=talent.damagePercent;
    if (talent.releaseAilments) {
        int count=0; for (const auto& e:target.statusEffects().active()) if (releasableAilment(e.type)) ++count;
        percent=percent*(100+25*count)/100;
    }
    if ((talent.consumeShock && target.statusEffects().has(StatusEffectType::Shock)) ||
        (talent.consumeChill && target.statusEffects().has(StatusEffectType::Chill)) ||
        (talent.consumeBurn && target.statusEffects().has(StatusEffectType::Burn))) percent=percent*(100+talent.statusBonusPercent)/100;
    if (target.statusEffects().has(StatusEffectType::Stun)) percent=percent*(100+kit.passiveValue(PassiveKind::CrushingBlows))/100;
    damage=damage*percent/100;
    if (attacker.statusEffects().has(StatusEffectType::Chill)) damage=damage*(100-attacker.statusEffects().magnitudeOf(StatusEffectType::Chill))/100;
    if (target.statusEffects().magnitudeOf(StatusEffectType::Marked)>0) damage=damage*(100+kMarkedDamagePercent)/100;
    int guard=target.statusEffects().magnitudeOf(StatusEffectType::Guard);
    if (guard && target.inventory().equipped(EquipmentSlot::OffHand)) guard+=target.talents().passiveValue(PassiveKind::ShieldTraining);
    guard+=armourGuardBonus(target)+ascendancyGuardBonus(target);
    const bool opportune=attacker.statusEffects().has(StatusEffectType::Concealed) || attacker.statusEffects().has(StatusEffectType::Opening);
    const float critBonus=talent.bonusCritDamageMultiplier+(opportune ? kit.passiveValue(PassiveKind::Opportunist)/100.f : 0.f)+
        attacker.inventory().affixTotal(BonusStat::CritDamage)/100.f;
    const int critical=std::max(0,static_cast<int>(damage*critDamageMultiplier(critBonus))-guard);
    damage=std::max(0,damage-guard);
    return {damage,critical};
}

bool applyTalentDamage(const Talent& talent, Actor& attacker, Actor& target) {
    if (rollChance(std::min(kTotalDodgeCap,dodgeChance(target.stats().dexterity)+(target.statusEffects().magnitudeOf(StatusEffectType::Evasion)+armourDodgeBonus(target)+ascendancyDodgeBonus(target))/100.f))) {
        if (target.talents().passiveValue(PassiveKind::Slippery)) target.statusEffects().apply({StatusEffectType::Opening,2,0});
        return false;
    }
    const auto estimate = estimateTalentDamage(talent, attacker, target);
    int damage = estimate.normal;
    // Global crit: multiplies the result of everything above (including
    // a conditional multiplier like a low-hp execute) rather than
    // adding to it -- "Base Damage x ability modifier x crit modifier"
    // was the explicit design intent. Rolled on the attacker's own
    // Dexterity plus any talent-specific bonus (Piercing Shot's own
    // +20% crit chance/+50% crit damage) -- crit is a property of the
    // one landing the hit, not the one receiving it, unlike dodge.
    const float aim=talent.tree==TalentTree::Bow && attacker.statusEffects().has(StatusEffectType::Opening) ?
        attacker.talents().passiveValue(PassiveKind::Marksmanship)/100.f : 0.f;
    if (rollCrit(attacker.stats().dexterity, talent.bonusCritChance+aim+(armourCritBonus(attacker)+attacker.inventory().affixTotal(BonusStat::CritChance)+attacker.talents().passiveValue(PassiveKind::Versatility))/100.f)) {
        damage = estimate.critical;
    }

    const int actualDamage=std::min(std::max(0,target.stats().hp),damage);
    // Gear on hit: life drain and elemental chances.
    if (damage>0) {
        const auto& gear=attacker.inventory();
        if (const int leech=gear.affixTotal(BonusStat::LifeOnHit)) attacker.stats().hp=std::min(attacker.stats().maxHp,attacker.stats().hp+leech);
        if (target.stats().hp-damage>0) {
            if (rollChance(gear.affixTotal(BonusStat::BurnChance)/100.f)) target.statusEffects().apply({StatusEffectType::Burn,3,2});
            if (rollChance(gear.affixTotal(BonusStat::ChillChance)/100.f)) target.statusEffects().apply({StatusEffectType::Chill,3,20});
            if (rollChance(gear.affixTotal(BonusStat::ShockChance)/100.f)) target.statusEffects().apply({StatusEffectType::Shock,3,0});
        }
    }
    const int wither=target.statusEffects().magnitudeOf(StatusEffectType::Wither);
    target.stats().hp -= damage;
    if (actualDamage>0) attacker.stats().hp=std::min(attacker.stats().maxHp,attacker.stats().hp+
        actualDamage*talent.drainPercent/100+std::min(actualDamage,wither));
    if (talent.releaseAilments) {
        auto& effects=target.statusEffects().active();
        effects.erase(std::remove_if(effects.begin(),effects.end(),[](const auto& e){return releasableAilment(e.type);}),effects.end());
    }
    if (talent.spellstrike && target.stats().hp>0) {
        const int burn=attacker.talents().passiveValue(PassiveKind::Kindle);
        const int shock=attacker.talents().passiveValue(PassiveKind::StaticCharge);
        if (burn) target.statusEffects().apply({StatusEffectType::Burn,2,burn});
        if (shock) target.statusEffects().apply({StatusEffectType::Shock,shock,0});
    }
    target.statusEffects().consumeMark();
    if (isMeleeAttack(talent)) applyImbueHit(attacker,target);
    if (damage>0) target.statusEffects().remove(StatusEffectType::Concealed);
    if (const int conduit=attacker.talents().passiveValue(PassiveKind::Conduit);
        conduit && ((talent.consumeShock && target.statusEffects().has(StatusEffectType::Shock)) ||
                    (talent.consumeBurn && target.statusEffects().has(StatusEffectType::Burn)) ||
                    (talent.consumeChill && target.statusEffects().has(StatusEffectType::Chill))))
        attacker.stats().mana=std::min(attacker.stats().maxMana,attacker.stats().mana+conduit);
    if (talent.consumeShock) target.statusEffects().remove(StatusEffectType::Shock);
    if (talent.consumeBurn) target.statusEffects().remove(StatusEffectType::Burn);
    if (talent.consumeChill && target.statusEffects().has(StatusEffectType::Chill)) {
        target.statusEffects().remove(StatusEffectType::Chill);
        if (target.stats().hp>0 && !armourResistsStun(target)) target.statusEffects().apply({StatusEffectType::Stun,1,0});
    }

    // Sorcerer's Mind Shatter (Prompt 19): a status effect applied to
    // the *target* on a successful hit, mirroring how
    // MonsterAttackProfile's onHitEffect has worked since Prompt 10 --
    // just available to a player talent for the first time. Only rolled
    // if the target survived the hit -- applying Stun to something
    // already dead has no meaning.
    if (talent.onHitEffect.has_value() && target.stats().hp > 0 &&
        rollChance(talent.onHitChance)) {
        auto effect=*talent.onHitEffect;
        const auto ailment=[](StatusEffectType t){ return t==StatusEffectType::Burn || t==StatusEffectType::Chill || t==StatusEffectType::Shock; };
        if (ailment(effect.type)) {
            effect.turnsRemaining+=attacker.talents().passiveValue(PassiveKind::LingeringElements);
            // Elemental Overload: a second, different ailment bursts.
            const int burst=attacker.talents().passiveValue(PassiveKind::Overload);
            bool other=false;
            for (const auto& e:target.statusEffects().active()) other=other || (ailment(e.type) && e.type!=effect.type);
            if (burst && other) target.stats().hp-=burst;
        }
        if (effect.type!=StatusEffectType::Stun || !armourResistsStun(target))
            target.statusEffects().apply(effect);
    }

    return true;
}

void applyTalentHeal(const Talent& talent, Actor& caster, Actor& target) {
    // Always Intelligence-scaled, regardless of talent.scalingStat --
    // healing is life magic, not a physical strike, so there's no
    // Strength-scaled equivalent the way there is for damage. Treated
    // as Filler tier (cooldown 0) for the bonus calculation regardless
    // of the healing talent's own actual cooldown -- healing doesn't
    // participate in the same "bigger cooldown, bigger payoff" tier
    // reasoning damage talents do; this project has exactly one heal
    // (Renewal) and it stays a modest, reliable topper-off rather than
    // scaling with its own 6-turn cooldown the way a damage talent
    // would.
    const int healAmount =
        talent.power + abilityDamageBonus(ScalingStat::Intelligence, caster.stats().intelligence, 0);
    target.stats().hp = std::min(target.stats().hp + healAmount, target.stats().maxHp);
}

void applyTalentSelfBuff(const Talent& talent, Actor& caster) {
    if (!armourMatches(caster,talent.armourRequirement)) return;
    if (talent.restoreMana>0) caster.stats().mana=std::min(caster.stats().maxMana,caster.stats().mana+talent.restoreMana);
    if (talent.restoreHpPercent>0) caster.stats().hp=std::min(caster.stats().maxHp,
        caster.stats().hp+(caster.stats().maxHp*talent.restoreHpPercent+99)/100);
    if (talent.cleanse) {
        auto& effects=caster.statusEffects().active();
        effects.erase(std::remove_if(effects.begin(),effects.end(),[](const auto& e){return isCleansable(e.type);}),effects.end());
    }
    if (talent.imbueElement) {
        for (int i=0;i<4;++i) caster.statusEffects().remove(static_cast<StatusEffectType>(static_cast<int>(StatusEffectType::FlameBlade)+i));
    }
    if (talent.huntersMark) caster.statusEffects().apply({StatusEffectType::Marked,3,1});
    if (talent.id=="stealth.conceal") caster.statusEffects().apply({StatusEffectType::UnseenReady,10000,1});
    if (talent.selfBuffEffect.has_value()) {
        caster.statusEffects().apply(*talent.selfBuffEffect);
    }
}

} // namespace engine
