#pragma once

#include <optional>
#include <string>

#include "entities/AttributeFormulas.hpp"
#include "entities/StatusEffects.hpp"

namespace engine {

enum class TalentTree {
    Blade,
    Flame, // legacy enemy/reserved definitions
    OneHanded, TwoHanded, Shield, Bow, Stealth, Acrobatics, Fire, Ice, Lightning, Arcane, Cloth, LightArmour, HeavyArmour, Spellblade, Animation, BloodMagic, ShadowArcher,
    Brawling, Whip, Shadow, Radiance, Alchemy,
    Spear, Daggers, Mace, Crossbow,
};

// Abilities and talents have five ranks; rank 5 often adds a mastery effect
// (TalentDefinition::mastery).
inline constexpr int kMaxTalentRank = 5;

enum class TargetingMode {
    Self,               // affects the caster only (Blink, Immolate's origin)
    AdjacentEnemy,      // must target an enemy in one of the 4 adjacent tiles
    RangedEnemyInSight, // any enemy within the caster's current field of view
};

enum class EffectShape {
    SingleTarget,      // affects only the resolved target
    AreaAroundTarget,  // affects the resolved target and anything within areaRadius of it
    AreaAroundSelf,    // affects anything within areaRadius of the caster
    Movement,          // relocates the caster; power/areaRadius unused
};

// What a talent's effect actually does to whatever it resolves as its
// target(s) -- introduced at Prompt 15 once a third kind (SelfBuff, for
// the Fighter's Rallying Cry, that class originally named "Marauder"
// and renamed at Prompt 19) made a lone `bool isHeal` (Prompt 14's
// healing addition) worth generalizing rather than bolting on a second
// flag. Mirrors AIDecision's own discriminated-by-enum shape
// (AIActionType), the established pattern for "one struct represents
// different kinds of things."
enum class TalentEffectKind {
    Damage,   // the common case -- TalentEffects::applyTalentDamage
    Heal,     // TalentEffects::applyTalentHeal
    SelfBuff, // TalentEffects::applyTalentSelfBuff, using selfBuffEffect below
};

// A talent's full definition -- deliberately POD-like data, no behavior
// of its own. The generic logic that interprets these fields lives in
// TalentEffects (application) and Application (targeting resolution),
// not here. See ARCHITECTURE_DECISIONS.md for why this counts as
// "data-driven" even without an external file: logic and data are
// cleanly separated, the data table is trivially swappable for a file
// loader later, but no such loader exists yet.
enum class WeaponRequirement { None, OneHanded, TwoHanded, Shield, Bow, Melee, Whip, Spear, Dagger, Mace, Crossbow };
enum class ArmourRequirement { None, Cloth, Light, Heavy };
inline bool isMagicTree(TalentTree tree) {
    return (tree >= TalentTree::Fire && tree <= TalentTree::Arcane) || tree == TalentTree::Shadow || tree == TalentTree::Radiance;
}

enum class PassiveKind { None, Riposte, Bloodlust, ShieldTraining, Marksmanship, Ambush, Footwork, Kindle, StaticCharge, Frostbite, ArcaneEfficiency, ClothWard, Spellweave, LightEvasion, LightPrecision, HeavyBrace, HeavyResolve, BattleRhythm, GravePact, Deathless, Unseen,
    // Ascendancy passives (entities/Ascendancy.hpp).
    Rampage, LastStand, IronSkin, CrushingBlows, Conduit, LingeringElements, Overload, Attunement,
    Opportunist, Slippery, QuickHands, KillerInstinct,
    // Hybrid ascendancies.
    Zeal, Retribution, Devotion, Righteous, HiddenCasting, LingeringShadow, ShadeStep, SpellThief,
    Momentum, Finisher, Counter, EnGarde, Balance, Versatility, Resilience, Wellspring,
    // Brawling, Whip, Shadow, Radiance, Alchemy.
    HardLanding, Flay, Umbral, InnerLight, PotentBrews,
    // Spear, Daggers, Mace, Crossbow.
    LongReach, Hemorrhage, Bonebreaker, Windlass };

struct Talent {
    std::string name;
    std::string description;
    TalentTree tree = TalentTree::Blade;
    TargetingMode targeting = TargetingMode::AdjacentEnemy;
    EffectShape shape = EffectShape::SingleTarget;

    int manaCost = 0;
    int hpCost = 0;       // Reckless Lunge: costs the caster's own hp too -- risk/reward
    int cooldownTurns = 0;

