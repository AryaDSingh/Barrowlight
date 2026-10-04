#include "entities/StatusEffectLogic.hpp"

#include <algorithm>
#include <utility>

#include "entities/Actor.hpp"
#include "entities/AttributeFormulas.hpp"

namespace engine {

bool tickStatusEffects(Actor& actor) {
    StatusEffects& effects = actor.statusEffects();
    // Gear that wards off ailments: a chance each turn to shake one off.
    for (const auto [type, stat] : {std::pair{StatusEffectType::Burn, BonusStat::ResistBurn}, std::pair{StatusEffectType::Chill, BonusStat::ResistChill},
                                    std::pair{StatusEffectType::Shock, BonusStat::ResistShock}, std::pair{StatusEffectType::Poison, BonusStat::ResistPoison}})
        if (effects.has(type))
            if (const int ward = actor.inventory().affixTotal(stat); ward > 0 && rollChance(ward / 100.f)) effects.remove(type);
    const bool stunnedThisTurn = effects.has(StatusEffectType::Stun);

    // Apply poison damage before decrementing/expiring, so the last
    // active tick of a poison effect still deals its damage.
    bool doomTriggered=false;
    for (const StatusEffectInstance& effect : effects.active()) {
        if (effect.type == StatusEffectType::Poison || effect.type == StatusEffectType::Burn || effect.type == StatusEffectType::Bleed || effect.type == StatusEffectType::Plague) {
            actor.stats().hp -= effect.magnitude;
        }
        if (effect.type==StatusEffectType::ManaDrain)
            actor.stats().mana=std::max(0,actor.stats().mana-effect.magnitude);
        if (effect.type==StatusEffectType::Doom && effect.turnsRemaining==1) {
            actor.stats().hp-=effect.magnitude;
            doomTriggered=true;
        }
    }

    if (doomTriggered || effects.has(StatusEffectType::Poison) || effects.has(StatusEffectType::Burn)) effects.remove(StatusEffectType::Concealed);
    auto& active = effects.active();
    for (StatusEffectInstance& effect : active) {
        --effect.turnsRemaining;
    }
    active.erase(std::remove_if(active.begin(), active.end(),
                                 [](const StatusEffectInstance& e) { return e.turnsRemaining <= 0; }),
                 active.end());

    if (stunnedThisTurn && !effects.has(StatusEffectType::Stun))
        effects.apply({StatusEffectType::StunRecovery,effects.stunRecoveryTurns(),0});
    return stunnedThisTurn;
}

} // namespace engine
