#include "entities/StatusEffects.hpp"

#include <algorithm>

namespace engine {

void StatusEffects::apply(StatusEffectInstance effect) {
    if (effect.turnsRemaining<=0) return;
    if (effect.type==StatusEffectType::Stun) {
        if (!canReceiveStun()) return;
        effect.turnsRemaining=std::min(effect.turnsRemaining,maxStunDuration_);
    }
    if (effect.type==StatusEffectType::Marked) effect.magnitude=1; // refresh, never stack charges
    for (StatusEffectInstance& existing : active_) {
        if (existing.type == effect.type) {
            if (effect.type==StatusEffectType::Evasion) {
                existing.magnitude=std::max(existing.magnitude,effect.magnitude);
                existing.turnsRemaining=std::max(existing.turnsRemaining,effect.turnsRemaining);
            } else existing = effect; // refresh rather than stack
            return;
        }
    }
    active_.push_back(effect);
}

void StatusEffects::consumeMark() {
    for (auto& effect:active_) if (effect.type==StatusEffectType::Marked && effect.magnitude>0) --effect.magnitude;
    active_.erase(std::remove_if(active_.begin(),active_.end(),[](const auto& effect) {
        return effect.type==StatusEffectType::Marked && effect.magnitude<=0;
    }),active_.end());
}

void StatusEffects::remove(StatusEffectType type) {
    active_.erase(std::remove_if(active_.begin(), active_.end(), [=](const auto& e) { return e.type == type; }), active_.end());
}

bool StatusEffects::has(StatusEffectType type) const {
    return std::any_of(active_.begin(), active_.end(),
                        [type](const StatusEffectInstance& e) { return e.type == type; });
}

int StatusEffects::magnitudeOf(StatusEffectType type) const {
    for (const StatusEffectInstance& e : active_) {
        if (e.type == type) {
            return e.magnitude;
        }
    }
    return 0;
}

} // namespace engine