    int power = 0;        // base damage/heal amount; unused for Movement and SelfBuff
    int areaRadius = 0;   // for AreaAroundTarget / AreaAroundSelf
    int moveDistance = 0; // for Movement (Blink)

    // Execution-style conditional bonus: if the target's hp fraction is
    // at or below this threshold, `power` is multiplied by
    // conditionalMultiplier instead of used as-is. 0 threshold means no
    // conditional effect (the common case).
    float conditionalHpFraction = 0.f;
    int conditionalMultiplier = 1;

    // Which attribute this talent's damage/heal scales with -- fully
    // rewritten from the original Prompt 14 version (which only chose
    // between Physical/Strength and Magic/Intelligence) into the new
    // three-way ScalingStat, now that Dexterity can power a talent's
    // damage too. Every talent scales from exactly one attribute; there
    // is no "scales from two stats" or "universal bonus" concept.
    // Unused for Movement and SelfBuff. Deliberately the LAST-but-two
    // field, not inserted earlier alongside power/areaRadius where it
    // would read more naturally: every talent in
    // SpellbladeTalents.cpp/WarriorTalents.cpp/ThiefTalents.cpp/
    // MageTalents.cpp is constructed with positional (not
    // designated) aggregate initialization, so inserting a field
    // anywhere but the end would silently shift every value after it in
    // every existing construction that lists that many fields -- worst
    // case, an int meant for conditionalMultiplier landing in
    // conditionalHpFraction instead, which compiles cleanly (int ->
    // float is an implicit, silent conversion) and would have been a
    // very easy bug to ship unnoticed. effectKind, selfBuffEffect,
    // retreatDistance, and onHitEffect/onHitChance, added after it
    // across Prompts 15-19, all follow the same rule.
    ScalingStat scalingStat = ScalingStat::Strength;

    // Damage (the default), Heal, or SelfBuff -- see TalentEffectKind's
    // own comment.
    TalentEffectKind effectKind = TalentEffectKind::Damage;

    // For SelfBuff only: the status effect applied directly to the
    // caster (e.g. the Fighter's Rallying Cry applying Empowered to
    // themselves, the same status effect the boss's enrage already
    // uses). Unset for every other effectKind.
    std::optional<StatusEffectInstance> selfBuffEffect;

    // Prompt 16 (originally "Archer's" Vault Kick, that class renamed
    // to Thief at Prompt 19): for a Damage-kind, AdjacentEnemy-targeted
    // talent, moves the caster this many tiles directly away
    // from the target after the damage step -- a knockback on the
    // caster's own position, not the target's. 0 (the default) means no
    // retreat, every existing talent's ordinary behavior. Happens
    // whether or not the damage itself was dodged: the retreat is the
    // caster's own follow-through motion, not an on-hit effect riding
    // on a successful strike the way Poison or Stun are. Reuses
    // resolveBlinkDestination() for the actual movement -- "walk N
    // tiles in a direction, stopping early at a wall or another actor"
    // is exactly what Blink already does, just computed away from the
    // target instead of in the caster's last-move direction.
    int retreatDistance = 0;

    // Prompt 19 (Sorcerer's Mind Shatter): for a Damage-kind talent,
    // optionally applies this status effect to the *target* on a
    // successful (non-dodged) hit, with probability onHitChance --
    // mirrors MonsterAttackProfile's onHitEffect/onHitChance exactly
    // (Poison, Stun, and so on have applied to the player from monsters
    // this way since Prompt 10), just now available to a player talent
    // for the first time. Unset/1.f are the defaults, meaning "no
    // extra effect," every existing talent's ordinary behavior.
    std::optional<StatusEffectInstance> onHitEffect;
    float onHitChance = 1.f;

    // Per-talent crit modifiers, on top of the global crit system
    // (AttributeFormulas::rollCrit/critDamageMultiplier) -- Thief's
    // Piercing Shot is the one talent that uses these: an inherent
    // +20% crit chance and +50% increased crit damage (so a Piercing
    // Shot crit deals 2.0x, not the normal 1.5x), replacing what was
    // originally a conditional triple-damage-on-low-hp mechanic. 0 (the
    // default) means no bonus, every other talent's ordinary behavior.
    float bonusCritChance = 0.f;
    float bonusCritDamageMultiplier = 0.f;

