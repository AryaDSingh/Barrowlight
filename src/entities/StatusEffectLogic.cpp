#include "entities/StatusEffectLogic.hpp"

#include <algorithm>

#include "entities/Actor.hpp"

namespace engine {

bool tickStatusEffects(Actor& actor) {
    StatusEffects& effects = actor.statusEffects();
    const bool stunnedThisTurn = effects.has(StatusEffectType::Stun);

    // Apply poison damage before decrementing/expiring, so the last
    // active tick of a poison effect still deals its damage.
    for (const StatusEffectInstance& effect : effects.active()) {
        if (effect.type == StatusEffectType::Poison) {
            actor.stats().hp -= effect.magnitude;
        }
    }

    auto& active = effects.active();
    for (StatusEffectInstance& effect : active) {
        --effect.turnsRemaining;
    }
    active.erase(std::remove_if(active.begin(), active.end(),
                                 [](const StatusEffectInstance& e) { return e.turnsRemaining <= 0; }),
                 active.end());

    return stunnedThisTurn;
}

} // namespace engine
