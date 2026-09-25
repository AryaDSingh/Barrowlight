#include "entities/TalentEffects.hpp"

#include <algorithm>

#include "entities/Actor.hpp"
#include "entities/AttributeFormulas.hpp"

namespace engine {

bool applyTalentDamage(const Talent& talent, Actor& attacker, Actor& target) {
    if (rollDodge(target.stats().dexterity)) {
        return false;
    }

    const int statValue = statValueForScalingStat(talent.scalingStat, attacker.stats().strength,
                                                    attacker.stats().dexterity,
                                                    attacker.stats().intelligence);
    int damage = talent.power + abilityDamageBonus(talent.scalingStat, statValue, talent.cooldownTurns);

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

    // Global crit: multiplies the result of everything above (including
    // a conditional multiplier like a low-hp execute) rather than
    // adding to it -- "Base Damage x ability modifier x crit modifier"
    // was the explicit design intent. Rolled on the attacker's own
    // Dexterity plus any talent-specific bonus (Piercing Shot's own
    // +20% crit chance/+50% crit damage) -- crit is a property of the
    // one landing the hit, not the one receiving it, unlike dodge.
    if (rollCrit(attacker.stats().dexterity, talent.bonusCritChance)) {
        damage = static_cast<int>(static_cast<float>(damage) *
                                   critDamageMultiplier(talent.bonusCritDamageMultiplier));
    }

    target.stats().hp -= damage;

    // Sorcerer's Mind Shatter (Prompt 19): a status effect applied to
    // the *target* on a successful hit, mirroring how
    // MonsterAttackProfile's onHitEffect has worked since Prompt 10 --
    // just available to a player talent for the first time. Only rolled
    // if the target survived the hit -- applying Stun to something
    // already dead has no meaning.
    if (talent.onHitEffect.has_value() && target.stats().hp > 0 &&
        rollChance(talent.onHitChance)) {
        target.statusEffects().apply(*talent.onHitEffect);
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
    if (talent.selfBuffEffect.has_value()) {
        caster.statusEffects().apply(*talent.selfBuffEffect);
    }
}

} // namespace engine
