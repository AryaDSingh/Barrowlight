#include "ai/Support.hpp"

#include "entities/Actor.hpp"

namespace engine {

Support::Support(int buffMagnitude, int buffDuration, int buffRadius)
    : buffMagnitude_(buffMagnitude), buffDuration_(buffDuration), buffRadius_(buffRadius) {}

AIDecision Support::decideAction(const Actor& self, const Map& /*map*/, Actor& /*player*/,
                                  const std::vector<Actor*>& allies) {
    // Support doesn't care about the player directly -- it's watching
    // its allies, not tracking a target. `map`/`player` aren't needed
    // for that, but stay in the signature for interface consistency
    // with every other behavior. Deliberately no line-of-sight check on
    // allies either (raw distance only) -- a support unit sensing its
    // own side doesn't need to literally see around a corner.

    if (self.talents().knownTalents().empty() || !self.talents().isReady(0)) {
        return AIDecision{};
    }

    for (Actor* ally : allies) {
        if (ally == nullptr || ally->stats().hp <= 0) {
            continue;
        }
        if (ally->statusEffects().has(StatusEffectType::Empowered)) {
            continue; // already buffed -- look for someone who isn't
        }

        const int dx = ally->position().x - self.position().x;
        const int dy = ally->position().y - self.position().y;
        if (dx * dx + dy * dy > buffRadius_ * buffRadius_) {
            continue;
        }

        AIDecision decision;
        decision.type = AIActionType::UseAbility;
        decision.target = ally;
        decision.abilityIndex = 0;
        decision.effectToApply =
            StatusEffectInstance{StatusEffectType::Empowered, buffDuration_, buffMagnitude_};
        return decision;
    }

    return AIDecision{}; // nobody nearby needs buffing right now
}

} // namespace engine
