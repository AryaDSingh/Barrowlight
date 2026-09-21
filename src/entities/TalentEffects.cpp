#include "entities/TalentEffects.hpp"

#include <algorithm>

#include "entities/Actor.hpp"
#include "entities/AttributeFormulas.hpp"

namespace engine {

bool applyTalentDamage(const Talent& talent, Actor& attacker, Actor& target) {
    if (rollDodge(target.stats().dexterity)) {
        return false;
    }

    int damage = talent.power;
    damage += (talent.damageType == DamageType::Physical)
                  ? physicalDamageBonus(attacker.stats().strength)
                  : magicDamageBonus(attacker.stats().intelligence);

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

    target.stats().hp -= damage;
    return true;
}

void applyTalentHeal(const Talent& talent, Actor& caster, Actor& target) {
    // Always Intelligence-scaled, regardless of talent.damageType --
    // healing is life magic, not a physical strike, so there's no
    // Strength-scaled equivalent the way there is for damage.
    const int healAmount = talent.power + magicDamageBonus(caster.stats().intelligence);
    target.stats().hp = std::min(target.stats().hp + healAmount, target.stats().maxHp);
}

void applyTalentSelfBuff(const Talent& talent, Actor& caster) {
    if (talent.selfBuffEffect.has_value()) {
        caster.statusEffects().apply(*talent.selfBuffEffect);
    }
}

} // namespace engine