    // Aimed ray, stopped by terrain or the first enemy. Direct spells retain
    // their visible-target rule. Kept last for aggregate initializer safety.
    bool projectile = false;
    std::string id; // persistent identity, independent of display name and learned order
    unsigned int tags = 0;
    int scalingCooldown = -1; // effective talents preserve the base damage-scaling tier
    int damagePercent = 100;
    bool chain = false;
    bool passive = false;
    PassiveKind passiveKind = PassiveKind::None;
    int passiveMagnitude = 0;
    WeaponRequirement weaponRequirement = WeaponRequirement::None;
    int pushDistance = 0;
    bool consumeShock = false, consumeChill = false, consumeBurn = false;
    int statusBonusPercent = 0;
    bool requiresStealth = false;
    int movementBurn = 0;
    bool cleanse = false;
    int committedBloodlust = -1; // frozen at cast commitment so HP costs cannot change the preview

    ArmourRequirement armourRequirement = ArmourRequirement::None;
    int restoreMana = 0;
    int restoreHpPercent = 0;
    int imbueElement=0; // 1 fire, 2 ice, 3 lightning, 4 arcane
    bool spellstrike=false, releaseAilments=false, boneSwap=false;
    int summonCount=0, summonDuration=0, summonRank=1;
    int drainPercent=0, stayHiddenPercent=0;
    bool huntersMark=false, returnConcealed=false;
    int committedRhythm=-1;
    bool conjureLight=false; // leaves a wisp of light where it is cast
    // Brawling. Charge: an AdjacentEnemy attack that may instead target an
    // enemy up to this many tiles further along a straight, clear line, running
    // up beside it first. Hurl: throw the target over your shoulder, landing up
    // to this many tiles behind you. Domino: a hurled creature knocks whatever
    // it hits one tile further.
    int chargeDistance=0, hurlDistance=0;
    bool domino=false;
    // Whip: reach lets an AdjacentEnemy attack strike along a clear straight
    // line up to this many tiles; pullDistance then drags the target toward
    // you (stopping beside you), through whatever lies between.
    int reach=0, pullDistance=0;
    // Shadow and Radiance. darkBonusPercent: extra damage against a target on
    // an unlit tile. searing: +50% against undead and creatures that see in
    // the dark. snuffRadius: put out every light around you. flare: reveal
    // hidden enemies and light the blast. dawn: relight the lights around you.
    int darkBonusPercent=0, snuffRadius=0;
    bool searing=false, flare=false, dawn=false;
    // Alchemy: the surface (a SurfaceType value) a flask leaves on the
    // tiles it splashes, and for how many turns (0: until disturbed).
    int splashSurface=0, splashTurns=0;
    // Weapons. vault: a movement that leaps over creatures and chasms (walls
    // still stop it). pierceBehind: also strikes whoever stands right behind
    // the target; pierceAll: a projectile passes through every enemy in line.
    // backstab: +100% against a foe that can't fight back properly (you are
    // hidden, or it is blinded, stunned, held or pinned). stagger: delays the
    // target's warned attack by this many actions. shatterIce: breaks the ice
    // around the target into shards.
    bool vault=false, pierceBehind=false, pierceAll=false, backstab=false, shatterIce=false;
    int stagger=0;

};

enum TalentTag : unsigned int { MeleeTag = 1, ProjectileTag = 2, AreaTag = 4,
    MovementTag = 8, DamagingTag = 16, PureMovementTag = 32 };
inline bool isSpell(const Talent& t) {
    return isMagicTree(t.tree) || t.spellstrike || t.tree==TalentTree::Animation || t.tree==TalentTree::BloodMagic;
}
inline bool isMeleeAttack(const Talent& t) {
    return t.effectKind==TalentEffectKind::Damage && (t.targeting==TargetingMode::AdjacentEnemy ||
        (t.shape==EffectShape::AreaAroundSelf && !isSpell(t) && t.scalingStat==ScalingStat::Strength));
}
inline unsigned int talentTags(const Talent& talent) {
    unsigned int tags = 0;
    if (talent.targeting == TargetingMode::AdjacentEnemy) tags |= MeleeTag;
    if (talent.projectile) tags |= ProjectileTag;
    if (talent.shape == EffectShape::AreaAroundSelf || talent.shape == EffectShape::AreaAroundTarget) tags |= AreaTag;
    if (talent.shape == EffectShape::Movement) tags |= MovementTag | PureMovementTag;
    if (talent.retreatDistance > 0 || talent.chargeDistance > 0) tags |= MovementTag;
    if (talent.effectKind == TalentEffectKind::Damage && talent.shape != EffectShape::Movement) tags |= DamagingTag;
    return tags;
}

} // namespace engine
