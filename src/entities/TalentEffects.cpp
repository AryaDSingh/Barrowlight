#include "entities/TalentEffects.hpp"

#include "entities/Actor.hpp"

namespace engine {

void applyTalentDamage(const Talent& talent, Actor& target) {
    int damage = talent.power;

    if (talent.conditionalHpFraction > 0.f && target.stats().maxHp > 0) {
        const float targetHpFraction =
            static_cast<float>(target.stats().hp) / static_cast<float>(target.stats().maxHp);
        if (targetHpFraction <= talent.conditionalHpFraction) {
            damage *= talent.conditionalMultiplier;
        }
    }

    target.stats().hp -= damage;
}

} // namespace engine
