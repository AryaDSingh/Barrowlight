#pragma once

#include <vector>

namespace engine {

enum class StatusEffectType {
    Poison,    // damage per turn
    Stun,      // skip the affected actor's next action
    Empowered, // bonus flat damage added to the actor's own attacks
};

struct StatusEffectInstance {
    StatusEffectType type;
    int turnsRemaining;
    int magnitude = 0; // Poison: damage/turn. Empowered: bonus dmg/hit. Stun: unused.
};

// Real bookkeeping now -- this is the wholesale replacement flagged back
// in Prompt 3 ("expect this to be replaced wholesale, not incrementally
// extended"). Tracks which effects are currently active on an Actor.
//
// Deliberately doesn't know how to *apply* an effect's per-turn behavior
// (poison damage, stun-skipping a turn) -- that's tickStatusEffects'
// job (see StatusEffectLogic.hpp), mirroring how TalentSet stays
// separate from TalentEffects.
class StatusEffects {
public:
    // Adds `effect`, replacing any existing instance of the same type
    // (refreshing duration/magnitude) rather than stacking multiple
    // instances of one type.
    void apply(StatusEffectInstance effect);

    bool has(StatusEffectType type) const;
    int magnitudeOf(StatusEffectType type) const; // 0 if not present

    std::vector<StatusEffectInstance>& active() { return active_; }
    const std::vector<StatusEffectInstance>& active() const { return active_; }

private:
    std::vector<StatusEffectInstance> active_;
};

} // namespace engine
