#pragma once

#include <vector>

namespace engine {

enum class StatusEffectType {
    Poison,    // damage per turn
    Stun,      // skip the affected actor's next action
    Empowered, // bonus flat damage added to the actor's own attacks
    Burn, Chill, Shock, Guard, Evasion, Concealed, Opening, Marked, StunRecovery, FlameBlade, FrostBlade, StormBlade, ArcaneBlade, BattleRhythm, BloodPact, Wither, HuntersMark, UnseenReady,
    ManaDrain, Doom, // curses: drain mana each tick; delayed HP damage on expiry
    Smothered,       // the Lich's darkness: your light can't burn until it ends (not cleansable)
    Grappled,        // held by the player (Brawling): can't walk away, and is dragged along
    Blinded,         // sees only what is beside it (Shadow, Radiance)
};

struct StatusEffectInstance {
    StatusEffectType type;
    int turnsRemaining;
    int magnitude = 0; // Poison: damage/turn. Empowered: bonus dmg/hit. Stun: unused. Concealed: source ability rank (1-3). Marked: remaining charge (1).
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
    void remove(StatusEffectType type);
    bool canReceiveStun() const { return !has(StatusEffectType::Stun) && !has(StatusEffectType::StunRecovery); }
    void setStunRules(int maximumDuration, int recoveryTurns) { maxStunDuration_=maximumDuration; stunRecoveryTurns_=recoveryTurns; }
    int stunRecoveryTurns() const { return stunRecoveryTurns_; }
    int maxStunDuration() const { return maxStunDuration_; }
    void consumeMark();
    int magnitudeOf(StatusEffectType type) const; // 0 if not present

    std::vector<StatusEffectInstance>& active() { return active_; }
    const std::vector<StatusEffectInstance>& active() const { return active_; }

private:
    std::vector<StatusEffectInstance> active_;
    int maxStunDuration_=3, stunRecoveryTurns_=1;
};

inline bool isCurse(StatusEffectType type) {
    return type==StatusEffectType::ManaDrain || type==StatusEffectType::Doom;
}
inline bool isCleansable(StatusEffectType type) {
    return type==StatusEffectType::Poison || type==StatusEffectType::Burn || type==StatusEffectType::Chill || type==StatusEffectType::Marked || isCurse(type);
}
inline constexpr int kMarkedDamagePercent=25;
inline const char* statusName(StatusEffectType type) {
    switch (type) {
    case StatusEffectType::Poison: return "Poison";
    case StatusEffectType::Stun: return "Stun";
    case StatusEffectType::Empowered: return "Empowered";
    case StatusEffectType::Burn: return "Burn";
    case StatusEffectType::Chill: return "Chill";
    case StatusEffectType::Shock: return "Shock";
    case StatusEffectType::Guard: return "Guard";
    case StatusEffectType::Evasion: return "Evasion";
    case StatusEffectType::Concealed: return "Concealed";
    case StatusEffectType::Opening: return "Opening";
    case StatusEffectType::Marked: return "Marked";
    case StatusEffectType::FlameBlade: return "Flame Blade";
    case StatusEffectType::FrostBlade: return "Frost Blade";
    case StatusEffectType::StormBlade: return "Storm Blade";
    case StatusEffectType::ArcaneBlade: return "Arcane Blade";
    case StatusEffectType::BattleRhythm: return "Battle Rhythm";
    case StatusEffectType::BloodPact: return "Blood Pact";
    case StatusEffectType::Wither: return "Wither";
    case StatusEffectType::HuntersMark: return "Hunter's Mark";
    case StatusEffectType::ManaDrain: return "Mana Drain";
    case StatusEffectType::Doom: return "Doom";
    case StatusEffectType::Smothered: return "Smothered";
    case StatusEffectType::Grappled: return "Grappled";
    case StatusEffectType::Blinded: return "Blinded";
    case StatusEffectType::UnseenReady: return "Unseen ready";
    case StatusEffectType::StunRecovery: return "Stun recovery";
    }
    return "Unknown";
}
} // namespace engine
