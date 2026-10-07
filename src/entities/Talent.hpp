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
    Earth, Tide, Hexes, Venom,
    Traps, Skirmish, Lamplighter, Stormlance, Hexblade, Saboteur, Stonefist,
    Warbanner, // the first deep tree
    Forgeborn, // the Ashen Foundry's deep tree
    Slagcaller, // the Ashen Foundry's second deep tree
    Tempest,    // the Drowned Cathedral's deep tree
    Bonewright, // the Deep Crypts' deep tree
    Rimeheart,  // the Deep Crypts' second deep tree
    Briarheart, // Thornwood Hollow's deep tree
    Packmaster, // Thornwood Hollow's second deep tree
    Wintermarch, // Rimeholt's deep tree
    Gravecold,   // Rimeholt's second deep tree
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
    return (tree >= TalentTree::Fire && tree <= TalentTree::Arcane) || tree == TalentTree::Shadow || tree == TalentTree::Radiance ||
           tree == TalentTree::Earth || tree == TalentTree::Tide || tree == TalentTree::Hexes || tree == TalentTree::Venom ||
           tree == TalentTree::Lamplighter;
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
    LongReach, Hemorrhage, Bonebreaker, Windlass,
    // Earth, Tide, Hexes, Venom.
    Stoneskin, Riptide, Malediction, ToxicRuin,
    // Traps, Skirmish and the second hybrids.
    Trapper, Lunge, PassStrike, RunningStart, LanternWard, StaticEdge, LingeringHex, DirtyTricks, GraniteFists,
    // The forked pilot trees (Fire, One-Handed, Arcane).
    Kindling, Wildfire, Exploit, Afterimage,
    // Resonances.
    SearingEdge,
    // The second forked batch (Ice, Lightning, Two-Handed) and its resonances.
    Hoarfrost, ArcFlash, FollowThrough, ThermalShock, ColdSteel,
    // The third batch (Shadow, Radiance, Shield) and its resonances.
    Dread, Bulwark, Twilight, TemplarsEdge, HallowedGuard,
    // The fourth batch (Bow, Stealth, Daggers) and its resonances.
    CutDeep, FireArrows, SoftSteps, HoldTheLine, BreakRanks, Tempered, HeatSink, HeatEngine, BurningPlate, Overheat, BrittleSlag, Pyroclasm, Overcharge, EyeOfTheStorm, Marrow, GrownGuard, BrittleCold, CreepingFrost, Thornborn, BriarSnares, PackTactics, Blooded, Thornmaw, PackOfTwo, RunningMate, GuardianInstinct, Contagion, Outbreak, Miasma, Wasting, Permafrost, BitterCold, GraveChill, Unrotting, UnseenHand, AssassinsEdge,
    // The fifth batch (Earth, Tide, Venom) and its resonances.
    Aftershock, Tidecaller, Virulence, Mire, FoulWater, EnvenomedBlades,
    // The sixth batch (Spear, Mace, Crossbow) and its resonances.
    Skewer, HeavyDraw, Bonecrusher, StormBolts, Bedrock,
    // The seventh batch (Brawling, Whip, Skirmish) and its resonances.
    Knockout, Taskmaster, LightningFeet, Tremor,
    // The eighth batch (Alchemy, Traps, Hexes) and its resonances.
    VolatileMix, Ambusher, HexEcho, Incendiary, WastingCurse, Witchfire,
    // The ninth batch (Acrobatics, the armour trees) and its resonances.
    Fleet, FlowingMana, ArcaneBulwark, Unstoppable,
    // The first hybrid batch (Spellblade, Animation, Blood Magic) and its resonances.
    BladeWard, BoneArmour, Transfusion, GraveLight, BoilingBlood, Bloodletter,
    // The second hybrid batch (Shadow Archer, Lamplighter, Stormlance) and its resonances.
    LongShadow, LanternBearer, Thunderflash, Ionise, GhostArrows,
    // The last hybrid batch (Hexblade, Saboteur, Stonefist) and its resonances.
    HungeringBlade, TrapSense, SoulHarvest, Magma, Bloodhound };

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
    // Slagcaller. summonKind: what is raised (a MonsterType; -1 skeletons).
    // slagPool: the ground it hits burns and slows. eruption: the ground
    // around its target burns, and the target is stunned.
    int summonKind=-1;
    bool slagPool=false, eruption=false;
    int stormcall=0, staticField=0; // Tempest: turns of the storm over you; radius of electrified ground around you
    int boneWall=0, boneStorm=0, boneLord=0; // Bonewright: walls raised; turns of the bone storm; turns your minions are lorded
    // Rimeheart. rime: freeze the ground it hits. onIceDouble: double damage to
    // a foe on ice. iceTrail: a movement that leaves ice behind. deepFreeze:
    // radius frozen around you. wintersHeart: turns encased in ice.
    bool rime=false, onIceDouble=false, iceTrail=false;
    int deepFreeze=0, wintersHeart=0;
    // Briarheart. briarSeed: thorns grow where it lands. onThornsDouble: double
    // damage to a foe standing in thorns, pinning it for lashPin turns.
    // bloodBriar: thorns burst around you, pinning for this many turns.
    // overgrowth: turns your thorns spread. heartOfBriars: turns your attackers
    // are caught in thorns (heartHeals: and you heal from their bleeding).
    bool briarSeed=false, onThornsDouble=false, heartHeals=false;
    int lashPin=0, bloodBriar=0, overgrowth=0, heartOfBriars=0;
    // Packmaster. callPack: how many hounds you keep. sicEm: turns your beasts
    // are hastened hunting the foe you mark (sicPin: their first bite pins).
    // bloodBond: life each beast heals. alphasHowl, feralBond: their turns
    // (feralHeals: your beasts' bites heal you).
    bool sicPin=false, feralHeals=false;
    int callPack=0, sicEm=0, bloodBond=0, alphasHowl=0, feralBond=0;
    // Beastwarden. pointLeap: Thornmaw leaps to the foe you mark and its next
    // bite doubles. callWild: turns you and Thornmaw run wild.
    bool pointLeap=false;
    int callWild=0;
    // Plaguebringer. patientZero: the plague it lays grows with Intelligence.
    // pandemic: every plagued foe in sight has its plague doubled and lengthened.
    bool patientZero=false, pandemic=false;
    // Wintermarch. rimePlate: turns your plate chills those who strike you.
    // frozenAdvance: a step that slows the foes around where you land.
    // hoarfrost: turns the cold chills the foes around you (hoarReach tiles).
    // winterMarch: turns everything near you slows to a crawl.
    bool frozenAdvance=false;
    int rimePlate=0, hoarfrost=0, hoarReach=1, winterMarch=0;
    // Gravecold. raiseFrozen: how many thralls you keep. coldGrasp: turns a foe
    // it kills fights for you, risen. shatterPercent: your thralls burst (their
    // blast, in percent). wintersHost: turns three wights rise for. lichfrost:
    // turns the dead near you rise.
    int raiseFrozen=0, coldGrasp=0, shatterPercent=0, wintersHost=0, lichfrost=0;
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
    // Magic. raisePillar: a stone pillar rises on the target tile for a while.
    // splashPath: the splash surface covers the whole path, not just the
    // impact. vortex: everything hit is dragged a tile toward the centre.
    bool raisePillar=false, splashPath=false, vortex=false;
    // placeTrap: a trap kind (Application::Trap) set on the target tile(s).
    // needsLight: only while your torch or lantern burns. hurlTorch: your
    // light goes with it. bonfire: sets the ground around you alight.
    // landingBurst: lightning of this power around where a movement lands.
    // curseBonus: +100% against a cursed foe. pillarSlam: raise a pillar
    // behind the target and drive it in. smokeBomb: blind those near you.
    // blitz: a movement that strikes everything beside its path.
    int placeTrap=0, landingBurst=0;
    bool needsLight=false, hurlTorch=false, bonfire=false, curseBonus=false, pillarSlam=false, smokeBomb=false, blitz=false;
    // Warbanner. plantBanner: plant your standard for that many turns (a
    // great standard when greatBanner). rallyCry: shake the foes around you
    // (breakWindups: and cancel what they were winding up). crashStun: a foe
    // knocked into another stuns both, the crash dealing crashStun times its
    // damage. knockAside: a blitz also throws aside what it strikes.
    int plantBanner=0, crashStun=0;
    bool greatBanner=false, rallyCry=false, breakWindups=false, knockAside=false;
    // Forgeborn. gainHeat: Heat you gain. spendHeat: damage per Heat spent
    // (all of it; the hit also burns). ventHeat: an area hit that spends all
    // your Heat for that much damage per point and sets the ground burning.
    // moltenPlate / forgeheart: turns of each.
    int gainHeat=0, spendHeat=0, ventHeat=0, moltenPlate=0, forgeheart=0;
    int quench=0, anvil=0; // Forgeknight: life per Heat quenched; turns of Anvil Stance
    // The forked pilot trees. guardPerHit: Guard for each foe struck (Blade
    // Dance). markOnHit: also marks what it hits. stunOnImpact: a shoved foe
    // that slams into something is stunned. scatterSplash: the splash lands
    // in patches, not everywhere. echoBeam: the beam fires again down the same
    // line at the start of your next turn.
    int guardPerHit=0;
    bool markOnHit=false, stunOnImpact=false, scatterSplash=false, echoBeam=false;
    // lingerTurns: the area keeps striking as your next turns begin (Blizzard).
    // chainJumps: how many foes a chain leaps to. landingSlam: a movement that
    // strikes everything beside where it lands, for this power; slamStun: and
    // stuns it.
    int lingerTurns=0, chainJumps=1, landingSlam=0;
    bool slamStun=false;
    // blindInDark: a target standing in darkness is blinded too. darkLanding:
    // a movement that may only end on an unlit tile; arrivalBlind: and blinds
    // the foe nearest where it lands. rootSelf: you can't walk while its
    // effect lasts (Bastion).
    bool blindInDark=false, darkLanding=false, arrivalBlind=false, rootSelf=false;
    // shakeOff: foes within five tiles lose track of you (Feign Death).
    // consumeBleed: tears out the target's whole bleed at once, double.
    bool shakeOff=false, consumeBleed=false;
    // pillarShove: a raised pillar shoves the foes beside it. festerPoison: a
    // poisoned target's poison doubles and lasts longer; festerSpread: and
    // spreads to the foes beside it.
    bool pillarShove=false, festerPoison=false, festerSpread=false;
    // grantOpening: you also gain Opening for a few turns (Slipstream).
    bool grantOpening=false;
    int hasteSelf=0; // you are Hasted by this much for a few turns (Slipstream)
    // A passive's flat number grows by 1 for every scalePer points of its
    // attribute (scalingStat, or your highest attribute when scaleHighest),
    // so it keeps its worth as you level. 0: it doesn't (percentages).
    int scalePer=0;
    bool scaleHighest=false;
    // wardPercent: gain spell ward worth this % of your maximum mana (Arcane
    // Shroud). manaBurst: spend all your mana as the blast's power.
    // slowSelf: you are Slowed by this much while it lasts (Fortify).
    // critWithOpening: always a critical hit while you have Opening.
    // shakeHolds: throws off pins, holds and slows (Unbreakable).
    int wardPercent=0, slowSelf=0;
    bool manaBurst=false, critWithOpening=false, shakeHolds=false;
    // boneprison: bone walls rise on every free tile around the target; prisonCut: and cut it.
    bool bonePrison=false, prisonCut=false;
    // A second status laid on what it hits (Shadow Pin), and a hit that can't be dodged (Ghost Arrows).
    std::optional<StatusEffectInstance> secondHitEffect;
    bool undodgeable=false;
    // consumeCurses: tears every curse off the target, +50% damage for each (Soul Reap).
    // delayedBlast: lands now, strikes as your next turn begins (Demolition).
    bool consumeCurses=false, delayedBlast=false;

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
