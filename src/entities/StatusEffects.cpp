#include "entities/StatusEffects.hpp"

#include <algorithm>

namespace engine {

void StatusEffects::apply(StatusEffectInstance effect) {
    for (StatusEffectInstance& existing : active_) {
        if (existing.type == effect.type) {
            existing = effect; // refresh in place rather than stacking
            return;
        }
    }
    active_.push_back(effect);
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
