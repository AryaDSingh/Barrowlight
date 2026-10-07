#pragma once

#include <vector>

namespace engine {

// The Vampire Lord's curse: how long it lasts, and how far you see in the dark.
inline constexpr int kVampirismTurns = 200, kVampireSight = 6;
enum class StatusEffectType {
    Poison,    // damage per turn
    Stun,      // skip the affected actor's next action
    Empowered, // bonus flat damage added to the actor's own attacks
    Burn, Chill, Shock, Guard, Evasion, Concealed, Opening, Marked, StunRecovery, FlameBlade, FrostBlade, StormBlade, ArcaneBlade, BattleRhythm, BloodPact, Wither, HuntersMark, UnseenReady,
    ManaDrain, Doom, // curses: drain mana each tick; delayed HP damage on expiry
    Smothered,       // the Lich's darkness: your light can't burn until it ends (not cleansable)
    Grappled,        // held by the player (Brawling): can't walk away, and is dragged along
    Blinded,         // sees only what is beside it (Shadow, Radiance)
    Bleed,           // damage per turn, leaving a blood trail (Daggers)
    Sundered,        // takes +magnitude damage from every hit (Mace)
    Pinned,          // can't move (Crossbow)
    Braced,          // the player strikes whatever steps up to them (Spear)
    Misfortune,      // its attacks miss magnitude% more often (Hexes)
    Linked,          // magnitude% of the damage it takes jumps to a nearby foe (Hexes)
    Puppeted,        // fights for the player until it ends (Hexes)
    Plague,          // poison that spreads to its neighbours when it dies (Venom)
    Frenzy,          // your hits heal you for magnitude% of the damage they deal (Two-Handed)
    Hasted,          // acts magnitude% more often
    Slowed,          // acts magnitude% less often (Chill slows the same way)
    Shaken,          // deals magnitude% less damage (Warbanner's Rally Cry)
    Steadfast,       // can't be moved or stunned; direct hits deal magnitude less (Hold the Line)
    Heat,            // builds near furnaces and fire; at 10 or more it burns (the Ashen Foundry)
    MoltenPlate,     // melee attackers take magnitude fire damage; each blow heats you (Forgeborn)
    Forgeheart,      // Heat can't fall; it vents when this ends (Forgeborn)
    Anvil,           // can't be moved; takes magnitude% less damage; heats 2 a turn (Forgeknight)
    Stormcall,       // a storm over you strikes the nearest foe each turn (Tempest)
    BoneStorm,       // bone shards whirl around you, cutting the foes beside you (Bonewright)
    BoneLord,        // your minions are hastened and hit harder (Bonewright)
    Encased,         // sealed in ice: can't act, can't be hurt; bursts when it ends (Rimeheart)
    Thornguard,      // standing in your thorns: armour (Briarheart)
    Overgrowth,      // your thorns spread each turn (Briarheart)
    BriarHeart,      // whatever hits you is caught in thorns (Briarheart)
    AlphasHowl,      // your beasts are hastened and their bites bleed (Packmaster)
    FeralBond,       // half of each hit on you goes to your nearest beast (Packmaster)
    CallOfTheWild,   // you and Thornmaw are hastened; each kill heals you both (Beastwarden)
    Vampirism,       // the Vampire Lord's curse: night eyes and a thirst, but light burns (save format 45)
    RimePlate,       // foes that strike you in melee are chilled (Wintermarch)
    Hoarfrost,       // the foes around you are chilled each turn; magnitude is the reach (Wintermarch)
    WinterMarch,     // foes within 3 tiles crawl; hits on you deal less (Wintermarch)
    Lichfrost,       // the foes that die near you rise for you (Gravecold)
    GlacialGuard,    // standing on ice: direct hits deal less (Wintercaller)
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
    bool canReceiveStun() const { return !has(StatusEffectType::Stun) && !has(StatusEffectType::StunRecovery) && !has(StatusEffectType::Steadfast) && !has(StatusEffectType::Encased); }
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
    return type==StatusEffectType::Poison || type==StatusEffectType::Burn || type==StatusEffectType::Chill || type==StatusEffectType::Marked ||
           type==StatusEffectType::Bleed || type==StatusEffectType::Sundered || type==StatusEffectType::Plague || isCurse(type);
}
inline constexpr int kMarkedDamagePercent=25;
// How fast a creature acts, as a percentage of its base speed: Hasted speeds
// it up, Slowed and Chill slow it down. Never below a quarter.
inline int speedPercent(const StatusEffects& effects) {
    const int percent=100+effects.magnitudeOf(StatusEffectType::Hasted)-effects.magnitudeOf(StatusEffectType::Slowed)
                     -effects.magnitudeOf(StatusEffectType::Chill);
    return percent<25?25:percent;
}
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
    case StatusEffectType::Bleed: return "Bleed";
    case StatusEffectType::Sundered: return "Sundered";
    case StatusEffectType::Pinned: return "Pinned";
    case StatusEffectType::Braced: return "Braced";
    case StatusEffectType::Misfortune: return "Misfortune";
    case StatusEffectType::Linked: return "Soul Link";
    case StatusEffectType::Puppeted: return "Puppet";
    case StatusEffectType::Plague: return "Plague";
    case StatusEffectType::Frenzy: return "Frenzy";
    case StatusEffectType::Hasted: return "Hasted";
    case StatusEffectType::Slowed: return "Slowed";
    case StatusEffectType::Shaken: return "Shaken";
    case StatusEffectType::Steadfast: return "Steadfast";
    case StatusEffectType::Heat: return "Heat";
    case StatusEffectType::MoltenPlate: return "Molten Plate";
    case StatusEffectType::Forgeheart: return "Forgeheart";
    case StatusEffectType::Anvil: return "Anvil Stance";
    case StatusEffectType::Stormcall: return "Stormcall";
    case StatusEffectType::BoneStorm: return "Bone Storm";
    case StatusEffectType::BoneLord: return "Bone Lord";
    case StatusEffectType::Encased: return "Encased";
    case StatusEffectType::Thornguard: return "Thornguard";
    case StatusEffectType::Overgrowth: return "Overgrowth";
    case StatusEffectType::BriarHeart: return "Heart of Briars";
    case StatusEffectType::AlphasHowl: return "Alpha's Howl";
    case StatusEffectType::FeralBond: return "Feral Bond";
    case StatusEffectType::CallOfTheWild: return "Call of the Wild";
    case StatusEffectType::Vampirism: return "Vampirism";
    case StatusEffectType::RimePlate: return "Rime Plate";
    case StatusEffectType::Hoarfrost: return "Hoarfrost";
    case StatusEffectType::WinterMarch: return "Winter's March";
    case StatusEffectType::Lichfrost: return "Lichfrost";
    case StatusEffectType::GlacialGuard: return "Glacial Armour";
    case StatusEffectType::UnseenReady: return "Unseen ready";
    case StatusEffectType::StunRecovery: return "Stun recovery";
    }
    return "Unknown";
}
} // namespace engine
