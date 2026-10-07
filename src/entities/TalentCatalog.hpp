#pragma once

#include <array>
#include <cstdint>
#include <algorithm>
#include <functional>
#include <stdexcept>
#include <map>
#include <vector>
#include "entities/Talent.hpp"
#include "entities/MonsterType.hpp"
#include "entities/PlayerClass.hpp"

namespace engine {
struct TreeDefinition {
    const char* id;
    const char* name;
    TalentTree tree;
    const char* description;
    const char* starterItem;
};
inline constexpr std::array<TreeDefinition, 44> kTalentTrees{{
    {"one_handed", "One-Handed", TalentTree::OneHanded, "Efficient strikes, defensive openings and finishing blows.", "iron_sword"},
    {"two_handed", "Two-Handed", TalentTree::TwoHanded, "Heavy swings, surrounding enemies and blood-fuelled attacks.", "greatsword"},
    {"shield", "Shield", TalentTree::Shield, "Block damage, interrupt attacks and control space.", "wooden_shield"},
    {"bow", "Bow", TalentTree::Bow, "Ranged pressure, clustered targets and critical shots.", "hunting_bow"},
    {"stealth", "Stealth", TalentTree::Stealth, "Hide, slip away and go unnoticed. Rank, DEX and distance oppose enemy detection rolls.", ""},
    {"acrobatics", "Acrobatics", TalentTree::Acrobatics, "Reposition, disengage and evade after movement abilities.", ""},
    {"fire", "Fire", TalentTree::Fire, "Burn enemies, spread fire and ignite movement abilities.", ""},
    {"ice", "Ice", TalentTree::Ice, "Chill slows your foes; shatter chilled enemies.", ""},
    {"lightning", "Lightning", TalentTree::Lightning, "Burst, chaining and consuming Shock for a decisive hit.", ""},
    {"arcane", "Arcane", TalentTree::Arcane, "Flexible force, Blink and efficient spellcasting.", ""},
    {"cloth", "Cloth / Unarmoured", TalentTree::Cloth, "Requires cloth or no armour. Recover mana, evade and exploit elemental ailments.", ""},
    {"light_armour", "Light Armour", TalentTree::LightArmour, "Requires light armour. Reposition for Opening, dodge and critical hits.", ""},
    {"heavy_armour", "Heavy Armour", TalentTree::HeavyArmour, "Requires heavy armour. Wait to brace, withstand stuns and recover in battle.", ""},
    {"spellblade", "Spellblade", TalentTree::Spellblade, "Imbue melee weapons and weave spells into strikes.", ""},
    {"animation", "Animation", TalentTree::Animation, "Raise undead allies and command the battlefield.", ""},
    {"blood_magic", "Blood Magic", TalentTree::BloodMagic, "Spend life to cast, drain enemies and defy death.", ""},
    {"shadow_archer", "Shadow Archer", TalentTree::ShadowArcher, "Bow and concealment: ambush, mark, and vanish.", ""},
    {"brawling", "Brawling", TalentTree::Brawling, "Charge, grab and throw: put enemies into fire, walls, each other and chasms. Any weapon or none.", ""},
    {"whip", "Whip", TalentTree::Whip, "Strike from two tiles away and drag enemies to you, through whatever lies between.", "leather_whip"},
    {"shadow", "Shadow", TalentTree::Shadow, "Darkness magic: snuff out the lights, blind your foes and strike from the dark.", ""},
    {"radiance", "Radiance", TalentTree::Radiance, "Light magic: sear the undead, blind with flares and bring back the dawn.", ""},
    {"alchemy", "Alchemy", TalentTree::Alchemy, "Thrown flasks of oil, fire and acid: make the ground fight for you.", ""},
    {"spear", "Spear", TalentTree::Spear, "Reach and footing: strike from two tiles, brace for the charge, vault over trouble.", "iron_spear"},
    {"daggers", "Daggers", TalentTree::Daggers, "Open wounds and finish the helpless: bleeding, backstabs and the killing blow from hiding.", "steel_dagger"},
    {"mace", "Mace", TalentTree::Mace, "Break guards and bones: sunder armour, stagger the wind-up, shatter the frozen.", "iron_mace"},
    {"crossbow", "Crossbow", TalentTree::Crossbow, "Heavy bolts that knock back, pierce lines and pin foes in place.", "light_crossbow"},
    {"earth", "Earth", TalentTree::Earth, "Shape the ground: spikes that pin, pillars to block or shove into, and quakes.", ""},
    {"tide", "Tide", TalentTree::Tide, "Water as a weapon: flood the ground, shove with waves, drag foes into the deep.", ""},
    {"hexes", "Hexes", TalentTree::Hexes, "Curses: make foes miss, share their pain, and turn them against their own.", ""},
    {"venom", "Venom", TalentTree::Venom, "Poison and plague: bolts, clouds of gas that explode in fire, and sickness that spreads.", ""},
    {"traps", "Traps", TalentTree::Traps, "Set snares, tripwires and charges where your foes will walk.", ""},
    {"skirmish", "Skirmish", TalentTree::Skirmish, "Moving is attacking: lunge, cut past, strike from a running start.", ""},
    {"lamplighter", "Lamplighter", TalentTree::Lamplighter, "Radiance and Fire: fight with the torch itself.", ""},
    {"stormlance", "Stormlance", TalentTree::Stormlance, "Spear and Lightning: a spear that carries the storm. Needs a spear.", ""},
    {"hexblade", "Hexblade", TalentTree::Hexblade, "One-Handed and Hexes: a blade that lays and feeds on curses. Needs a one-handed weapon.", ""},
    {"saboteur", "Saboteur", TalentTree::Saboteur, "Stealth and Alchemy: caltrops, smoke and booby traps.", ""},
    {"stonefist", "Stonefist", TalentTree::Stonefist, "Brawling and Earth: fists of stone, and pillars to break foes against.", ""},
    {"warbanner", "Warbanner", TalentTree::Warbanner, "A deep tree: plant a war standard, hold your ground beside it, and break their packs.", ""},
    {"forgeborn", "Forgeborn", TalentTree::Forgeborn, "A deep tree: burning plate. Struck, you heat up; hot, your blows sear.", ""},
    {"slagcaller", "Slagcaller", TalentTree::Slagcaller, "A deep tree: molten ground, and slag that rises to fight for you.", ""},
    {"tempest", "Tempest", TalentTree::Tempest, "A deep tree: lightning that stays. Storms that follow you, and bolts that leap.", ""},
    {"bonewright", "Bonewright", TalentTree::Bonewright, "A deep tree: build from the dead. Bone walls, bone armour, and a guardian that grows.", ""},
    {"rimeheart", "Rimeheart", TalentTree::Rimeheart, "A deep tree: freeze the ground itself. Ice that spreads, and blows that shatter it.", ""},
    {"briarheart", "Briarheart", TalentTree::Briarheart, "A deep tree: thorns that grow from your traps and your blood.", ""},
}};
inline const TreeDefinition* findTree(const std::string& id) {
    for (const auto& t : kTalentTrees) if (id == t.id) return &t;
    return nullptr;
}
inline bool startingTreeAllowed(PlayerClass cls, TalentTree tree) {
    if (cls == PlayerClass::Warrior) return tree == TalentTree::OneHanded || tree == TalentTree::TwoHanded || tree == TalentTree::Shield || tree == TalentTree::Brawling;
    if (cls == PlayerClass::Thief) return tree == TalentTree::Daggers || tree == TalentTree::Bow || tree == TalentTree::Whip;
    return cls == PlayerClass::Mage && (tree == TalentTree::Fire || tree == TalentTree::Ice || tree == TalentTree::Lightning || tree == TalentTree::Arcane);
}
// Utility trees: how you survive rather than how you kill. They take
// utility points, and open without tree points.
inline bool utilityTree(const std::string& id) {
    return id=="cloth" || id=="light_armour" || id=="heavy_armour" || id=="acrobatics" || id=="stealth" || id=="alchemy" || id=="traps";
}
// The colours a talent carries. Each rank bought in a node adds one point of
// its colour; two colours held deeply enough wake the resonance between them.
enum class Affinity { None, Steel, Guard, Motion, Hunt, Guile, Flame, Frost, Storm, Earth, Water, Light, Dark, Arcane, Blood, Death, Rot };
struct AffinityInfo { const char* name; std::uint8_t r, g, b; };
inline AffinityInfo affinityInfo(Affinity a) {
    switch (a) {
        case Affinity::Steel: return {"Steel", 196, 200, 214};
        case Affinity::Guard: return {"Guard", 214, 178, 110};
        case Affinity::Motion: return {"Motion", 130, 214, 150};
        case Affinity::Hunt: return {"Hunt", 170, 196, 90};
        case Affinity::Guile: return {"Guile", 120, 150, 120};
        case Affinity::Flame: return {"Flame", 244, 124, 52};
        case Affinity::Frost: return {"Frost", 150, 210, 250};
        case Affinity::Storm: return {"Storm", 120, 170, 255};
        case Affinity::Earth: return {"Earth", 176, 140, 96};
        case Affinity::Water: return {"Water", 80, 150, 210};
        case Affinity::Light: return {"Light", 250, 230, 150};
        case Affinity::Dark: return {"Dark", 130, 100, 170};
        case Affinity::Arcane: return {"Arcane", 180, 130, 255};
        case Affinity::Blood: return {"Blood", 210, 50, 60};
        case Affinity::Death: return {"Death", 160, 170, 150};
        case Affinity::Rot: return {"Rot", 140, 170, 70};
        case Affinity::None: break;
    }
    return {"", 200, 200, 200};
}
// A resonance wakes once you hold this many points of each of its colours.
inline constexpr int kResonancePoints = 4;

// One node of a talent tree. A node has as many ranks as `ranks` holds (at
// most kMaxTalentRank). `prerequisites` lists nodes of which at least one
// must be learned first; nodes of one tree that share a `fork` exclude each
// other, so learning one closes the rest.
struct TalentDefinition {
    std::string id;
    std::string treeId;
    int tier = 0;
    std::vector<Talent> ranks;
    std::string mastery; // what the last rank adds beyond bigger numbers, if anything
    std::vector<std::string> prerequisites;
    std::string fork;
    Affinity affinity = Affinity::None;           // the colour each rank adds
    Affinity resonance[2]{Affinity::None, Affinity::None}; // a resonance: the two colours it lies between
    int maxRank() const { return static_cast<int>(ranks.size()); }
    // The talent at `rank` (1-based), clamped to the ranks this node has.
    const Talent& atRank(int rank) const { return ranks[static_cast<std::size_t>(std::clamp(rank, 1, maxRank()) - 1)]; }
};

// Rank 5 masteries: mostly utility rather than raw damage. Applied to the
// fifth rank only, after the ordinary rank curve.
inline void applyMastery(TalentDefinition& d) {
    Talent& m = d.ranks.back();
    const auto mark = [&] { m.onHitEffect = StatusEffectInstance{StatusEffectType::Marked, 3, 1}; m.onHitChance = 1.f; };
    const auto longer = [&] { if (m.selfBuffEffect) ++m.selfBuffEffect->turnsRemaining; };
    const auto dodge = [&](int amount) { m.selfBuffEffect = StatusEffectInstance{StatusEffectType::Evasion, 1, amount}; };
    const auto stun = [&](int turns) { m.onHitEffect = StatusEffectInstance{StatusEffectType::Stun, turns, 0}; m.onHitChance = 1.f; };
    const std::string& id = d.id;
    if (id == "one_handed.quick_strike") { mark(); d.mastery = "Marks the target: its next direct hit taken deals +25%."; }
    if (id == "one_handed.parry") { longer(); d.mastery = "Guard lasts one enemy response longer."; }
    if (id == "one_handed.execution") { m.conditionalHpFraction = .4f; d.mastery = "Double damage from 40% HP instead of 30%."; }
    if (id == "two_handed.cleave") { m.pushDistance = 1; d.mastery = "Pushes surviving targets one tile away."; }
    if (id == "two_handed.fury") { m.hpCost = 0; d.mastery = "Costs no life."; }
    if (id == "two_handed.whirlwind") { m.pushDistance = 2; d.mastery = "Pushes surviving targets two tiles away."; }
    if (id == "two_handed.leap_slam") { m.slamStun = true; d.mastery = "Stuns what the landing strikes."; }
    if (id == "two_handed.blood_frenzy") { if (m.selfBuffEffect) m.selfBuffEffect->magnitude = 40; d.mastery = "Heals for 40% of the damage dealt."; }
    if (id == "shield.bash") { m.pushDistance = 2; d.mastery = "Pushes two tiles."; }
    if (id == "shield.guard") { longer(); d.mastery = "Guard lasts one enemy response longer."; }
    if (id == "shield.rush") { stun(1); d.mastery = "The impact stuns for one enemy turn."; }
    if (id == "shield.bastion") { if (m.selfBuffEffect) m.selfBuffEffect->magnitude = 12; d.mastery = "Guard 12."; }
    if (id == "shield.shockwave") { m.areaRadius = 2; d.mastery = "Reaches enemies up to two tiles away."; }
    if (id == "bow.quick_shot") { mark(); d.mastery = "Marks the target: its next direct hit taken deals +25%."; }
    if (id == "bow.volley") { m.onHitEffect = StatusEffectInstance{StatusEffectType::Pinned, 2, 0}; m.markOnHit = true; d.mastery = "Also pins what it hits for a turn."; }
    if (id == "bow.point_blank") { m.pushDistance = 1; d.mastery = "Knocks the foe back a tile too."; }
    if (id == "bow.rain") { m.lingerTurns = 4; d.mastery = "The arrows fall two turns longer."; }
    if (id == "bow.piercing_shot") { m.bonusCritChance += .2f; d.mastery = "A further +20% critical chance."; }
    if (id == "stealth.conceal") { longer(); d.mastery = "Hide for four responses."; }
    if (id == "stealth.shadow_step") { m.moveDistance += 2; d.mastery = "Slip two tiles further."; }
    if (id == "stealth.vanish") { longer(); d.mastery = "Hide for four responses."; }
    if (id == "stealth.smoke_bomb") { m.cooldownTurns -= 4; d.mastery = "Ready again four turns sooner."; }
    if (id == "stealth.feign") { m.restoreHpPercent = 10; d.mastery = "You also recover 10% of your life."; }
    if (id == "briarheart.seed") { m.areaRadius = 2; d.mastery = "The patch is 5 by 5."; }
    if (id == "briarheart.lash") { m.lashPin = 2; d.mastery = "Pins for two turns."; }
    if (id == "briarheart.blood") { m.hpCost = 3; d.mastery = "Costs 3 life."; }
    if (id == "briarheart.overgrowth") { m.overgrowth = 8; d.mastery = "Lasts 8 turns."; }
    if (id == "briarheart.heart") { m.heartHeals = true; d.mastery = "You also heal 2 a turn for each bleeding foe in view."; }
    if (id == "rimeheart.rime") { m.areaRadius = 2; d.mastery = "Freezes 5 by 5."; }
    if (id == "rimeheart.lance") { m.onHitEffect = StatusEffectInstance{StatusEffectType::Stun, 1, 0}; d.mastery = "It also stuns."; }
    if (id == "rimeheart.path") { m.moveDistance += 2; d.mastery = "Slide two tiles further."; }
    if (id == "rimeheart.freeze") { m.deepFreeze = 4; d.mastery = "Reaches 4 tiles."; }
    if (id == "rimeheart.heart") { m.wintersHeart = 4; d.mastery = "The burst reaches 3 tiles and freezes those it hits for a turn."; }
    if (id == "bonewright.armour") { if (m.selfBuffEffect) m.selfBuffEffect->magnitude = 6; d.mastery = "Guard 6."; }
    if (id == "bonewright.wall") { m.boneWall = 5; d.mastery = "A wall of 5."; }
    if (id == "bonewright.guard") { m.summonRank = 3; d.mastery = "The guardian rises tougher."; }
    if (id == "bonewright.storm") { m.boneStorm = 5; d.mastery = "The storm lasts 5 turns."; }
    if (id == "bonewright.lord") { m.boneLord = 8; d.mastery = "Lasts 8 turns."; }
    if (id == "tempest.stormcall") { m.stormcall = 6; d.mastery = "The storm stays 6 turns."; }
    if (id == "tempest.forked") { m.chainJumps = 3; d.mastery = "Leaps to three more foes."; }
    if (id == "tempest.static") { m.staticField = 2; d.mastery = "Covers everything within 2 tiles."; }
    if (id == "tempest.thunderhead") { m.lingerTurns = 5; d.mastery = "The storm stays two turns longer."; }
    if (id == "tempest.ride") { m.moveDistance += 2; d.mastery = "Flash two tiles further."; }
    if (id == "slagcaller.pool") { m.areaRadius = 2; d.mastery = "The pool spreads over 5 by 5."; }
    if (id == "slagcaller.slagling") { m.summonCount = 2; d.mastery = "Two slaglings rise."; }
    if (id == "slagcaller.hail") { m.lingerTurns = 4; d.mastery = "The cinders fall two turns longer."; }
    if (id == "slagcaller.golem") { m.summonDuration = 30; d.mastery = "The golem stands for 30 turns."; }
    if (id == "slagcaller.eruption") { m.areaRadius = 2; d.mastery = "It strikes everything within two tiles."; }
    if (id == "forgeborn.stoke") { m.gainHeat = 6; d.mastery = "Gain 6 Heat."; }
    if (id == "forgeborn.searing") { m.shape = EffectShape::AreaAroundTarget; m.areaRadius = 1; d.mastery = "The blow also strikes the foes beside its target."; }
    if (id == "forgeborn.plate") { m.moltenPlate = 6; d.mastery = "Lasts 6 turns."; }
    if (id == "forgeborn.vent") { m.areaRadius = 3; d.mastery = "The blast reaches three tiles."; }
    if (id == "forgeborn.forgeheart") { m.forgeheart = 7; if (m.selfBuffEffect) m.selfBuffEffect->turnsRemaining = 7; d.mastery = "Lasts 7 turns."; }
    if (id == "warbanner.plant") { m.plantBanner = 12; d.mastery = "The banner stands for 12 turns."; }
    if (id == "warbanner.rally") { m.breakWindups = true; d.mastery = "Shaken foes also lose whatever they were winding up."; }
    if (id == "warbanner.bash") { m.crashStun = 2; d.mastery = "The crash deals double damage."; }
    if (id == "warbanner.last") { m.plantBanner = 9; d.mastery = "The great standard stands for 9 turns."; }
    if (id == "warbanner.charge") { m.moveDistance += 2; d.mastery = "Charge two tiles further."; }
    if (id == "daggers.assassinate") { m.conditionalHpFraction = .6f; d.mastery = "Triple damage below 60% life instead of half."; }
    if (id == "acrobatics.tumble") { dodge(15); d.mastery = "+15% dodge for one enemy response after tumbling."; }
    if (id == "acrobatics.vault_kick") { stun(1); d.mastery = "The kick stuns for one enemy turn."; }
    if (id == "acrobatics.leap") { if (m.selfBuffEffect) m.selfBuffEffect->magnitude = 30; d.mastery = "+30% dodge instead of +20%."; }
    if (id == "acrobatics.somersault") { m.blitz = true; m.power = 5; d.mastery = "You cut the foes you roll past."; }
    if (id == "spellblade.imbue") { longer(); longer(); d.mastery = "The imbue lasts two turns longer."; }
    if (id == "spellblade.strike") { m.pierceBehind = true; d.mastery = "Runs through to the foe behind."; }
    if (id == "spellblade.cleave") { m.pushDistance = 1; d.mastery = "Shoves them back a tile."; }
    if (id == "spellblade.blink_strike") { stun(1); d.mastery = "Stuns the foe."; }
    if (id == "animation.raise") { m.summonCount = 2; d.mastery = "Raises two at once."; }
    if (id == "animation.swap") { m.hasteSelf = 30; d.mastery = "You come out of the swap 30% faster for three turns."; }
    if (id == "animation.spear") { m.onHitEffect = StatusEffectInstance{StatusEffectType::Bleed, 3, 2}; m.onHitChance = 1.f; d.mastery = "Everything it pierces bleeds."; }
    if (id == "animation.prison") { m.prisonCut = true; d.mastery = "The walls also cut the prisoner."; }
    if (id == "blood_magic.pact") { longer(); d.mastery = "Lasts a turn longer."; }
    if (id == "blood_magic.drain") { m.areaRadius = 1; m.shape = EffectShape::AreaAroundTarget; d.mastery = "Drains the foes beside the target too."; }
    if (id == "blood_magic.boil") { m.areaRadius = 3; d.mastery = "Reaches three tiles out."; }
    if (id == "blood_magic.rite") { if (m.selfBuffEffect) m.selfBuffEffect->magnitude = 12; d.mastery = "+12 on every hit."; }
    if (id == "acrobatics.untouchable") { longer(); d.mastery = "Lasts three turns."; }
    if (id == "cloth.shroud") { m.wardPercent = 35; d.mastery = "Ward worth 35% of your maximum mana."; }
    if (id == "cloth.surge") { m.restoreMana = 35; d.mastery = "Restores 35 mana."; }
    if (id == "cloth.burst") { m.areaRadius = 3; d.mastery = "Reaches three tiles out."; }
    if (id == "light_armour.feint") { m.onHitEffect = StatusEffectInstance{StatusEffectType::Blinded, 2, 0}; m.markOnHit = true; d.mastery = "The feint also blinds it for a turn."; }
    if (id == "light_armour.blur") { if (m.selfBuffEffect) m.selfBuffEffect->magnitude = 45; d.mastery = "+45% dodge."; }
    if (id == "light_armour.perfect") { m.bonusCritDamageMultiplier += .5f; d.mastery = "The critical hit lands half again as hard."; }
    if (id == "heavy_armour.fortify") { if (m.selfBuffEffect) m.selfBuffEffect->magnitude = 6; d.mastery = "Guard 6."; }
    if (id == "heavy_armour.juggernaut") { stun(1); d.mastery = "The impact stuns."; }
    if (id == "heavy_armour.unbreakable") { if (m.selfBuffEffect) m.selfBuffEffect->magnitude = 12; d.mastery = "Guard 12."; }
    if (id == "fire.ember_bolt") { if (m.onHitEffect) m.onHitEffect->magnitude = 2; d.mastery = "Burn deals 2 damage per turn."; }
    if (id == "fire.fireball") { m.splashSurface = 3; m.splashTurns = 4; m.scatterSplash = true; d.mastery = "Leaves fire burning on the ground where it bursts."; }
    if (id == "fire.flame_wall") { m.splashTurns = 6; if (m.onHitEffect) m.onHitEffect->magnitude = 2; d.mastery = "Burns for six turns, and hotter: 2 a turn."; }
    if (id == "fire.firestorm") { m.areaRadius = 3; d.mastery = "The storm covers three tiles."; }
    if (id == "one_handed.pommel") { m.markOnHit = true; d.mastery = "Also marks the target: its next direct hit taken deals +25%."; }
    if (id == "one_handed.blade_dance") { m.guardPerHit = 2; d.mastery = "Each hit gives Guard 2."; }
    if (id == "arcane.repulse") { m.stunOnImpact = true; d.mastery = "Foes that slam into something are stunned for a turn."; }
    if (id == "arcane.torrent") { m.echoBeam = true; d.mastery = "The beam fires again down the same line at the start of your next turn."; }
    if (id == "fire.meteor") { m.statusBonusPercent = 100; d.mastery = "Consuming Burn doubles the hit (+100%)."; }
    if (id == "ice.shard") { m.splashSurface = 4; m.splashTurns = 4; d.mastery = "Freezes the ground under the target."; }
    if (id == "ice.nova") { m.splashSurface = 4; m.splashTurns = 4; d.mastery = "Leaves a ring of ice around you."; }
    if (id == "ice.rime_field") { m.areaRadius = 2; d.mastery = "The field reaches two tiles out."; }
    if (id == "ice.blizzard") { m.lingerTurns = 4; d.mastery = "The storm lasts two turns longer."; }
    if (id == "ice.shatter") { m.statusBonusPercent = 100; d.mastery = "Consuming Chill doubles the hit (+100%)."; }
    if (id == "lightning.bolt") { m.pierceBehind = true; d.mastery = "Also strikes whoever stands right behind the target."; }
    if (id == "lightning.chain") { m.chainJumps = 2; d.mastery = "Jumps twice."; }
    if (id == "lightning.thunderclap") { m.pushDistance = 2; d.mastery = "Shoves them two tiles."; }
    if (id == "lightning.tempest") { m.onHitEffect = StatusEffectInstance{StatusEffectType::Shock, 3, 0}; m.onHitChance = 1.f; d.mastery = "Shocks everything it strikes."; }
    if (id == "lightning.discharge") { m.statusBonusPercent = 100; d.mastery = "Consuming Shock doubles the hit (+100%)."; }
    if (id == "arcane.bolt") { m.manaCost = std::max(1, m.manaCost / 2); d.mastery = "Costs half as much mana."; }
    if (id == "arcane.blink") { m.vault = true; d.mastery = "Passes through creatures in the way."; }
    if (id == "arcane.mind_shatter") { stun(2); d.mastery = "Stuns for two enemy turns (bosses still resist repeats)."; }
    if (id == "cloth.gather_mana") { m.restoreHpPercent = 10; d.mastery = "Also restores 10% of maximum life."; }
    if (id == "cloth.pulse") { m.pushDistance = 3; d.mastery = "Pushes survivors three tiles."; }
    if (id == "light_armour.sidestep") { dodge(15); d.mastery = "+15% dodge for one enemy response after the step."; }
    if (id == "light_armour.parting_strike") { m.retreatDistance = 3; d.mastery = "Retreat three tiles instead of two."; }
    if (id == "heavy_armour.shoulder_check") { stun(1); d.mastery = "The check stuns for one enemy turn."; }
    if (id == "heavy_armour.second_wind") { m.cleanse = true; d.mastery = "Also removes Poison, Burn, Chill, Marked and curses."; }
    if (id == "brawling.tackle") { m.pushDistance = 2; d.mastery = "Knocks the target two tiles."; }
    if (id == "brawling.grapple") { m.selfBuffEffect = StatusEffectInstance{StatusEffectType::Guard, 2, 2}; d.mastery = "Bracing against your catch grants Guard 2 for two enemy responses."; }
    if (id == "brawling.hurl") { m.domino = true; d.mastery = "Domino: a hurled enemy knocks whatever it hits one tile further."; }
    if (id == "whip.lash") { m.pullDistance = 2; d.mastery = "Pulls two tiles."; }
    if (id == "whip.trip") { stun(2); d.mastery = "The fall stuns for two enemy turns (bosses still resist repeats)."; }
    if (id == "whip.snare") { if (m.onHitEffect) m.onHitEffect->turnsRemaining = 4; d.mastery = "Holds the snared enemy for four turns."; }
    if (id == "shadow.bolt") { m.blindInDark = true; d.mastery = "A target standing in darkness is blinded too."; }
    if (id == "shadow.step") { m.arrivalBlind = true; d.mastery = "The foe nearest where you arrive is blinded."; }
    if (id == "shadow.devour") { m.drainPercent = 100; d.mastery = "Drains all the damage it deals."; }
    if (id == "shadow.snuff") { m.selfBuffEffect = StatusEffectInstance{StatusEffectType::Concealed, 2, 2}; d.mastery = "You vanish into the new dark: Concealed for two responses."; }
    if (id == "shadow.veil") { m.areaRadius = 2; d.mastery = "Blinds everything within two tiles."; }
    if (id == "radiance.sear") { m.onHitEffect = StatusEffectInstance{StatusEffectType::Burn, 3, 2}; m.onHitChance = 1.f; d.mastery = "Sets the target burning (2 per turn)."; }
    if (id == "radiance.flare") { m.areaRadius = 2; d.mastery = "The flare covers two tiles."; }
    if (id == "radiance.holy_light") { m.cleanse = true; d.mastery = "Also removes Poison, Burn, Chill, Marked and curses."; }
    if (id == "radiance.judgement") { m.areaRadius = 1; d.mastery = "The pillar strikes everything beside the target too."; }
    if (id == "radiance.dawn") { m.areaRadius = 4; d.mastery = "Reaches enemies up to four tiles away."; }
    if (id == "alchemy.oil") { m.areaRadius = 2; d.mastery = "The flask splashes two tiles."; }
    if (id == "alchemy.firebomb") { if (m.onHitEffect) m.onHitEffect->magnitude = 2; d.mastery = "Burn deals 2 damage per turn."; }
    if (id == "alchemy.acid") { m.areaRadius = 2; d.mastery = "The flask splashes two tiles."; }
    if (id == "spear.thrust") { mark(); d.mastery = "Marks the target: its next direct hit taken deals +25%."; }
    if (id == "spear.brace") { longer(); d.mastery = "Braced for one enemy response longer."; }
    if (id == "spear.vault") { m.landingSlam = 6; d.mastery = "You land with a strike on the foes beside you."; }
    if (id == "spear.impale") { stun(1); d.mastery = "Stuns the foe instead of pinning it."; }
    if (id == "spear.throw") { m.pushDistance = 1; d.mastery = "Knocks everything it hits back a tile."; }
    if (id == "daggers.lacerate") { if (m.onHitEffect) m.onHitEffect->magnitude = 3; d.mastery = "Bleed deals 3 damage per turn."; }
    if (id == "daggers.backstab") { m.selfBuffEffect = StatusEffectInstance{StatusEffectType::Concealed, 1, 2}; d.mastery = "You melt back into the shadows: Concealed for one response."; }
    if (id == "daggers.throw") { m.pierceBehind = true; d.mastery = "Also hits whoever stands right behind the target."; }
    if (id == "daggers.eviscerate") { m.statusBonusPercent = 1; d.mastery = "Triple the bleed, not double."; }
    if (id == "mace.crush") { m.pushDistance = 1; d.mastery = "Also knocks the foe back a tile."; }
    if (id == "mace.stagger") { m.markOnHit = true; d.mastery = "Also marks the foe: its next direct hit taken deals +25%."; }
    if (id == "mace.slam") { m.areaRadius = 2; d.mastery = "Reaches foes two tiles out."; }
    if (id == "mace.skullcracker") { if (m.onHitEffect) m.onHitEffect->turnsRemaining = 3; d.mastery = "Stuns for three turns."; }
    if (id == "mace.shatter") { stun(1); d.mastery = "The blow also stuns for one enemy turn."; }
    if (id == "crossbow.heavy") { m.pushDistance = 2; d.mastery = "Knocks the target back two tiles."; }
    if (id == "crossbow.pierce") { m.onHitEffect = StatusEffectInstance{StatusEffectType::Bleed, 3, 2}; m.onHitChance = 1.f; d.mastery = "Leaves everything it passes through bleeding."; }
    if (id == "crossbow.explosive") { m.areaRadius = 2; d.mastery = "The burst covers two tiles."; }
    if (id == "crossbow.ballista") { stun(1); d.mastery = "Stuns what it hits."; }
    if (id == "earth.spike") { m.areaRadius = 1; m.shape = EffectShape::AreaAroundTarget; d.mastery = "Also strikes and pins the foes beside the target."; }
    if (id == "earth.pillar") { m.pillarShove = true; d.mastery = "The pillar bursts up and shoves the foes beside it a tile."; }
    if (id == "earth.grasp") { m.areaRadius = 2; d.mastery = "Seizes everything within two tiles."; }
    if (id == "earth.boulder") { stun(1); d.mastery = "Stuns what it hits."; }
    if (id == "earth.quake") { m.areaRadius = 3; d.mastery = "Reaches enemies up to three tiles away."; }
    if (id == "tide.bolt") { m.onHitEffect = StatusEffectInstance{StatusEffectType::Chill, 2, 20}; m.onHitChance = 1.f; d.mastery = "Also chills the target."; }
    if (id == "tide.wave") { m.pushDistance = 3; d.mastery = "Shoves enemies three tiles."; }
    if (id == "tide.maelstrom") { m.areaRadius = 3; d.mastery = "Covers three tiles."; }
    if (id == "tide.undertow") { m.onHitEffect = StatusEffectInstance{StatusEffectType::Pinned, 2, 0}; m.onHitChance = 1.f; d.mastery = "It comes up gasping: pinned for a turn."; }
    if (id == "tide.flood") { m.onHitEffect = StatusEffectInstance{StatusEffectType::Chill, 3, 20}; m.onHitChance = 1.f; d.mastery = "Chills everything it floods."; }
    if (id == "hexes.misfortune") { m.areaRadius = 1; m.shape = EffectShape::AreaAroundTarget; d.mastery = "Curses the foes beside it too."; }
    if (id == "hexes.link") { if (m.selfBuffEffect) m.selfBuffEffect->magnitude = 100; d.mastery = "All of the damage jumps, not half."; }
    if (id == "hexes.puppet") { if (m.selfBuffEffect) m.selfBuffEffect->turnsRemaining = 5; d.mastery = "The puppet serves for five turns."; }
    if (id == "venom.bolt") { if (m.onHitEffect) m.onHitEffect->magnitude = 3; d.mastery = "Poison deals 3 damage per turn."; }
    if (id == "venom.miasma") { m.areaRadius = 2; d.mastery = "The cloud spreads two tiles."; }
    if (id == "venom.plague") { if (m.onHitEffect) m.onHitEffect->turnsRemaining = 7; d.mastery = "The plague lasts seven turns."; }
    if (id == "venom.fester") { m.festerSpread = true; d.mastery = "The festering poison spreads to the foes beside it."; }
    if (id == "venom.blight") { if (m.onHitEffect) m.onHitEffect->magnitude = 5; d.mastery = "Every hit it takes deals 5 more."; }
    if (id == "traps.snare" || id == "traps.tripwire") { m.areaRadius = 1; m.shape = EffectShape::AreaAroundTarget; d.mastery = "Sets them on a spot and every tile around it."; }
    if (id == "traps.bear") { m.placeTrap = 7; d.mastery = "The jaws leave it bleeding too."; }
    if (id == "traps.net") { m.areaRadius = 2; d.mastery = "The net catches everything within two tiles."; }
    if (id == "alchemy.frost") { m.areaRadius = 2; d.mastery = "The flask splashes two tiles."; }
    if (id == "alchemy.greek_fire") { m.areaRadius = 3; d.mastery = "Sets everything within three tiles ablaze."; }
    if (id == "hexes.enfeeble") { if (m.selfBuffEffect) m.selfBuffEffect->magnitude = 60; d.mastery = "Slowed by 60%."; }
    if (id == "hexes.doom") { if (m.selfBuffEffect) m.selfBuffEffect->magnitude = 30; d.mastery = "Doom falls for 30."; }
    if (id == "traps.rigged") { m.areaRadius = 2; d.mastery = "The charge blasts two tiles."; }
    if (id == "skirmish.blitz") { m.selfBuffEffect = StatusEffectInstance{StatusEffectType::Opening, 2, 0}; d.mastery = "You end the run with Opening."; }
    if (id == "skirmish.hit_and_run") { m.retreatDistance = 2; d.mastery = "Step back two tiles."; }
    if (id == "skirmish.slipstream") { if (m.selfBuffEffect) m.selfBuffEffect->magnitude = 40; m.hasteSelf = 75; d.mastery = "+40% dodge, and 75% faster."; }
    if (id == "skirmish.flying_kick") { stun(1); d.mastery = "The kick stuns."; }
    if (id == "brawling.haymaker") { stun(1); d.mastery = "The punch stuns too."; }
    if (id == "brawling.piledriver") { m.onHitEffect = StatusEffectInstance{StatusEffectType::Stun, 2, 0}; d.mastery = "Stuns for two turns."; }
    if (id == "whip.disarm") { if (m.onHitEffect) m.onHitEffect->magnitude = 50; d.mastery = "Its attacks miss 50% more often."; }
    if (id == "whip.whirl") { m.areaRadius = 3; d.mastery = "Reaches three tiles out."; }
    if (id == "lamplighter.swing") { m.areaRadius = 1; m.shape = EffectShape::AreaAroundTarget; d.mastery = "The swing catches the foes beside the target too."; }
    if (id == "lamplighter.brandish") { m.areaRadius = 2; d.mastery = "Reaches two tiles out."; }
    if (id == "lamplighter.pyre") { m.areaRadius = 1; m.shape = EffectShape::AreaAroundTarget; d.mastery = "The fire spreads to the foes beside it."; }
    if (id == "shadow_archer.shot") { m.onHitEffect = StatusEffectInstance{StatusEffectType::Blinded, 2, 0}; m.onHitChance = 1.f; d.mastery = "Also blinds the target."; }
    if (id == "shadow_archer.mark") { longer(); d.mastery = "Lasts a turn longer."; }
    if (id == "shadow_archer.smoke") { m.areaRadius = 2; d.mastery = "The smoke spreads two tiles."; }
    if (id == "shadow_archer.pin") { if (m.onHitEffect) m.onHitEffect->turnsRemaining = 3; if (m.secondHitEffect) m.secondHitEffect->turnsRemaining = 3; d.mastery = "Pinned and blinded for three turns."; }
    if (id == "stormlance.rod") { m.lingerTurns = 4; d.mastery = "The lightning keeps striking two turns longer."; }
    if (id == "stormlance.ride") { m.splashSurface = 5; m.splashTurns = 3; m.splashPath = true; d.mastery = "The ground crackles behind you."; }
    if (id == "lamplighter.hurl") { m.areaRadius = 2; d.mastery = "The torch's fire spreads two tiles."; }
    if (id == "lamplighter.bonfire") { m.areaRadius = 3; d.mastery = "Reaches enemies up to three tiles away."; }
    if (id == "stormlance.thrust") { m.pierceBehind = true; d.mastery = "The charge runs on into the foe behind."; }
    if (id == "stormlance.javelin") { m.chainJumps = 2; d.mastery = "Jumps twice."; }
    if (id == "stormlance.vault") { m.landingBurst = 10; d.mastery = "The landing burst hits for 10."; }
    if (id == "hexblade.edge") { m.secondHitEffect = StatusEffectInstance{StatusEffectType::Slowed, 3, 20}; d.mastery = "Also slows it by 20%."; }
    if (id == "hexblade.rend") { m.drainPercent = 25; d.mastery = "Heals you for a quarter of the damage."; }
    if (id == "hexblade.cleave") { m.secondHitEffect = StatusEffectInstance{StatusEffectType::Slowed, 3, 20}; d.mastery = "Also slows them by 20%."; }
    if (id == "saboteur.firecracker") { m.areaRadius = 2; d.mastery = "Bursts two tiles wide."; }
    if (id == "saboteur.demolition") { m.areaRadius = 3; d.mastery = "Blows three tiles wide."; }
    if (id == "stonefist.tremor") { m.areaRadius = 2; d.mastery = "Grips everything within two tiles."; }
    if (id == "stonefist.rockfall") { m.slamStun = true; d.mastery = "Stuns what it lands on."; }
    if (id == "hexblade.doom") { if (m.onHitEffect) m.onHitEffect->magnitude = 25; d.mastery = "Doom erupts for 25."; }
    if (id == "saboteur.caltrops") { m.areaRadius = 2; d.mastery = "Scatters caltrops two tiles wide."; }
    if (id == "saboteur.smoke") { longer(); longer(); d.mastery = "Hidden for two responses longer."; }
    if (id == "saboteur.booby") { m.cooldownTurns = std::max(1, m.cooldownTurns - 3); d.mastery = "Cooldown three turns shorter."; }
    if (id == "stonefist.fist") { m.pushDistance = 2; d.mastery = "Knocks the target two tiles."; }
    if (id == "stonefist.slam") { stun(1); d.mastery = "The slam also stuns for one enemy turn."; }
    if (id == "stonefist.landslide") { ++m.chargeDistance; d.mastery = "Charges one tile further."; }
    if (id == "crossbow.pin") { if (m.onHitEffect) m.onHitEffect->turnsRemaining = 5; d.mastery = "Pinned for five enemy turns."; }
}

// Explicit rank profiles share existing targeting/effect data. No runtime content loader.
inline const std::vector<TalentDefinition>& talentCatalog() {
    static const auto catalog = [] {
        std::vector<TalentDefinition> out;
        auto attack = [](const char* name, const char* desc, int damage, int mana, int cd,
                         bool ranged = false, int radius = 0) {
            Talent t; t.name = name; t.description = desc; t.power = damage;
            t.manaCost = mana; t.cooldownTurns = cd;
            t.targeting = ranged ? TargetingMode::RangedEnemyInSight : TargetingMode::AdjacentEnemy;
            t.projectile = ranged; t.areaRadius = radius;
            if (radius) { t.shape = ranged ? EffectShape::AreaAroundTarget : EffectShape::AreaAroundSelf;
                if (!ranged) t.targeting = TargetingMode::Self; }
            return t;
        };
        auto buff = [](const char* name, const char* desc, StatusEffectType effect, int duration, int magnitude, int mana, int cd) {
            Talent t; t.name=name; t.description=desc; t.targeting=TargetingMode::Self;
            t.effectKind=TalentEffectKind::SelfBuff; t.selfBuffEffect=StatusEffectInstance{effect,duration,magnitude};
            t.manaCost=mana; t.cooldownTurns=cd; return t;
        };
        auto move = [](const char* name, const char* desc, int distance, int mana, int cd) {
            Talent t; t.name=name; t.description=desc; t.targeting=TargetingMode::Self;
            t.shape=EffectShape::Movement; t.moveDistance=distance; t.manaCost=mana; t.cooldownTurns=cd; return t;
        };
        auto passive = [](const char* name, const char* desc, PassiveKind kind, int amount) {
            Talent t; t.name=name; t.description=desc; t.passive=true; t.passiveKind=kind; t.passiveMagnitude=amount; return t;
        };
        auto add = [&](int tree, const char* id, int tier, Talent t) {
            const bool hybrid = tree >= 13 && tree <= 16;
            t.id=id; t.tree=kTalentTrees[tree].tree; t.scalingCooldown=t.cooldownTurns;
            t.scalingStat = (tree >= 6 && tree <= 10) ? ScalingStat::Intelligence : ((tree >= 3 && tree <= 5) || tree == 11) ? ScalingStat::Dexterity : ScalingStat::Strength;
            if (tree == 0) t.weaponRequirement=WeaponRequirement::OneHanded;
            if (tree == 1) t.weaponRequirement=WeaponRequirement::TwoHanded;
            if (tree == 2) t.weaponRequirement=WeaponRequirement::Shield;
            if (tree == 3) t.weaponRequirement=WeaponRequirement::Bow;
            if (tree == 18) t.weaponRequirement=WeaponRequirement::Whip;
            if (tree == 18 || tree == 21) t.scalingStat=ScalingStat::Dexterity;
            if (tree == 19 || tree == 20) t.scalingStat=ScalingStat::Intelligence;
            if (tree == 22) t.weaponRequirement=WeaponRequirement::Spear;
            if (tree == 23) { t.weaponRequirement=WeaponRequirement::Dagger; t.scalingStat=ScalingStat::Dexterity; }
            if (tree == 24) t.weaponRequirement=WeaponRequirement::Mace;
            if (tree == 25) { t.weaponRequirement=WeaponRequirement::Crossbow; t.scalingStat=ScalingStat::Dexterity; }
            if (tree >= 26 && tree <= 29) t.scalingStat=ScalingStat::Intelligence;
            if (tree == 30 || tree == 31 || tree == 35) t.scalingStat=ScalingStat::Dexterity;
            if (tree == 32) t.scalingStat=ScalingStat::Intelligence;
            if (tree == 33 || tree == 34 || tree == 36) t.scalingStat=ScalingStat::Strength;
            if (tree >= 39 && tree <= 42) t.scalingStat=ScalingStat::Intelligence;
            if (tree == 43) t.scalingStat=ScalingStat::Dexterity;
            if (tree == 33) t.weaponRequirement=WeaponRequirement::Spear;
            if (tree == 34) t.weaponRequirement=WeaponRequirement::OneHanded;
            if (tree == 10) t.armourRequirement=ArmourRequirement::Cloth;
            if (tree == 11) t.armourRequirement=ArmourRequirement::Light;
            if (tree == 12) t.armourRequirement=ArmourRequirement::Heavy;
            if (tree==13) t.weaponRequirement=WeaponRequirement::Melee;
            if (tree==16) t.weaponRequirement=WeaponRequirement::Bow;
            if (hybrid) t.scalingStat=tree==16?ScalingStat::Dexterity:ScalingStat::Intelligence;
            TalentDefinition d{id,kTalentTrees[tree].id,tier,std::vector<Talent>(kMaxTalentRank,t)};
            // The rank curve, by rank index 1-4 (ranks 2-5).
            for (int rank=1; rank<d.maxRank(); ++rank) {
                auto& r=d.ranks[rank];
                if (r.passive) {
                    constexpr int precise[]{10,12,15,18,20}, efficient[]{10,20,30,35,40};
                    if (r.passiveKind==PassiveKind::Marksmanship || r.passiveKind==PassiveKind::Footwork) r.passiveMagnitude=precise[rank];
                    else if (r.passiveKind==PassiveKind::ArcaneEfficiency) r.passiveMagnitude=efficient[rank];
                    else r.passiveMagnitude += rank;
                }
                else if (r.shape==EffectShape::Movement) {
                    if (rank>=2) ++r.moveDistance;
                    if (rank>=4) ++r.moveDistance;
                    if (rank==3 || rank==4) r.cooldownTurns=std::max(1,t.cooldownTurns-1);
                }
                else if (r.effectKind==TalentEffectKind::SelfBuff) {
                    if (rank>=2) r.cooldownTurns=std::max(1,r.cooldownTurns-1);
                    if (rank>=3 && r.selfBuffEffect) ++r.selfBuffEffect->turnsRemaining;
                    if (rank>=4) r.cooldownTurns=std::max(1,r.cooldownTurns-1);
                } else {
                    constexpr int percent[]{100,120,145,170,200};
                    r.damagePercent=percent[rank];
                    // Charges run further and throws fly further at ranks 3 and 5.
                    if (r.chargeDistance) r.chargeDistance=t.chargeDistance+(rank>=2)+(rank>=4);
                    if (r.hurlDistance) r.hurlDistance=t.hurlDistance+(rank>=2)+(rank>=4);
                    if (t.reach>=3) { r.reach=t.reach+(rank>=2)+(rank>=4); r.pullDistance=r.reach; }
                }
                r.tags=talentTags(r);
            }
            if (d.id=="bow.piercing_shot" || d.id=="two_handed.fury") {
                constexpr int piercing[]{100,112,125,137,150}, fury[]{100,112,112,125,125};
                for (int rank=1; rank<d.maxRank(); ++rank) d.ranks[rank].damagePercent=d.id=="bow.piercing_shot" ? piercing[rank] : fury[rank];
                for (int rank=2; rank<d.maxRank(); ++rank) d.ranks[rank].cooldownTurns=t.cooldownTurns-1;
            }
            for (int rank=0; rank<d.maxRank(); ++rank) {
                auto& r=d.ranks[rank];
                if (r.selfBuffEffect && r.selfBuffEffect->type==StatusEffectType::Concealed)
                    r.selfBuffEffect->magnitude=rank+1;
            }
            if (d.id=="stealth.conceal") {
                // Detection improves with rank; avoid also improving cost and uptime.
                for (auto& r:d.ranks) { r.manaCost=t.manaCost; r.cooldownTurns=t.cooldownTurns; }
            }
            // Spell costs double at every rank, before Arcane Efficiency.
            // Keep rank discounts proportional and catalogue/preview costs aligned.
            if ((tree>=6 && tree<=9) || tree==19 || tree==20 || (tree>=26 && tree<=29) || tree==32) for (auto& r:d.ranks) if (!r.passive) r.manaCost*=2;
            if (d.id=="earth.stoneskin") { constexpr int skin[]{1,1,2,2,3}; for (int rank=0;rank<d.maxRank();++rank) d.ranks[rank].passiveMagnitude=skin[rank]; }
            if (tree>=10 && tree<=12) for (int rank=0; rank<d.maxRank(); ++rank) {
                auto& r=d.ranks[rank];
                // Explicit modest armour profiles: no hidden mana or cooldown discounts.
                r.manaCost=t.manaCost; r.cooldownTurns=t.cooldownTurns;
                if (r.selfBuffEffect) r.selfBuffEffect=t.selfBuffEffect;
                if (r.moveDistance) r.moveDistance=t.moveDistance+(rank>=2)+(rank>=4);
                if (r.passive) r.passiveMagnitude=t.passiveMagnitude*(100+25*rank)/100;
                constexpr int mana[]{6,7,9,10,12}, life[]{10,12,15,17,20};
                if (t.restoreMana) r.restoreMana=mana[rank];
                if (t.restoreHpPercent) r.restoreHpPercent=life[rank];
            }
            if (hybrid) for (int rank=0;rank<d.maxRank();++rank) {
                auto& r=d.ranks[rank]; r.manaCost=t.manaCost; r.cooldownTurns=t.cooldownTurns;
                r.summonRank=std::min(rank+1,3);
                if (d.id=="spellblade.imbue" && rank>0) r.manaCost=3;
                if (t.summonDuration) r.summonDuration=5+rank;
                if (t.drainPercent) r.drainPercent=40+10*rank;
                if (t.stayHiddenPercent) r.stayHiddenPercent=35+10*rank;
                if (t.selfBuffEffect && (t.selfBuffEffect->type==StatusEffectType::Wither || t.selfBuffEffect->type==StatusEffectType::BloodPact))
                    r.selfBuffEffect->turnsRemaining+=rank;
                if (r.returnConcealed && r.selfBuffEffect) r.selfBuffEffect->magnitude=rank+1;
            }
            // Higher ranks cost more: +10% mana per rank over rank 1, rounded
            // to the nearest point (so the cheapest abilities barely move).
            for (int rank=1; rank<d.maxRank(); ++rank) if (!d.ranks[rank].passive && d.ranks[0].manaCost>0)
                d.ranks[rank].manaCost=(d.ranks[0].manaCost*(100+10*rank)+50)/100;
            applyMastery(d);
            for (auto& r:d.ranks) r.tags=talentTags(r);
            out.push_back(std::move(d));
        };
        Talent t;
        // The forked trees: a root, a fork between two actives, the passive of
        // the side you took, and a fork between two capstones. Actives have
        // three ranks (the third changes how they play), passives one.
        const auto shape=[&](const char* id,int ranks,const char* fork,std::vector<std::string> needs) {
            auto& d=out.back();
            if (d.id!=id) throw std::logic_error("shape() must follow its add()");
            if (ranks==3) d.ranks={d.ranks[0],d.ranks[2],d.ranks[4]};
            else if (ranks==1) d.ranks={d.ranks[0]};
            d.fork=fork; d.prerequisites=std::move(needs);
        };
        add(0,"one_handed.quick_strike",0,attack("Quick Strike","A free, efficient melee strike.",4,0,1));
        shape("one_handed.quick_strike",3,"",{});
        add(0,"one_handed.parry",1,buff("Parry","Reduce incoming direct damage by 3 for two enemy responses.",StatusEffectType::Guard,2,3,2,5));
        shape("one_handed.parry",3,"path",{"one_handed.quick_strike"});
        t=attack("Pommel Strike","A blow with the hilt that stuns for one enemy turn. Bosses resist repeated stuns.",5,3,7);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Stun,1,0}; add(0,"one_handed.pommel",1,t);
        shape("one_handed.pommel",3,"path",{"one_handed.quick_strike"});
        add(0,"one_handed.riposte",2,passive("Riposte","While you wield a one-handed weapon, Guard adds +6 to your melee hits.",PassiveKind::Riposte,6));
        shape("one_handed.riposte",1,"",{"one_handed.parry"});
        add(0,"one_handed.exploit",2,passive("Exploit","Your attacks deal +2 damage for each ailment on the target: stun, mark, burn, chill, shock, poison, bleed and the like.",PassiveKind::Exploit,2));
        shape("one_handed.exploit",1,"",{"one_handed.pommel"});
        t=attack("Blade Dance","Strike every adjacent foe; each hit gives you Guard 1 for two enemy responses.",6,5,7,false,1);
        t.guardPerHit=1; add(0,"one_handed.blade_dance",3,t);
        shape("one_handed.blade_dance",3,"capstone",{});
        t=attack("Execution","Double damage against an enemy at or below 30% HP.",8,4,5); t.conditionalHpFraction=.3f; t.conditionalMultiplier=2; add(0,"one_handed.execution",3,t);
        shape("one_handed.execution",3,"capstone",{});
        add(1,"two_handed.cleave",0,attack("Cleave","A heavy swing hitting all four adjacent tiles.",5,3,3,false,1));
        shape("two_handed.cleave",3,"",{});
        t=attack("Berserker's Fury","A heavy blow paid for with 5 HP.",14,0,4); t.hpCost=5; add(1,"two_handed.fury",1,t);
        shape("two_handed.fury",3,"path",{"two_handed.cleave"});
        t=move("Leap Slam","Leap up to three tiles, over anything in the way, and slam down: everything beside where you land is struck.",3,4,7);
        t.vault=true; t.landingSlam=8; add(1,"two_handed.leap_slam",1,t);
        shape("two_handed.leap_slam",3,"path",{"two_handed.cleave"});
        add(1,"two_handed.bloodlust",2,passive("Bloodlust","With a two-handed weapon, your hits deal +6 while you are at or below half life.",PassiveKind::Bloodlust,6));
        shape("two_handed.bloodlust",1,"",{"two_handed.fury"});
        add(1,"two_handed.follow_through",2,passive("Follow Through","After a movement ability, your blows land harder: Empowered +6 for a turn.",PassiveKind::FollowThrough,6));
        shape("two_handed.follow_through",1,"",{"two_handed.leap_slam"});
        t=attack("Whirlwind","Strike in a two-tile circle and push surviving targets one tile away.",8,6,6,false,2); t.pushDistance=1; add(1,"two_handed.whirlwind",3,t);
        shape("two_handed.whirlwind",3,"capstone",{});
        t=buff("Blood Frenzy","For four turns, your hits heal you for a quarter of the damage they deal.",StatusEffectType::Frenzy,4,25,4,12);
        add(1,"two_handed.blood_frenzy",3,t);
        shape("two_handed.blood_frenzy",3,"capstone",{});
        t=attack("Shield Bash","Strike, push one tile and Mark for the next direct hit (+25%, 3 enemy turns).",4,1,3); t.pushDistance=1; t.onHitEffect=StatusEffectInstance{StatusEffectType::Marked,3,1}; add(2,"shield.bash",0,t);
        shape("shield.bash",3,"",{});
        add(2,"shield.guard",1,buff("Guard","Reduce incoming direct damage by 4 for two enemy responses.",StatusEffectType::Guard,2,4,3,6));
        shape("shield.guard",3,"path",{"shield.bash"});
        t=attack("Shield Rush","Charge up to three tiles into a foe in a straight line and knock it back two tiles.",5,3,7);
        t.chargeDistance=3; t.pushDistance=2; add(2,"shield.rush",1,t);
        shape("shield.rush",3,"path",{"shield.bash"});
        add(2,"shield.training",2,passive("Shield Training","While a shield is equipped, Guard blocks 6 extra damage per direct hit.",PassiveKind::ShieldTraining,6));
        shape("shield.training",1,"",{"shield.guard"});
        add(2,"shield.bulwark",2,passive("Bulwark","After you charge, you are guarded: Guard 3 for two enemy responses.",PassiveKind::Bulwark,3));
        shape("shield.bulwark",1,"",{"shield.rush"});
        t=attack("Shield Shockwave","Strike adjacent enemies and stun successful hits for one enemy turn.",5,5,7,false,1); t.onHitEffect=StatusEffectInstance{StatusEffectType::Stun,1,0}; add(2,"shield.shockwave",3,t);
        shape("shield.shockwave",3,"capstone",{});
        t=buff("Bastion","Plant your shield: Guard 8 for three enemy responses, but you can't walk while it holds.",StatusEffectType::Guard,3,8,4,12);
        t.rootSelf=true; add(2,"shield.bastion",3,t);
        shape("shield.bastion",3,"capstone",{});
        add(3,"bow.quick_shot",0,attack("Quick Shot","An efficient projectile intercepted by the first enemy.",5,2,1,true));
        shape("bow.quick_shot",3,"",{});
        t=attack("Volley","Burst in a two-tile circle and Mark survivors for the next direct hit (+25%, 3 enemy turns).",5,6,4,true,2);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Marked,3,1}; add(3,"bow.volley",1,t);
        shape("bow.volley",3,"path",{"bow.quick_shot"});
        t=attack("Point Blank","Loose an arrow into a foe beside you and spring two tiles back from it.",6,2,5); t.retreatDistance=2;
        add(3,"bow.point_blank",1,t);
        shape("bow.point_blank",3,"path",{"bow.quick_shot"});
        add(3,"bow.marksmanship",2,passive("Marksmanship","Bow attacks gain 15% critical chance while you have Opening from waiting or movement abilities.",PassiveKind::Marksmanship,15));
        shape("bow.marksmanship",1,"",{"bow.volley"});
        add(3,"bow.focus",2,passive("Hunter's Focus","Each hit you land in a row on the same foe deals +2 more, up to +6. Switching targets starts over.",PassiveKind::Momentum,2));
        shape("bow.focus",1,"",{"bow.point_blank"});
        t=attack("Piercing Shot","A precision shot with +20% critical chance and 2x critical damage; stops at first enemy.",10,5,7,true); t.bonusCritChance=.2f; t.bonusCritDamageMultiplier=.5f; add(3,"bow.piercing_shot",3,t);
        shape("bow.piercing_shot",3,"capstone",{});
        t=attack("Rain of Arrows","Arrows fall on everything within a tile of a spot for three turns, striking what stands there as each of your turns begins.",6,6,10,true,1);
        t.projectile=false; t.lingerTurns=2; add(3,"bow.rain",3,t);
        shape("bow.rain",3,"capstone",{});
        add(4,"stealth.conceal",0,buff("Conceal","Hide for three responses. Nearby enemies roll to detect you: rank, your DEX and distance help; enemy DEX increases risk. Detection, attacks and damage reveal you.",StatusEffectType::Concealed,3,1,3,7));
        shape("stealth.conceal",3,"",{});
        t=move("Shadow Step","While concealed, slip up to four tiles. End in darkness and you stay hidden two responses longer.",4,2,5); t.requiresStealth=true;
        add(4,"stealth.shadow_step",1,t);
        shape("stealth.shadow_step",3,"path",{"stealth.conceal"});
        t=buff("Feign Death","Drop as if dead and hide: Concealed for three responses, and every foe within five tiles loses track of you.",StatusEffectType::Concealed,3,2,3,12);
        t.shakeOff=true; add(4,"stealth.feign",1,t);
        shape("stealth.feign",3,"path",{"stealth.conceal"});
        add(4,"stealth.soft_steps",2,passive("Soft Steps","Enemies are 25% less likely to spot you while you hide, and those that haven't noticed you only do once you're in the 8 tiles around them.",PassiveKind::SoftSteps,25));
        shape("stealth.soft_steps",1,"",{"stealth.shadow_step"});
        add(4,"stealth.phantom",2,passive("Phantom","Attacking from concealment has a 35% chance not to reveal you.",PassiveKind::LingeringShadow,35));
        shape("stealth.phantom",1,"",{"stealth.feign"});
        t=move("Vanish","Even while seen: slip up to three tiles away and hide for three responses. Every foe within five tiles loses track of you.",3,5,12);
        t.selfBuffEffect=StatusEffectInstance{StatusEffectType::Concealed,3,1}; t.shakeOff=true; add(4,"stealth.vanish",3,t);
        shape("stealth.vanish",3,"capstone",{});
        t=buff("Smoke Bomb","Smoke bursts around you: every foe in the 24 tiles within two of you is Blinded for two turns and loses track of you.",StatusEffectType::Concealed,2,1,4,12);
        t.smokeBomb=true; t.shakeOff=true; add(4,"stealth.smoke_bomb",3,t);
        shape("stealth.smoke_bomb",3,"capstone",{});
        add(5,"acrobatics.tumble",0,move("Tumble","Move up to three visible tiles, stopping before obstacles and actors.",3,2,4));
        shape("acrobatics.tumble",3,"",{});
        t=attack("Vault Kick","Kick, then retreat up to three tiles even if the hit is dodged.",4,3,4); t.retreatDistance=3; add(5,"acrobatics.vault_kick",1,t);
        shape("acrobatics.vault_kick",3,"path",{"acrobatics.tumble"});
        t=move("Somersault","Roll up to two tiles, over anything in your way.",2,2,5); t.vault=true; add(5,"acrobatics.somersault",1,t);
        shape("acrobatics.somersault",3,"path",{"acrobatics.tumble"});
        add(5,"acrobatics.footwork",2,passive("Footwork","Successful movement abilities grant +15% dodge for one enemy response.",PassiveKind::Footwork,15));
        shape("acrobatics.footwork",1,"",{"acrobatics.vault_kick"});
        add(5,"acrobatics.fleet",2,passive("Fleet","You are always 15% faster.",PassiveKind::Fleet,15));
        shape("acrobatics.fleet",1,"",{"acrobatics.somersault"});
        t=move("Evasive Leap","Move up to four tiles and gain +20% dodge for two enemy responses.",4,4,6); t.selfBuffEffect=StatusEffectInstance{StatusEffectType::Evasion,2,20}; add(5,"acrobatics.leap",3,t);
        shape("acrobatics.leap",3,"capstone",{});
        add(5,"acrobatics.untouchable",3,buff("Untouchable","+50% dodge for two enemy responses.",StatusEffectType::Evasion,2,50,4,12));
        shape("acrobatics.untouchable",3,"capstone",{});
        t=attack("Ember Bolt","A flame projectile; successful hits Burn for 1 damage per turn over three enemy turns.",4,2,1,true); t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,3,1}; add(6,"fire.ember_bolt",0,t);
        shape("fire.ember_bolt",3,"",{});
        t=attack("Fireball","Explode at first impact, burning enemies for 1 damage per turn over three enemy turns.",5,6,5,true,2); t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,3,1}; add(6,"fire.fireball",1,t);
        shape("fire.fireball",3,"path",{"fire.ember_bolt"});
        t=attack("Flame Wall","Fire runs out along a line toward your target and burns on the ground for four turns. Whatever it touches catches fire.",3,5,6,true);
        t.pierceAll=true; t.splashSurface=3; t.splashTurns=4; t.splashPath=true; t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,3,1};
        add(6,"fire.flame_wall",1,t);
        shape("fire.flame_wall",3,"path",{"fire.ember_bolt"});
        add(6,"fire.kindling",2,passive("Kindling","Your attacks and spells deal +3 damage to burning foes.",PassiveKind::Kindling,3));
        shape("fire.kindling",1,"",{"fire.fireball"});
        add(6,"fire.wildfire",2,passive("Wildfire","When a burning foe dies, its fire leaps to the nearest foe within four tiles.",PassiveKind::Wildfire,1));
        shape("fire.wildfire",1,"",{"fire.flame_wall"});
        t=attack("Meteor","Consume an existing Burn on a successful hit for +50% direct damage.",11,9,7,true); t.consumeBurn=true; t.statusBonusPercent=50; add(6,"fire.meteor",3,t);
        shape("fire.meteor",3,"capstone",{});
        t=attack("Firestorm","Fire rains on everything within two tiles of a spot, setting it burning and leaving patches of fire on the ground.",7,9,9,true,2);
        t.projectile=false; t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,3,2};
        t.splashSurface=3; t.splashTurns=4; t.scatterSplash=true; add(6,"fire.firestorm",3,t);
        shape("fire.firestorm",3,"capstone",{});
        t=attack("Ice Shard","A shard of ice: chills on hit, slowing the target by 20% for three enemy turns.",4,2,2,true); t.onHitEffect=StatusEffectInstance{StatusEffectType::Chill,3,20}; add(7,"ice.shard",0,t);
        shape("ice.shard",3,"",{});
        t=attack("Frost Nova","Chill adjacent enemies for three turns; helps create an escape.",4,4,5,false,1); t.onHitEffect=StatusEffectInstance{StatusEffectType::Chill,3,20}; add(7,"ice.nova",1,t);
        shape("ice.nova",3,"path",{"ice.shard"});
        t=attack("Rime Field","Cover a spot and the tiles around it in ice for six turns. Whatever stands in it is chilled.",3,4,6,true,1);
        t.projectile=false; t.onHitEffect=StatusEffectInstance{StatusEffectType::Chill,3,20}; t.splashSurface=4; t.splashTurns=6;
        add(7,"ice.rime_field",1,t);
        shape("ice.rime_field",3,"path",{"ice.shard"});
        add(7,"ice.frostbite",2,passive("Frostbite","Direct hits against chilled enemies deal +6 damage, from any tree.",PassiveKind::Frostbite,6));
        shape("ice.frostbite",1,"",{"ice.nova"});
        add(7,"ice.hoarfrost",2,passive("Hoarfrost","When a chilled foe dies, it shatters and chills everything beside it.",PassiveKind::Hoarfrost,1));
        shape("ice.hoarfrost",1,"",{"ice.rime_field"});
        t=attack("Shatter","Consume Chill on hit for +50% direct damage and a one-turn Stun.",9,6,7,true); t.consumeChill=true; t.statusBonusPercent=50; add(7,"ice.shatter",3,t);
        shape("ice.shatter",3,"capstone",{});
        t=attack("Blizzard","A storm settles over everything within two tiles of a spot for three turns, striking and chilling what stands inside as each of your turns begins.",5,9,10,true,2);
        t.projectile=false; t.onHitEffect=StatusEffectInstance{StatusEffectType::Chill,3,20}; t.lingerTurns=2; add(7,"ice.blizzard",3,t);
        shape("ice.blizzard",3,"capstone",{});
        add(8,"lightning.bolt",0,attack("Lightning Bolt","A direct lightning projectile.",5,2,2,true));
        shape("lightning.bolt",3,"",{});
        t=attack("Chain Lightning","Jump to one visible enemy within three tiles of impact, for half damage. Terrain blocks the jump.",6,5,5,true); t.chain=true; add(8,"lightning.chain",1,t);
        shape("lightning.chain",3,"path",{"lightning.bolt"});
        t=attack("Thunderclap","A blast of thunder around you: shocks every adjacent foe and shoves it back a tile.",4,4,6,false,1);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Shock,3,0}; t.pushDistance=1; add(8,"lightning.thunderclap",1,t);
        shape("lightning.thunderclap",3,"path",{"lightning.bolt"});
        add(8,"lightning.static_charge",2,passive("Static Charge","Direct Lightning hits apply Shock for 5 turns. Shock does not stack and empowers Discharge.",PassiveKind::StaticCharge,5));
        shape("lightning.static_charge",1,"",{"lightning.chain"});
        add(8,"lightning.arc_flash",2,passive("Arc Flash","Any hit you land on a shocked foe arcs 4 damage to the nearest other foe within three tiles.",PassiveKind::ArcFlash,4));
        shape("lightning.arc_flash",1,"",{"lightning.thunderclap"});
        t=attack("Discharge","Consume Shock on a successful hit for +50% direct damage. Cannot reapply Shock.",11,6,7,true); t.consumeShock=true; t.statusBonusPercent=50; add(8,"lightning.discharge",3,t);
        shape("lightning.discharge",3,"capstone",{});
        t=attack("Tempest","Lightning strikes every foe within three tiles of you.",8,10,10,false,3); add(8,"lightning.tempest",3,t);
        shape("lightning.tempest",3,"capstone",{});
        t=attack("Arcane Bolt","Mark on a successful hit: next direct hit gains +25% damage, within 3 enemy turns.",3,2,1,true);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Marked,3,1}; add(9,"arcane.bolt",0,t);
        shape("arcane.bolt",3,"",{});
        add(9,"arcane.blink",1,move("Blink","Move up to three visible tiles; terrain and actors block travel.",3,4,4));
        shape("arcane.blink",3,"path",{"arcane.bolt"});
        t=attack("Repulse","A burst of force shoves every adjacent foe two tiles away: into walls, fire, each other or chasms.",3,4,6,false,1);
        t.pushDistance=2; add(9,"arcane.repulse",1,t);
        shape("arcane.repulse",3,"path",{"arcane.bolt"});
        add(9,"arcane.afterimage",2,passive("Afterimage","When you Blink, the place you left bursts with force after the enemy's next turn, striking everything beside it.",PassiveKind::Afterimage,8));
        shape("arcane.afterimage",1,"",{"arcane.blink"});
        add(9,"arcane.kinetic",2,passive("Kinetic","Foes you shove or throw take +4 when they slam into something.",PassiveKind::HardLanding,4));
        shape("arcane.kinetic",1,"",{"arcane.repulse"});
        t=attack("Mind Shatter","A direct force spell that stuns on a successful hit. Does not chain or travel as a projectile.",7,6,7,true); t.projectile=false; t.onHitEffect=StatusEffectInstance{StatusEffectType::Stun,1,0}; add(9,"arcane.mind_shatter",3,t);
        shape("arcane.mind_shatter",3,"capstone",{});
        t=attack("Arcane Torrent","A beam of raw force that strikes every foe in its line.",9,8,8,true);
        t.pierceAll=true; add(9,"arcane.torrent",3,t);
        shape("arcane.torrent",3,"capstone",{});
        t=Talent{}; t.name="Gather Mana"; t.description="Spend a turn restoring 6/7/9/10/12 mana. Requires cloth or no armour.";
        t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.restoreMana=6; t.cooldownTurns=8;
        add(10,"cloth.gather_mana",0,t);
        shape("cloth.gather_mana",3,"",{});
        t=Talent{}; t.name="Arcane Shroud"; t.description="With cloth or no armour, wrap yourself in a ward worth 20% of your maximum mana. It soaks hits until it is spent.";
        t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.wardPercent=20; t.manaCost=4; t.cooldownTurns=10;
        add(10,"cloth.shroud",1,t);
        shape("cloth.shroud",3,"path",{"cloth.gather_mana"});
        t=attack("Repelling Pulse","With cloth or no armour, hit adjacent enemies and push survivors two tiles. Intelligence-scaled; creates room to cast.",5,6,7,false,1); t.pushDistance=2;
        add(10,"cloth.pulse",1,t);
        shape("cloth.pulse",3,"path",{"cloth.gather_mana"});
        add(10,"cloth.ward",2,passive("Loose Weave","With cloth or no armour and at least half mana, gain +12% dodge. Total dodge is capped at 75%.",PassiveKind::ClothWard,12));
        shape("cloth.ward",1,"",{"cloth.shroud"});
        add(10,"cloth.spellweave",2,passive("Spellweave","With cloth or no armour, magic-tree hits against Burn, Chill or Shock gain +6 damage. Multiple ailments do not stack this bonus.",PassiveKind::Spellweave,6));
        shape("cloth.spellweave",1,"",{"cloth.pulse"});
        t=Talent{}; t.name="Mana Surge"; t.description="With cloth or no armour, draw the weave in: restore 20 mana and 15% of your life.";
        t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.restoreMana=20; t.restoreHpPercent=15; t.cooldownTurns=16;
        add(10,"cloth.surge",3,t);
        shape("cloth.surge",3,"capstone",{});
        // The armour trees' restore table is for Gather Mana; Mana Surge keeps its own numbers.
        for (std::size_t r=0;r<out.back().ranks.size();++r) {
            out.back().ranks[r].restoreMana=r+1<out.back().ranks.size()?20:35;
            out.back().ranks[r].restoreHpPercent=15;
        }
        t=attack("Mana Burst","With cloth or no armour, spend all your mana at once: everything within two tiles takes that much.",0,0,12,false,2);
        t.manaBurst=true; add(10,"cloth.burst",3,t);
        shape("cloth.burst",3,"capstone",{});
        add(11,"light_armour.sidestep",0,move("Sidestep","Requires light armour. Move up to two visible tiles, gaining Opening and triggering movement talents.",2,0,5));
        shape("light_armour.sidestep",3,"",{});
        add(11,"light_armour.evasion",2,passive("Agile Fit","With light armour and Opening from waiting or movement abilities, gain +12% dodge. Total dodge is capped at 75%.",PassiveKind::LightEvasion,12));
        shape("light_armour.evasion",1,"",{"light_armour.parting_strike"});
        add(11,"light_armour.precision",2,passive("Moving Aim","With light armour and Opening, all direct attacks gain +15% critical chance. Combines with Bow's Marksmanship.",PassiveKind::LightPrecision,15));
        shape("light_armour.precision",1,"",{"light_armour.feint"});
        t=attack("Parting Strike","Requires light armour. Strike and Mark an adjacent enemy, then retreat two tiles even on a miss. No weapon requirement.",6,3,6); t.retreatDistance=2; t.onHitEffect=StatusEffectInstance{StatusEffectType::Marked,3,1};
        add(11,"light_armour.parting_strike",1,t);
        shape("light_armour.parting_strike",3,"path",{"light_armour.sidestep"});
        t=attack("Feint","Requires light armour. A false blow that marks the foe and gives you Opening.",2,2,5);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Marked,3,1}; t.selfBuffEffect=StatusEffectInstance{StatusEffectType::Opening,2,0};
        add(11,"light_armour.feint",1,t);
        shape("light_armour.feint",3,"path",{"light_armour.sidestep"});
        t=buff("Blur","Requires light armour. +30% dodge and 30% faster for three turns.",StatusEffectType::Evasion,3,30,4,12);
        t.hasteSelf=30; add(11,"light_armour.blur",3,t);
        shape("light_armour.blur",3,"capstone",{});
        t=attack("Perfect Opening","Requires light armour. A strike that is always a critical hit while you have Opening.",8,4,8);
        t.critWithOpening=true; add(11,"light_armour.perfect",3,t);
        shape("light_armour.perfect",3,"capstone",{});
        t=attack("Shoulder Check","Requires heavy armour. Strike an adjacent enemy and push them one tile. No shield required.",5,2,4); t.pushDistance=1;
        add(12,"heavy_armour.shoulder_check",0,t);
        shape("heavy_armour.shoulder_check",3,"",{});
        add(12,"heavy_armour.brace",2,passive("Brace","With heavy armour and Opening from waiting or movement abilities, reduce direct hits by 3 damage. Stacks with Guard; does not block damage over time.",PassiveKind::HeavyBrace,3));
        shape("heavy_armour.brace",1,"",{"heavy_armour.second_wind"});
        add(12,"heavy_armour.resolve",2,passive("Unyielding","While wearing heavy armour, gain a 30% chance to resist an incoming Stun. Existing stuns are not removed.",PassiveKind::HeavyResolve,30));
        shape("heavy_armour.resolve",1,"",{"heavy_armour.fortify"});
        t=Talent{}; t.name="Second Wind"; t.description="Requires heavy armour. Spend a turn recovering 10/12/15/17/20% of maximum HP (rounded up). Costs 4/4/5/5/6 mana, cooldown 10.";
        t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.restoreHpPercent=10; t.manaCost=4; t.cooldownTurns=10;
        add(12,"heavy_armour.second_wind",1,t);
        shape("heavy_armour.second_wind",3,"path",{"heavy_armour.shoulder_check"});
        t=buff("Fortify","Requires heavy armour. Dig in: Guard 4 for four turns, but you move 25% slower while it holds.",StatusEffectType::Guard,4,4,3,9);
        t.slowSelf=25; add(12,"heavy_armour.fortify",1,t);
        shape("heavy_armour.fortify",3,"path",{"heavy_armour.shoulder_check"});
        t=attack("Juggernaut","Requires heavy armour. Charge up to four tiles at a foe, knock it two tiles back, and end the charge guarded (Guard 4).",8,4,9);
        t.chargeDistance=4; t.pushDistance=2; t.selfBuffEffect=StatusEffectInstance{StatusEffectType::Guard,2,4};
        add(12,"heavy_armour.juggernaut",3,t);
        shape("heavy_armour.juggernaut",3,"capstone",{});
        t=buff("Unbreakable","Requires heavy armour. Throw off pins, holds and slows, and stand guarded: Guard 8 for three turns.",StatusEffectType::Guard,3,8,4,12);
        t.shakeHolds=true; add(12,"heavy_armour.unbreakable",3,t);
        shape("heavy_armour.unbreakable",3,"capstone",{});
        t=buff("Imbue Weapon","V: choose an owned element, then bind 1-9. Five turns or three landed melee hits; all variants share rank and cooldown. Melee weapon required.",StatusEffectType::FlameBlade,6,3,4,8);
        add(13,"spellblade.imbue",0,t);
        shape("spellblade.imbue",3,"",{});
        t=attack("Spellstrike","INT-scaled melee spell. Triggers Kindle on this hit, Frostbite, Static Charge and Spellweave when learned.",7,6,4); t.spellstrike=true;
        add(13,"spellblade.strike",1,t);
        shape("spellblade.strike",3,"path",{"spellblade.imbue"});
        t=attack("Arcane Cleave","A spell-charged sweep through every foe beside you, powered by Intelligence.",6,5,6,false,1); t.spellstrike=true;
        add(13,"spellblade.cleave",1,t);
        shape("spellblade.cleave",3,"path",{"spellblade.imbue"});
        add(13,"spellblade.rhythm",2,passive("Battle Rhythm","Casting a spell grants +6 damage to your next melee attack. A landed melee attack reduces one running spell cooldown by one, once per action.",PassiveKind::BattleRhythm,6));
        shape("spellblade.rhythm",1,"",{"spellblade.strike"});
        add(13,"spellblade.ward",2,passive("Blade Ward","Each melee hit you land gives you 2 spell ward, more with Intelligence.",PassiveKind::BladeWard,2));
        shape("spellblade.ward",1,"",{"spellblade.cleave"});
        t=attack("Elemental Release","Consume Burn, Chill, Shock, Marked, Poison, Stun, Wither and Hunter's Mark on adjacent enemies: +25% damage per type consumed. Guard and buffs are preserved.",7,10,8,false,1); t.releaseAilments=true;
        add(13,"spellblade.release",3,t);
        shape("spellblade.release",3,"capstone",{});
        t=attack("Blink Strike","Flash up to five tiles along a clear line to a foe and strike it.",8,6,9); t.chargeDistance=5;
        add(13,"spellblade.blink_strike",3,t);
        shape("spellblade.blink_strike",3,"capstone",{});
        t=Talent{}; t.name="Raise Skeleton"; t.description="Raise an adjacent ally. Cap: 1 + INT/10, maximum 5. Rank and INT improve its stats. Allies dissolve on travel."; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.summonCount=1; t.manaCost=8; t.cooldownTurns=5;
        add(14,"animation.raise",0,t);
        shape("animation.raise",3,"",{});
        t=Talent{}; t.name="Bone Swap"; t.description="Swap with a visible allied skeleton. Empty ground still spends the cast. Counts as movement for your movement talents."; t.targeting=TargetingMode::RangedEnemyInSight; t.effectKind=TalentEffectKind::SelfBuff; t.boneSwap=true; t.manaCost=4; t.cooldownTurns=4;
        add(14,"animation.swap",1,t);
        shape("animation.swap",3,"path",{"animation.raise"});
        t=attack("Bone Spear","A spear of bone that runs through every foe in a line.",8,5,6,true); t.pierceAll=true;
        add(14,"animation.spear",1,t);
        shape("animation.spear",3,"path",{"animation.raise"});
        add(14,"animation.pact",2,passive("Grave Pact","Slain minions explode for 6 damage to adjacent enemies. Expiration, gear-cap dissolution and travel never explode.",PassiveKind::GravePact,6));
        shape("animation.pact",1,"",{"animation.swap"});
        add(14,"animation.bone_armour",2,passive("Bone Armour","Each skeleton you have makes hits on you deal 1 less.",PassiveKind::BoneArmour,1));
        shape("animation.bone_armour",1,"",{"animation.spear"});
        t=Talent{}; t.name="Army of the Dead"; t.description="Raise up to three adjacent temporary skeletons for 5/6/7/8/9 actions. They do not count toward the normal cap. Only one army at a time."; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.summonCount=3; t.summonDuration=5; t.manaCost=16; t.cooldownTurns=12;
        add(14,"animation.army",3,t);
        shape("animation.army",3,"capstone",{});
        t=Talent{}; t.name="Bone Prison"; t.description="Bone walls burst up on every free tile around a visible foe, trapping it where it stands.";
        t.targeting=TargetingMode::RangedEnemyInSight; t.effectKind=TalentEffectKind::SelfBuff; t.bonePrison=true; t.manaCost=8; t.cooldownTurns=14;
        add(14,"animation.prison",3,t);
        shape("animation.prison",3,"capstone",{});
        add(15,"blood_magic.pact",0,buff("Blood Pact","For several actions, spells spend HP instead of mana. A spell cannot spend your last HP. Activating this pact costs mana.",StatusEffectType::BloodPact,4,1,2,9));
        shape("blood_magic.pact",3,"",{});
        t=attack("Drain Life","Direct spell: heal for 40/50/60/70/80% of actual HP removed, excluding overkill. Empty ground or a miss gives no healing.",7,8,5,true); t.projectile=false; t.drainPercent=40;
        add(15,"blood_magic.drain",1,t);
        shape("blood_magic.drain",3,"path",{"blood_magic.pact"});
        t=attack("Blood Boil","Pay 6 life: the blood of every foe within two tiles boils. Each is struck and bleeds.",5,0,7,false,2);
        t.hpCost=6; t.onHitEffect=StatusEffectInstance{StatusEffectType::Bleed,3,2}; add(15,"blood_magic.boil",1,t);
        shape("blood_magic.boil",3,"path",{"blood_magic.pact"});
        add(15,"blood_magic.deathless",2,passive("Deathless","Once per newly explored floor, survive lethal damage at 1 HP. Revisiting floors or town does not recharge it.",PassiveKind::Deathless,1));
        shape("blood_magic.deathless",1,"",{"blood_magic.drain"});
        add(15,"blood_magic.transfusion",2,passive("Transfusion","Your hits on bleeding foes heal you 2, more with Intelligence.",PassiveKind::Transfusion,2));
        shape("blood_magic.transfusion",1,"",{"blood_magic.boil"});
        t=buff("Wither","Curse visible ground for 3/4/5/6/7 enemy turns. Each direct hit you land on the cursed enemy heals 2 HP, capped by HP actually removed.",StatusEffectType::Wither,3,2,8,9); t.targeting=TargetingMode::RangedEnemyInSight;
        add(15,"blood_magic.wither",3,t);
        shape("blood_magic.wither",3,"capstone",{});
        t=buff("Blood Rite","Pay 10 life: for four turns, every hit you land deals 8 more.",StatusEffectType::Empowered,4,8,0,12); t.hpCost=10;
        add(15,"blood_magic.rite",3,t);
        shape("blood_magic.rite",3,"capstone",{});
        t=attack("Shadow Shot","Bow shot requiring Concealment: 35/45/55/65/75% chance to remain concealed after firing. Detection and damage can still reveal you.",7,4,4,true); t.requiresStealth=true; t.stayHiddenPercent=35;
        add(16,"shadow_archer.shot",0,t);
        shape("shadow_archer.shot",3,"",{});
        t=buff("Hunter's Mark","Mark visible ground for 3 turns. That enemy's stealth detection chance is halved, with a 5% minimum when it can check. Casting reveals you.",StatusEffectType::HuntersMark,3,50,3,5); t.targeting=TargetingMode::RangedEnemyInSight; t.huntersMark=true;
        add(16,"shadow_archer.mark",1,t);
        shape("shadow_archer.mark",3,"path",{"shadow_archer.shot"});
        t=attack("Smoke Arrow","An arrow that bursts into smoke where it strikes: everything within a tile is blinded for two enemy turns, and you slip out of sight.",3,3,7,true,1);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Blinded,2,0}; t.selfBuffEffect=StatusEffectInstance{StatusEffectType::Concealed,2,2};
        add(16,"shadow_archer.smoke",1,t);
        shape("shadow_archer.smoke",3,"path",{"shadow_archer.shot"});
        add(16,"shadow_archer.unseen",2,passive("Unseen","A direct kill begun while Concealed refreshes concealment to 3 responses. At most once per Conceal cast; a qualifying kill can preserve your concealment.",PassiveKind::Unseen,3));
        shape("shadow_archer.unseen",1,"",{"shadow_archer.mark"});
        add(16,"shadow_archer.long_shadow",2,passive("Long Shadow","Your bow attacks deal +4 damage to foes four or more tiles away.",PassiveKind::LongShadow,4));
        shape("shadow_archer.long_shadow",1,"",{"shadow_archer.smoke"});
        t=attack("Death from Shadows","A heavy bow shot with +50% damage while Concealed. After firing, gain Concealment for two responses. Long cooldown; does not reset Unseen.",12,8,12,true); t.returnConcealed=true; t.selfBuffEffect=StatusEffectInstance{StatusEffectType::Concealed,2,1};
        add(16,"shadow_archer.death",3,t);
        shape("shadow_archer.death",3,"capstone",{});
        t=attack("Shadow Pin","An arrow through the foe's shadow: it is pinned and blinded for two enemy turns.",8,5,9,true);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Pinned,2,0}; t.secondHitEffect=StatusEffectInstance{StatusEffectType::Blinded,2,0};
        add(16,"shadow_archer.pin",3,t);
        shape("shadow_archer.pin",3,"capstone",{});
        // Brawling (STR, any weapon or none): putting enemies where they hurt.
        t=attack("Tackle","Charge up to 3/3/4/4/5 tiles in a straight line at an enemy, strike it and knock it back a tile. Counts as movement.",5,3,5);
        t.chargeDistance=3; t.pushDistance=1; add(17,"brawling.tackle",0,t);
        shape("brawling.tackle",3,"",{});
        t=attack("Grapple","Seize an adjacent enemy for three enemy turns: it can't walk away, and when you step, you drag it into the tile you left (through fire, water, anything). Bosses and champions are too massive to hold.",3,2,6);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Grappled,3,0}; add(17,"brawling.grapple",1,t);
        shape("brawling.grapple",3,"path",{"brawling.tackle"});
        t=attack("Haymaker","A huge punch that knocks an adjacent foe three tiles back: into walls, fire, each other or chasms.",8,3,7);
        t.pushDistance=3; add(17,"brawling.haymaker",1,t);
        shape("brawling.haymaker",3,"path",{"brawling.tackle"});
        add(17,"brawling.hard_landing",2,passive("Hard Landing","Creatures you push, drag or throw take +4 more damage when they crash into a wall, a fixture or another creature.",PassiveKind::HardLanding,4));
        shape("brawling.hard_landing",1,"",{"brawling.grapple"});
        add(17,"brawling.knockout",2,passive("Knockout","Foes you knock into walls, fixtures or other creatures are stunned.",PassiveKind::Knockout,1));
        shape("brawling.knockout",1,"",{"brawling.haymaker"});
        t=attack("Hurl","Heave an adjacent enemy over your shoulder: it lands up to 2/2/3/3/4 tiles behind you, crashing into whatever is there. A grappled enemy flies one tile further. Bosses and champions are too heavy to lift.",8,5,7);
        t.hurlDistance=2; add(17,"brawling.hurl",3,t);
        shape("brawling.hurl",3,"capstone",{});
        t=attack("Piledriver","Drive a foe into the floor: double damage if it can't fight back properly (held, stunned, pinned or blinded), and it is stunned.",9,5,9);
        t.backstab=true; t.onHitEffect=StatusEffectInstance{StatusEffectType::Stun,1,0}; add(17,"brawling.piledriver",3,t);
        shape("brawling.piledriver",3,"capstone",{});
        // Whip (DEX, needs a whip): reach and pull.
        t=attack("Lash","Strike an enemy up to two tiles away in a straight line and pull it one tile toward you, through fire, water or whatever lies between.",4,1,2);
        t.reach=2; t.pullDistance=1; add(18,"whip.lash",0,t);
        shape("whip.lash",3,"",{});
        t=attack("Trip","Crack the whip at the legs of an enemy up to two tiles away: it falls, stunned for one enemy turn. Bosses resist repeated stuns.",4,3,6);
        t.reach=2; t.onHitEffect=StatusEffectInstance{StatusEffectType::Stun,1,0}; add(18,"whip.trip",1,t);
        shape("whip.trip",3,"path",{"whip.lash"});
        t=attack("Disarm","Crack the whip across a foe's hand, up to two tiles away: its attacks miss 30% more often for three enemy turns.",4,3,6);
        t.reach=2; t.onHitEffect=StatusEffectInstance{StatusEffectType::Misfortune,3,30}; add(18,"whip.disarm",1,t);
        shape("whip.disarm",3,"path",{"whip.lash"});
        add(18,"whip.flay",2,passive("Flay","Whip attacks deal +4 damage to enemies that are stunned, held, blinded, burning, chilled or shocked.",PassiveKind::Flay,4));
        shape("whip.flay",1,"",{"whip.trip"});
        add(18,"whip.taskmaster",2,passive("Taskmaster","Foes you pull toward you are marked: their next direct hit taken deals +25%.",PassiveKind::Taskmaster,1));
        shape("whip.taskmaster",1,"",{"whip.disarm"});
        t=attack("Snare","Lasso an enemy up to 3/3/4/4/5 tiles away, drag it right up to you and hold it fast for two turns (as Grapple). Bosses and champions won't budge.",6,4,7);
        t.reach=3; t.pullDistance=3; t.onHitEffect=StatusEffectInstance{StatusEffectType::Grappled,2,0}; add(18,"whip.snare",3,t);
        shape("whip.snare",3,"capstone",{});
        t=attack("Whirling Lash","Spin the whip around you: everything within two tiles is struck and dragged a tile toward you.",5,5,8,false,2);
        t.vortex=true; add(18,"whip.whirl",3,t);
        shape("whip.whirl",3,"capstone",{});
        // Shadow (INT spells): darkness as a weapon.
        t=attack("Shadow Bolt","A bolt of darkness: +50% damage against a target standing in darkness.",5,2,1,true);
        t.darkBonusPercent=50; add(19,"shadow.bolt",0,t);
        shape("shadow.bolt",3,"",{});
        t=Talent{}; t.name="Snuff"; t.description="Every torch, brazier, wisp and burning tile within four tiles goes out, your own light too (L relights it). Creatures that need light lose sight of you.";
        t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.snuffRadius=4; t.manaCost=2; t.cooldownTurns=6;
        add(19,"shadow.snuff",1,t);
        shape("shadow.snuff",3,"path",{"shadow.bolt"});
        t=move("Shadow Step","Step through the dark to any unlit tile up to five tiles away, past anything in the way.",5,4,6);
        t.vault=true; t.darkLanding=true; add(19,"shadow.step",1,t);
        shape("shadow.step",3,"path",{"shadow.bolt"});
        add(19,"shadow.umbral",2,passive("Umbral Shroud","While your own light is out, your spells deal +4 damage and you gain 12% dodge.",PassiveKind::Umbral,4));
        shape("shadow.umbral",1,"",{"shadow.snuff"});
        add(19,"shadow.dread",2,passive("Dread","Blinded foes take +3 damage from all your hits.",PassiveKind::Dread,3));
        shape("shadow.dread",1,"",{"shadow.step"});
        t=attack("Veil of Night","Darkness swallows an enemy and those beside it: blinded for three enemy turns, they see only what is next to them.",4,5,8,true,1);
        t.projectile=false; t.onHitEffect=StatusEffectInstance{StatusEffectType::Blinded,3,0}; add(19,"shadow.veil",3,t);
        shape("shadow.veil",3,"capstone",{});
        t=attack("Devour","A bolt of hunger: half the damage it deals returns to you as life, and it bites harder at a foe in darkness (+50%).",9,7,8,true);
        t.drainPercent=50; t.darkBonusPercent=50; add(19,"shadow.devour",3,t);
        shape("shadow.devour",3,"capstone",{});
        // Radiance (INT spells): light as a weapon.
        t=attack("Sear","A ray of light: +50% damage against undead and creatures that see in the dark.",5,2,1,true);
        t.searing=true; add(20,"radiance.sear",0,t);
        shape("radiance.sear",3,"",{});
        t=attack("Flare","A blinding burst: enemies within a tile are blinded for two enemy turns, hidden ones are revealed, and the spot stays lit for a while.",3,4,6,true,1);
        t.projectile=false; t.flare=true; t.onHitEffect=StatusEffectInstance{StatusEffectType::Blinded,2,0}; add(20,"radiance.flare",1,t);
        shape("radiance.flare",3,"path",{"radiance.sear"});
        t=Talent{}; t.name="Holy Light"; t.description="A warm light washes over you, restoring 20% of your maximum life.";
        t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.restoreHpPercent=20; t.manaCost=5; t.cooldownTurns=10;
        add(20,"radiance.holy_light",1,t);
        shape("radiance.holy_light",3,"path",{"radiance.sear"});
        add(20,"radiance.inner_light",2,passive("Inner Light","Your light reaches one tile further, and your spells deal +4 damage to enemies standing in light.",PassiveKind::InnerLight,4));
        shape("radiance.inner_light",1,"",{"radiance.flare"});
        add(20,"radiance.halo",2,passive("Halo","While your light burns, foes beside you are seared for 3 each turn.",PassiveKind::LanternWard,3));
        shape("radiance.halo",1,"",{"radiance.holy_light"});
        t=attack("Dawn","Light floods out three tiles, searing enemies (+50% against undead and darkvision), relighting every torch and brazier within six tiles and breaking any smothering darkness on you.",7,8,12,false,3);
        t.searing=true; t.dawn=true; add(20,"radiance.dawn",3,t);
        shape("radiance.dawn",3,"capstone",{});
        t=attack("Judgement","A pillar of light falls on one foe: a heavy, searing blow (+50% against undead and darkvision) that blinds it.",14,9,9,true);
        t.projectile=false; t.searing=true; t.onHitEffect=StatusEffectInstance{StatusEffectType::Blinded,2,0}; add(20,"radiance.judgement",3,t);
        shape("radiance.judgement",3,"capstone",{});
        // Alchemy (DEX): flasks that leave something on the ground.
        t=attack("Oil Flask","Lob a flask that splashes oil over a tile and its neighbours. Oil burns long once lit.",2,2,4,false,1);
        t.targeting=TargetingMode::RangedEnemyInSight; t.shape=EffectShape::AreaAroundTarget; t.splashSurface=1; add(21,"alchemy.oil",0,t);
        shape("alchemy.oil",3,"",{});
        t=attack("Firebomb","A flask of fire: burns everything within a tile for 1 per turn and sets the ground alight.",5,4,6,false,1);
        t.targeting=TargetingMode::RangedEnemyInSight; t.shape=EffectShape::AreaAroundTarget; t.splashSurface=3; t.splashTurns=3;
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,3,1}; add(21,"alchemy.firebomb",1,t);
        shape("alchemy.firebomb",3,"path",{"alchemy.oil"});
        t=attack("Frost Flask","A flask of ice: splashes ice over a tile and its neighbours for six turns, chilling what stands there.",3,3,6,false,1);
        t.targeting=TargetingMode::RangedEnemyInSight; t.shape=EffectShape::AreaAroundTarget; t.splashSurface=4; t.splashTurns=6;
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Chill,3,20}; add(21,"alchemy.frost",1,t);
        shape("alchemy.frost",3,"path",{"alchemy.oil"});
        add(21,"alchemy.brews",2,passive("Potent Brews","Your flasks deal +4 damage.",PassiveKind::PotentBrews,4));
        shape("alchemy.brews",1,"",{"alchemy.firebomb"});
        add(21,"alchemy.volatile",2,passive("Volatile Mix","The ground your flasks leave lasts three turns longer.",PassiveKind::VolatileMix,3));
        shape("alchemy.volatile",1,"",{"alchemy.frost"});
        t=attack("Acid Flask","Splash acid within a tile for eight turns: anything standing in it takes 2 a turn and is Marked (its next direct hit taken deals +25%).",4,5,8,false,1);
        t.targeting=TargetingMode::RangedEnemyInSight; t.shape=EffectShape::AreaAroundTarget; t.splashSurface=7; t.splashTurns=8; add(21,"alchemy.acid",3,t);
        shape("alchemy.acid",3,"capstone",{});
        t=attack("Greek Fire","A flask of fire that won't go out: everything within two tiles is set ablaze for six turns.",6,7,10,false,2);
        t.targeting=TargetingMode::RangedEnemyInSight; t.shape=EffectShape::AreaAroundTarget; t.splashSurface=3; t.splashTurns=6;
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,3,2}; add(21,"alchemy.greek_fire",3,t);
        shape("alchemy.greek_fire",3,"capstone",{});
        // Spear (STR, two hands): reach and footing.
        t=attack("Thrust","Strike an enemy up to two tiles away in a straight line; the point runs on into anyone standing right behind it.",5,1,2);
        t.reach=2; t.pierceBehind=true; add(22,"spear.thrust",0,t);
        shape("spear.thrust",3,"",{});
        t=buff("Brace","Plant the spear for two enemy responses: anything that steps up beside you is struck first, for 6 damage.",StatusEffectType::Braced,2,6,2,6);
        add(22,"spear.brace",1,t);
        shape("spear.brace",3,"path",{"spear.thrust"});
        t=attack("Impale","A reaching thrust, up to two tiles, that pins the foe for two enemy turns.",6,3,6);
        t.reach=2; t.onHitEffect=StatusEffectInstance{StatusEffectType::Pinned,2,0}; add(22,"spear.impale",1,t);
        shape("spear.impale",3,"path",{"spear.thrust"});
        add(22,"spear.long_reach",2,passive("Long Reach","Spear attacks deal +4 damage to enemies two tiles away.",PassiveKind::LongReach,4));
        shape("spear.long_reach",1,"",{"spear.brace"});
        add(22,"spear.skewer",2,passive("Skewer","Your attacks deal +4 damage to pinned foes.",PassiveKind::Skewer,4));
        shape("spear.skewer",1,"",{"spear.impale"});
        t=move("Pole Vault","Vault up to three tiles in a line, over enemies, chasms and burning ground. Walls still stop you.",3,3,7);
        t.vault=true; add(22,"spear.vault",3,t);
        shape("spear.vault",3,"capstone",{});
        t=attack("Spear Throw","Hurl your spear through every foe in a line.",9,4,8,true);
        t.pierceAll=true; add(22,"spear.throw",3,t);
        shape("spear.throw",3,"capstone",{});
        // Daggers (DEX): wounds and the helpless.
        t=attack("Lacerate","A quick cut that bleeds: 2 damage per turn for three enemy turns, and the living leave a trail of blood.",4,1,2);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Bleed,3,2}; add(23,"daggers.lacerate",0,t);
        shape("daggers.lacerate",3,"",{});
        t=attack("Backstab","Double damage against a foe that can't fight back properly: you are hidden, or it is blinded, stunned, held or pinned.",8,3,5);
        t.backstab=true; add(23,"daggers.backstab",1,t);
        shape("daggers.backstab",3,"path",{"daggers.lacerate"});
        t=attack("Throwing Knife","Throw a dagger at a foe in sight: it bleeds for 2 a turn over three turns.",5,2,3,true);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Bleed,3,2}; add(23,"daggers.throw",1,t);
        shape("daggers.throw",3,"path",{"daggers.lacerate"});
        add(23,"daggers.ambush",2,passive("Ambush","Direct damage from concealment gains +6, spells and ranged attacks included.",PassiveKind::Ambush,6));
        shape("daggers.ambush",1,"",{"daggers.backstab"});
        add(23,"daggers.hemorrhage",2,passive("Hemorrhage","Your attacks deal +4 damage to bleeding enemies, and bleeding you cause lasts two turns longer.",PassiveKind::Hemorrhage,4));
        shape("daggers.hemorrhage",1,"",{"daggers.throw"});
        t=attack("Assassinate","A strike from hiding: triple damage against a foe below half its life.",9,5,9); t.requiresStealth=true;
        t.conditionalHpFraction=.5f; t.conditionalMultiplier=3; add(23,"daggers.assassinate",3,t);
        shape("daggers.assassinate",3,"capstone",{});
        t=attack("Eviscerate","Tear the wound open: the target's whole bleed comes due at once, doubled.",6,4,8); t.consumeBleed=true;
        add(23,"daggers.eviscerate",3,t);
        shape("daggers.eviscerate",3,"capstone",{});
        // Warbanner (STR), the first deep tree: Steel 8, Guard 6, level 10, and the Warlord's standard.
        t=Talent{}; t.name="Plant the Standard"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="Plant your banner on a tile beside you for 8 turns. Within two tiles of it you gain 3 Guard each turn and can't be slowed.";
        t.manaCost=4; t.cooldownTurns=10; t.plantBanner=8; add(37,"warbanner.plant",0,t);
        shape("warbanner.plant",3,"",{});
        t=Talent{}; t.name="Rally Cry"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="Foes within two tiles of you are Shaken for three turns: they deal 25% less damage.";
        t.manaCost=4; t.cooldownTurns=8; t.rallyCry=true; add(37,"warbanner.rally",1,t);
        shape("warbanner.rally",3,"path",{"warbanner.plant"});
        t=attack("Standard Bash","Strike with the banner pole and knock the foe two tiles back. If it crashes into another foe, both are stunned.",6,3,5);
        t.pushDistance=2; t.crashStun=1; add(37,"warbanner.bash",1,t);
        shape("warbanner.bash",3,"path",{"warbanner.plant"});
        add(37,"warbanner.hold",2,passive("Hold the Line","Within two tiles of your banner you can't be moved or stunned, and direct hits deal 2 less.",PassiveKind::HoldTheLine,2));
        shape("warbanner.hold",1,"",{"warbanner.rally"});
        add(37,"warbanner.ranks",2,passive("Break Their Ranks","Your hits deal +2 for each other foe in the 8 tiles around your target.",PassiveKind::BreakRanks,2));
        shape("warbanner.ranks",1,"",{"warbanner.bash"});
        t=Talent{}; t.name="Last Banner"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="Raise a great standard for 6 turns: foes within three tiles of it are slowed, and you are hastened while within it.";
        t.manaCost=8; t.cooldownTurns=16; t.plantBanner=6; t.greatBanner=true; add(37,"warbanner.last",3,t);
        shape("warbanner.last",3,"capstone",{});
        t=move("Warlord's Charge","Charge up to five tiles, striking every foe along the way and throwing them aside. Your banner plants where you stop.",5,6,10);
        t.blitz=true; t.knockAside=true; t.power=6; t.plantBanner=8; add(37,"warbanner.charge",3,t);
        shape("warbanner.charge",3,"capstone",{});
        // Forgeborn (STR), the Ashen Foundry's deep tree: Steel 8, Flame 6, level 10, and the Forgemaster's brand.
        t=Talent{}; t.name="Stoke"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="Draw the fire into your armour: gain 4 Heat, and Guard 3 for two enemy responses.";
        t.selfBuffEffect=StatusEffectInstance{StatusEffectType::Guard,2,3}; t.gainHeat=4; t.manaCost=2; t.cooldownTurns=5; add(38,"forgeborn.stoke",0,t);
        shape("forgeborn.stoke",3,"",{});
        t=attack("Searing Blow","Spend all your Heat on one blow: +3 damage for each point, and the target burns.",5,3,4);
        t.spendHeat=3; add(38,"forgeborn.searing",1,t);
        shape("forgeborn.searing",3,"path",{"forgeborn.stoke"});
        t=Talent{}; t.name="Molten Plate"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="For 4 turns, foes that strike you in melee take 3 fire damage, and each blow heats you by 1.";
        t.moltenPlate=4; t.manaCost=4; t.cooldownTurns=10; add(38,"forgeborn.plate",1,t);
        shape("forgeborn.plate",3,"path",{"forgeborn.stoke"});
        add(38,"forgeborn.tempered",2,passive("Tempered","While you hold 4 Heat or more, your melee hits deal +3.",PassiveKind::Tempered,3));
        shape("forgeborn.tempered",1,"",{"forgeborn.searing"});
        add(38,"forgeborn.heat_sink",2,passive("Heat Sink","Heat never burns you, and every 3 Heat you hold blunts direct hits on you by 1.",PassiveKind::HeatSink,1));
        shape("forgeborn.heat_sink",1,"",{"forgeborn.plate"});
        t=attack("Vent","Release all your Heat in a blast within two tiles: 2 damage for each point, and the ground there burns.",4,4,8,false,2);
        t.ventHeat=2; add(38,"forgeborn.vent",3,t);
        shape("forgeborn.vent",3,"capstone",{});
        t=Talent{}; t.name="Forgeheart"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="For 5 turns your Heat can't fall and you are hastened. When it ends, your Heat vents in a blast within two tiles.";
        t.selfBuffEffect=StatusEffectInstance{StatusEffectType::Hasted,5,25}; t.forgeheart=5; t.manaCost=8; t.cooldownTurns=16; add(38,"forgeborn.forgeheart",3,t);
        shape("forgeborn.forgeheart",3,"capstone",{});
        // Slagcaller (INT), the Foundry's second deep tree: Earth 8, Flame 6, level 12, and the slag formula.
        t=attack("Slag Pool","Pour molten slag over a 3 by 3 patch in sight: it burns, and foes standing in it are slowed for three turns.",3,5,6,true,1);
        t.slagPool=true; add(39,"slagcaller.pool",0,t);
        shape("slagcaller.pool",3,"",{});
        t=Talent{}; t.name="Raise Slagling"; t.description="A slagling crawls up beside you and fights for 10 turns. When it dies it bursts into flame.";
        t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.summonCount=1; t.summonDuration=10; t.summonKind=static_cast<int>(MonsterType::Slagling);
        t.manaCost=6; t.cooldownTurns=6; add(39,"slagcaller.slagling",1,t);
        shape("slagcaller.slagling",3,"path",{"slagcaller.pool"});
        t=attack("Cinder Hail","Cinders fall on everything within a tile of a spot for three turns, burning what stands there.",5,6,8,true,1);
        t.projectile=false; t.lingerTurns=2; t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,3,2}; add(39,"slagcaller.hail",1,t);
        shape("slagcaller.hail",3,"path",{"slagcaller.pool"});
        add(39,"slagcaller.brittle",2,passive("Brittle Slag","When one of your slaglings dies, it splits into two smaller ones, once.",PassiveKind::BrittleSlag,1));
        shape("slagcaller.brittle",1,"",{"slagcaller.slagling"});
        add(39,"slagcaller.pyroclasm",2,passive("Pyroclasm","Fire on the ground also slows the foes standing in it by 20%.",PassiveKind::Pyroclasm,20));
        shape("slagcaller.pyroclasm",1,"",{"slagcaller.hail"});
        t=Talent{}; t.name="Slag Golem"; t.description="Raise a slow, tough slag golem beside you for 20 turns. When it dies it splits into two slaglings.";
        t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.summonCount=1; t.summonDuration=20; t.summonKind=static_cast<int>(MonsterType::SlagGolem);
        t.manaCost=12; t.cooldownTurns=18; add(39,"slagcaller.golem",3,t);
        shape("slagcaller.golem",3,"capstone",{});
        t=attack("Eruption","The ground under a foe in sight erupts: heavy damage to everything beside it, the ground around burns, and the target is stunned.",12,10,10,true,1);
        t.projectile=false; t.eruption=true; add(39,"slagcaller.eruption",3,t);
        shape("slagcaller.eruption",3,"capstone",{});
        // Tempest (INT), the Cathedral's deep tree: Storm 10, level 12, and the Drowned Chorister's hymn.
        t=Talent{}; t.name="Stormcall"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="A storm hangs over you for 4 turns. Each turn it strikes the nearest foe within 3 tiles with lightning that shocks.";
        t.stormcall=4; t.manaCost=6; t.cooldownTurns=10; add(40,"tempest.stormcall",0,t);
        shape("tempest.stormcall",3,"",{});
        t=attack("Forked Bolt","A bolt that leaps on to two more foes near the first, each at half damage. Terrain blocks the leap.",7,5,5,true);
        t.chain=true; t.chainJumps=2; t.onHitEffect=StatusEffectInstance{StatusEffectType::Shock,3,0}; add(40,"tempest.forked",1,t);
        shape("tempest.forked",3,"path",{"tempest.stormcall"});
        t=Talent{}; t.name="Static Field"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="The ground in the 8 tiles around you crackles with lightning for 3 turns, shocking whatever stands in it.";
        t.staticField=1; t.manaCost=6; t.cooldownTurns=8; add(40,"tempest.static",1,t);
        shape("tempest.static",3,"path",{"tempest.stormcall"});
        add(40,"tempest.overcharge",2,passive("Overcharge","Your lightning hits on shocked foes deal +3.",PassiveKind::Overcharge,3));
        shape("tempest.overcharge",1,"",{"tempest.forked"});
        add(40,"tempest.eye",2,passive("Eye of the Storm","Electrified ground can't harm you, and you are 15% faster while you stand on it.",PassiveKind::EyeOfTheStorm,15));
        shape("tempest.eye",1,"",{"tempest.static"});
        t=attack("Thunderhead","A great storm gathers over a spot in sight for 4 turns, striking everything within 2 tiles of it as each of your turns begins.",4,9,12,true,2);
        t.projectile=false; t.lingerTurns=3; t.onHitEffect=StatusEffectInstance{StatusEffectType::Shock,3,0}; add(40,"tempest.thunderhead",3,t);
        shape("tempest.thunderhead",3,"capstone",{});
        t=move("Ride the Lightning","Flash up to 5 tiles in a line, striking and shocking everything beside your path.",5,6,8);
        t.blitz=true; t.power=6; t.onHitEffect=StatusEffectInstance{StatusEffectType::Shock,3,0}; add(40,"tempest.ride",3,t);
        shape("tempest.ride",3,"capstone",{});
        // Bonewright (INT), the Crypts' deep tree: Death 8, Earth 4, level 12, and a Bonecaller's journal.
        add(41,"bonewright.armour",0,buff("Bone Armour","Wrap yourself in bone: Guard 4 for four enemy responses.",StatusEffectType::Guard,4,4,4,8));
        shape("bonewright.armour",3,"",{});
        t=Talent{}; t.name="Bone Wall"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="Raise a wall of 3 bone pillars across your path, two tiles ahead. It crumbles after 6 turns.";
        t.boneWall=3; t.manaCost=6; t.cooldownTurns=9; add(41,"bonewright.wall",1,t);
        shape("bonewright.wall",3,"path",{"bonewright.armour"});
        t=Talent{}; t.name="Ossuary Guard"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="Raise a bone guardian beside you. It stays until it falls, and counts toward your minions.";
        t.summonCount=1; t.summonKind=static_cast<int>(MonsterType::SkeletonGuard); t.manaCost=10; t.cooldownTurns=12; add(41,"bonewright.guard",1,t);
        shape("bonewright.guard",3,"path",{"bonewright.armour"});
        add(41,"bonewright.marrow",2,passive("Marrow","When a foe dies within 2 tiles of you, you gain 2 Guard.",PassiveKind::Marrow,2));
        shape("bonewright.marrow",1,"",{"bonewright.wall"});
        add(41,"bonewright.grown",2,passive("Grown Guard","Your bone guardian draws on your mind: +1 Strength and +2 life for every 5 Intelligence you have.",PassiveKind::GrownGuard,1));
        shape("bonewright.grown",1,"",{"bonewright.guard"});
        t=Talent{}; t.name="Bone Storm"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="Bone shards whirl around you for 3 turns: each turn the foes beside you take 4 and bleed.";
        t.boneStorm=3; t.manaCost=8; t.cooldownTurns=12; add(41,"bonewright.storm",3,t);
        shape("bonewright.storm",3,"capstone",{});
        t=Talent{}; t.name="Bone Lord"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="For 5 turns your minions are hastened and deal +3.";
        t.boneLord=5; t.manaCost=8; t.cooldownTurns=14; add(41,"bonewright.lord",3,t);
        shape("bonewright.lord",3,"capstone",{});
        // Rimeheart (INT), the Crypts' second deep tree: Frost 8, Water 4, level 12, and a Frost Acolyte's catechism.
        t=attack("Rime","Freeze a 3 by 3 patch in sight into ice, chilling whoever stands there.",3,4,5,true,1);
        t.projectile=false; t.rime=true; t.onHitEffect=StatusEffectInstance{StatusEffectType::Chill,2,20}; add(42,"rimeheart.rime",0,t);
        shape("rimeheart.rime",3,"",{});
        t=attack("Shatter Lance","A lance of ice: double damage to a foe standing on ice, and the ice around it shatters, cutting those beside it.",7,5,5,true);
        t.onIceDouble=true; t.shatterIce=true; add(42,"rimeheart.lance",1,t);
        shape("rimeheart.lance",3,"path",{"rimeheart.rime"});
        t=move("Glacial Path","Slide up to 4 tiles, leaving ice behind you.",4,4,6); t.iceTrail=true; add(42,"rimeheart.path",1,t);
        shape("rimeheart.path",3,"path",{"rimeheart.rime"});
        add(42,"rimeheart.brittle",2,passive("Brittle Cold","Chilled foes take +3 from your hits.",PassiveKind::BrittleCold,3));
        shape("rimeheart.brittle",1,"",{"rimeheart.lance"});
        add(42,"rimeheart.creeping",2,passive("Creeping Frost","Ice you make spreads a tile each turn, for 3 turns.",PassiveKind::CreepingFrost,3));
        shape("rimeheart.creeping",1,"",{"rimeheart.path"});
        t=Talent{}; t.name="Deep Freeze"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="Water within 3 tiles freezes solid, and every foe there standing on ice or in water is frozen in place for 2 turns.";
        t.deepFreeze=3; t.manaCost=10; t.cooldownTurns=14; add(42,"rimeheart.freeze",3,t);
        shape("rimeheart.freeze",3,"capstone",{});
        t=Talent{}; t.name="Winter's Heart"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="Ice encases you for 3 turns: you can't act, but nothing can hurt or move you. Then it bursts: everything within 2 tiles takes heavy frost damage and is chilled, and the ground turns to ice.";
        t.wintersHeart=3; t.manaCost=8; t.cooldownTurns=18; add(42,"rimeheart.heart",3,t);
        shape("rimeheart.heart",3,"capstone",{});
        // Briarheart (DEX), Thornwood Hollow's deep tree: Rot 8, Hunt 6, level 14, and a Rot Witch's seed.
        t=attack("Seed the Briar","Throw a seed anywhere in sight: thorns grow on a 3 by 3 patch there, and whoever stands in it starts to bleed.",3,4,5,true,1);
        t.projectile=false; t.briarSeed=true; t.onHitEffect=StatusEffectInstance{StatusEffectType::Bleed,3,2}; add(43,"briarheart.seed",0,t);
        shape("briarheart.seed",3,"",{});
        t=attack("Bramble Lash","A thorned whip that strikes a foe up to 3 tiles away in a straight line. Against a foe standing in thorns it hits twice as hard and pins it for a turn.",5,3,3);
        t.reach=3; t.onThornsDouble=true; t.lashPin=1; add(43,"briarheart.lash",1,t);
        shape("briarheart.lash",3,"path",{"briarheart.seed"});
        t=Talent{}; t.name="Blood Briar"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="Pay 6 life: thorns burst from the 8 tiles around you, and every foe on them is pinned for a turn.";
        t.hpCost=6; t.bloodBriar=1; t.manaCost=4; t.cooldownTurns=8; add(43,"briarheart.blood",1,t);
        shape("briarheart.blood",3,"path",{"briarheart.seed"});
        add(43,"briarheart.thornborn",2,passive("Thornborn","Thorns no longer cut or slow you, and while you stand in them direct hits deal 3 less.",PassiveKind::Thornborn,3));
        shape("briarheart.thornborn",1,"",{"briarheart.lash"});
        add(43,"briarheart.snares",2,passive("Briar Snares","When one of your traps goes off, thorns grow on the 8 tiles around it.",PassiveKind::BriarSnares,1));
        shape("briarheart.snares",1,"",{"briarheart.blood"});
        t=Talent{}; t.name="Overgrowth"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="For 6 turns your thorns spread a tile each turn, never more than 4 tiles from you. A foe they reach is pinned, once.";
        t.overgrowth=6; t.manaCost=10; t.cooldownTurns=16; add(43,"briarheart.overgrowth",3,t);
        shape("briarheart.overgrowth",3,"capstone",{});
        t=Talent{}; t.name="Heart of Briars"; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
        t.description="For 8 turns, whatever hits you has thorns grow beneath it and bleeds 3.";
        t.heartOfBriars=8; t.manaCost=8; t.cooldownTurns=18; add(43,"briarheart.heart",3,t);
        shape("briarheart.heart",3,"capstone",{});
        // Mace (STR, one hand): guards and bones.
        t=attack("Crush","A heavy blow that sunders: the target takes +2 damage from every hit for four enemy turns.",6,2,4);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Sundered,4,2}; add(24,"mace.crush",0,t);
        shape("mace.crush",3,"",{});
        t=attack("Stagger","Knock an enemy off balance: an attack it is winding up is delayed by two actions.",5,2,5);
        t.stagger=2; add(24,"mace.stagger",1,t);
        shape("mace.stagger",3,"path",{"mace.crush"});
        t=attack("Ground Slam","Smash the ground: every foe beside you is struck and sundered (+2 from every hit) for four enemy turns.",5,4,7,false,1);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Sundered,4,2}; add(24,"mace.slam",1,t);
        shape("mace.slam",3,"path",{"mace.crush"});
        add(24,"mace.bonebreaker",2,passive("Bonebreaker","With a mace, your attacks deal +4 damage to skeletons, spirits and other bloodless foes.",PassiveKind::Bonebreaker,4));
        shape("mace.bonebreaker",1,"",{"mace.stagger"});
        add(24,"mace.concussion",2,passive("Concussion","Your attacks deal +30% damage to stunned foes.",PassiveKind::CrushingBlows,30));
        shape("mace.concussion",1,"",{"mace.slam"});
        t=attack("Shatter","Consume Chill for double damage, and break the ice around the target into shards that cut everyone standing on it (4).",9,5,7);
        t.consumeChill=true; t.statusBonusPercent=100; t.shatterIce=true; add(24,"mace.shatter",3,t);
        shape("mace.shatter",3,"capstone",{});
        t=attack("Skullcracker","A crushing blow to the head: stuns for two enemy turns. Bosses resist repeated stuns.",10,5,10);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Stun,2,0}; add(24,"mace.skullcracker",3,t);
        shape("mace.skullcracker",3,"capstone",{});
        // Crossbow (DEX, two hands): heavy bolts.
        t=attack("Heavy Bolt","A heavy bolt that knocks its target back a tile: into fire, walls, foes or chasms.",8,2,3,true);
        t.pushDistance=1; add(25,"crossbow.heavy",0,t);
        shape("crossbow.heavy",3,"",{});
        t=attack("Piercing Bolt","A bolt that passes through every enemy in its line.",7,4,6,true);
        t.pierceAll=true; add(25,"crossbow.pierce",1,t);
        shape("crossbow.pierce",3,"path",{"crossbow.heavy"});
        t=attack("Explosive Bolt","A bolt that bursts where it strikes, scorching everything within a tile and setting oil alight.",6,4,6,true,1);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,2,1}; add(25,"crossbow.explosive",1,t);
        shape("crossbow.explosive",3,"path",{"crossbow.heavy"});
        add(25,"crossbow.windlass",2,passive("Windlass","Crossbow attacks deal +4 damage while you have Opening (from waiting or moving).",PassiveKind::Windlass,4));
        shape("crossbow.windlass",1,"",{"crossbow.pierce"});
        add(25,"crossbow.heavy_draw",2,passive("Heavy Draw","Your crossbow bolts knock foes back one tile further.",PassiveKind::HeavyDraw,1));
        shape("crossbow.heavy_draw",1,"",{"crossbow.explosive"});
        t=attack("Pinning Shot","A bolt that pins its target in place: it can't move for three enemy turns (it can still fight).",7,4,7,true);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Pinned,3,0}; add(25,"crossbow.pin",3,t);
        shape("crossbow.pin",3,"capstone",{});
        t=attack("Ballista Bolt","An enormous bolt that tears through a line of foes and knocks each one back two tiles.",12,7,10,true);
        t.pierceAll=true; t.pushDistance=2; add(25,"crossbow.ballista",3,t);
        shape("crossbow.ballista",3,"capstone",{});
        // Earth (INT spells): shape the ground.
        t=attack("Stone Spike","A spike of rock erupts under a visible foe and pins it in place for two enemy turns.",6,2,3,true);
        t.projectile=false; t.onHitEffect=StatusEffectInstance{StatusEffectType::Pinned,2,0}; add(26,"earth.spike",0,t);
        shape("earth.spike",3,"",{});
        t=Talent{}; t.name="Raise Pillar"; t.description="A stone pillar rises on empty visible ground for 12 turns: block a corridor, break a line of fire, or shove foes into it.";
        t.targeting=TargetingMode::RangedEnemyInSight; t.effectKind=TalentEffectKind::SelfBuff; t.raisePillar=true; t.manaCost=3; t.cooldownTurns=10;
        add(26,"earth.pillar",1,t);
        shape("earth.pillar",3,"path",{"earth.spike"});
        t=attack("Grasping Earth","The ground seizes everything within a tile of a spot: pinned for two enemy turns.",3,4,7,true,1);
        t.projectile=false; t.onHitEffect=StatusEffectInstance{StatusEffectType::Pinned,2,0}; add(26,"earth.grasp",1,t);
        shape("earth.grasp",3,"path",{"earth.spike"});
        add(26,"earth.stoneskin",2,passive("Stoneskin","Direct hits on you deal 2 less damage.",PassiveKind::Stoneskin,2));
        shape("earth.stoneskin",1,"",{"earth.pillar"});
        add(26,"earth.aftershock",2,passive("Aftershock","Whenever you pin a foe, the foes beside it take 3.",PassiveKind::Aftershock,3));
        shape("earth.aftershock",1,"",{"earth.grasp"});
        t=attack("Quake","The ground bucks: everything within two tiles is struck and thrown back a tile.",7,6,8,false,2);
        t.pushDistance=1; add(26,"earth.quake",3,t);
        shape("earth.quake",3,"capstone",{});
        t=attack("Boulder","A boulder rolls down a line, striking and shoving everything in its path.",8,7,9,true);
        t.pierceAll=true; t.pushDistance=1; add(26,"earth.boulder",3,t);
        shape("earth.boulder",3,"capstone",{});
        // Tide (INT spells): water as a weapon.
        t=attack("Water Bolt","A bolt of water that floods the tile it strikes.",4,2,1,true);
        t.splashSurface=2; add(27,"tide.bolt",0,t);
        shape("tide.bolt",3,"",{});
        t=attack("Wave","A wave rolls down a line, shoving every enemy in it back two tiles and leaving water behind.",4,4,6,true);
        t.pierceAll=true; t.pushDistance=2; t.splashSurface=2; t.splashPath=true; add(27,"tide.wave",1,t);
        shape("tide.wave",3,"path",{"tide.bolt"});
        t=attack("Undertow","The water drags a foe in a straight line up to three tiles toward you.",4,3,6);
        t.reach=4; t.pullDistance=3; add(27,"tide.undertow",1,t);
        shape("tide.undertow",3,"path",{"tide.bolt"});
        add(27,"tide.riptide",2,passive("Riptide","Your spells deal +4 damage to enemies standing in water or blood.",PassiveKind::Riptide,4));
        shape("tide.riptide",1,"",{"tide.wave"});
        add(27,"tide.tidecaller",2,passive("Tidecaller","Standing in water heals you 2 each turn.",PassiveKind::Tidecaller,2));
        shape("tide.tidecaller",1,"",{"tide.undertow"});
        t=attack("Maelstrom","Floods everything within two tiles of a spot, chills it, and drags every enemy there a tile toward the centre.",5,7,9,true,2);
        t.projectile=false; t.splashSurface=2; t.vortex=true; t.onHitEffect=StatusEffectInstance{StatusEffectType::Chill,3,30}; add(27,"tide.maelstrom",3,t);
        shape("tide.maelstrom",3,"capstone",{});
        t=attack("Flood","Water floods out from you over everything within three tiles.",3,8,10,false,3);
        t.splashSurface=2; add(27,"tide.flood",3,t);
        shape("tide.flood",3,"capstone",{});
        // Hexes (INT spells): curses.
        t=buff("Misfortune","Curse a visible foe for four enemy turns: its attacks miss 25% more often.",StatusEffectType::Misfortune,4,25,2,6);
        t.targeting=TargetingMode::RangedEnemyInSight; add(28,"hexes.misfortune",0,t);
        shape("hexes.misfortune",3,"",{});
        t=buff("Soul Link","Bind a visible foe for four enemy turns: half of every hit it takes from you jumps to the nearest other foe within three tiles.",StatusEffectType::Linked,4,50,3,7);
        t.targeting=TargetingMode::RangedEnemyInSight; add(28,"hexes.link",1,t);
        shape("hexes.link",3,"path",{"hexes.misfortune"});
        t=buff("Enfeeble","Curse a visible foe: it grows slow, acting 40% less often for four enemy turns.",StatusEffectType::Slowed,4,40,3,7);
        t.targeting=TargetingMode::RangedEnemyInSight; add(28,"hexes.enfeeble",1,t);
        shape("hexes.enfeeble",3,"path",{"hexes.misfortune"});
        add(28,"hexes.malediction",2,passive("Malediction","Your attacks deal +4 damage to cursed enemies (Misfortune, Soul Link, Enfeeble, Plague, Wither, Doom).",PassiveKind::Malediction,4));
        shape("hexes.malediction",1,"",{"hexes.link"});
        add(28,"hexes.echo",2,passive("Hex Echo","When a cursed foe dies, its curses leap to the nearest foe within four tiles.",PassiveKind::HexEcho,1));
        shape("hexes.echo",1,"",{"hexes.enfeeble"});
        t=buff("Puppet","Seize a visible foe's will for three enemy turns: it fights its own side. Bosses and champions resist.",StatusEffectType::Puppeted,3,1,6,12);
        t.targeting=TargetingMode::RangedEnemyInSight; add(28,"hexes.puppet",3,t);
        shape("hexes.puppet",3,"capstone",{});
        t=buff("Doom","Lay doom on a visible foe: in four enemy turns, it takes 20.",StatusEffectType::Doom,4,20,5,10);
        t.targeting=TargetingMode::RangedEnemyInSight; add(28,"hexes.doom",3,t);
        shape("hexes.doom",3,"capstone",{});
        // Venom (INT spells): poison and plague.
        t=attack("Venom Bolt","A bolt of venom: poisons for 2 per turn over four enemy turns.",3,2,1,true);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Poison,4,2}; add(29,"venom.bolt",0,t);
        shape("venom.bolt",3,"",{});
        t=attack("Miasma","A cloud of poison gas fills a tile and its neighbours for six turns. Fire makes it explode.",2,4,6,true,1);
        t.projectile=false; t.splashSurface=8; t.splashTurns=6; add(29,"venom.miasma",1,t);
        shape("venom.miasma",3,"path",{"venom.bolt"});
        t=attack("Fester","Make a poisoned foe's poison fester: it doubles in strength and lasts two turns longer.",3,3,6,true);
        t.projectile=false; t.festerPoison=true; add(29,"venom.fester",1,t);
        shape("venom.fester",3,"path",{"venom.bolt"});
        add(29,"venom.ruin",2,passive("Toxic Ruin","Your attacks deal +4 damage to poisoned or plagued enemies.",PassiveKind::ToxicRuin,4));
        shape("venom.ruin",1,"",{"venom.miasma"});
        add(29,"venom.virulence",2,passive("Virulence","Poison you cause lasts two turns longer.",PassiveKind::Virulence,2));
        shape("venom.virulence",1,"",{"venom.fester"});
        t=attack("Plague","Infect a visible foe: 3 damage a turn for five enemy turns, and when it dies the plague spreads to everything beside it.",4,6,9,true);
        t.projectile=false; t.onHitEffect=StatusEffectInstance{StatusEffectType::Plague,5,3}; add(29,"venom.plague",3,t);
        shape("venom.plague",3,"capstone",{});
        t=attack("Blight","Rot takes a foe's flesh: for five enemy turns, every hit it takes deals 3 more.",5,6,9,true);
        t.projectile=false; t.onHitEffect=StatusEffectInstance{StatusEffectType::Sundered,5,3}; add(29,"venom.blight",3,t);
        shape("venom.blight",3,"capstone",{});
        // Traps (DEX): set where your foes will walk. Up to eight at once; each lasts 40 turns.
        const auto trap=[&](const char* name,const char* desc,int kind,int mana,int cd,int radius=0,bool spread=false) {
            Talent tr; tr.name=name; tr.description=desc; tr.targeting=TargetingMode::RangedEnemyInSight; tr.effectKind=TalentEffectKind::SelfBuff;
            tr.placeTrap=kind; tr.manaCost=mana; tr.cooldownTurns=cd; tr.areaRadius=radius; if (spread) tr.shape=EffectShape::AreaAroundTarget; return tr;
        };
        add(30,"traps.snare",0,trap("Snare","Set a snare on visible ground: the first foe to step on it takes 4 and is pinned for three turns.",1,1,4));
        shape("traps.snare",3,"",{});
        add(30,"traps.tripwire",1,trap("Tripwire","Stretch a tripwire: the first foe to cross it takes 2 and is flung two tiles on in the direction it was walking.",2,2,5));
        shape("traps.tripwire",3,"path",{"traps.snare"});
        add(30,"traps.bear",1,trap("Bear Trap","Set iron jaws: the first foe to step in takes 8 and is held, stunned, for two enemy turns.",6,3,7));
        shape("traps.bear",3,"path",{"traps.snare"});
        add(30,"traps.trapper",2,passive("Trapper","Your traps deal +4 damage.",PassiveKind::Trapper,4));
        shape("traps.trapper",1,"",{"traps.tripwire"});
        add(30,"traps.ambusher",2,passive("Ambusher","Foes caught in your traps are marked: their next direct hit taken deals +25%.",PassiveKind::Ambusher,1));
        shape("traps.ambusher",1,"",{"traps.bear"});
        add(30,"traps.rigged",3,trap("Rigged Charge","Bury a charge: when a foe steps on it, it blows up for 8 to everything within a tile and sets the ground alight.",3,4,8,1));
        shape("traps.rigged",3,"capstone",{});
        add(30,"traps.net",3,trap("Net Trap","Hide a weighted net: when a foe steps on it, everything within a tile is pinned for three turns.",8,4,9,1));
        shape("traps.net",3,"capstone",{});
        // Skirmish (DEX): moving is attacking.
        add(31,"skirmish.lunge",0,passive("Lunge","Step toward a foe two tiles ahead and you strike it as you close, for 6.",PassiveKind::Lunge,6));
        shape("skirmish.lunge",1,"",{});
        t=move("Blitz","Dash up to three tiles and strike everything beside your path. Counts as movement.",3,3,7);
        t.blitz=true; t.power=6; add(31,"skirmish.blitz",1,t);
        shape("skirmish.blitz",3,"path",{"skirmish.lunge"});
        t=attack("Hit and Run","Strike a foe beside you, step a tile back from it and gain Opening.",5,2,5);
        t.retreatDistance=1; t.selfBuffEffect=StatusEffectInstance{StatusEffectType::Opening,2,0}; add(31,"skirmish.hit_and_run",1,t);
        shape("skirmish.hit_and_run",3,"path",{"skirmish.lunge"});
        add(31,"skirmish.pass",2,passive("Pass Strike","Step from beside a foe to another tile beside it and you cut it in passing, for 5.",PassiveKind::PassStrike,5));
        shape("skirmish.pass",1,"",{"skirmish.blitz"});
        add(31,"skirmish.running_start",2,passive("Running Start","While you have Opening (after moving or waiting), your attacks deal +4 damage.",PassiveKind::RunningStart,4));
        shape("skirmish.running_start",1,"",{"skirmish.hit_and_run"});
        t=buff("Slipstream","Stay light on your feet: +25% dodge, Opening, and 50% faster, for three turns.",StatusEffectType::Evasion,3,25,3,12);
        t.grantOpening=true; t.hasteSelf=50; add(31,"skirmish.slipstream",3,t);
        shape("skirmish.slipstream",3,"capstone",{});
        t=attack("Flying Kick","Charge up to four tiles in a straight line at a foe and kick it two tiles back.",7,4,8);
        t.chargeDistance=4; t.pushDistance=2; add(31,"skirmish.flying_kick",3,t);
        shape("skirmish.flying_kick",3,"capstone",{});
        // Lamplighter (INT; Radiance + Fire): the torch itself. Needs your light burning.
        t=attack("Torch Swing","Swing your burning torch: the foe takes the hit and burns for 2 a turn. Needs your light lit.",6,1,2);
        t.needsLight=true; t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,3,2}; add(32,"lamplighter.swing",0,t);
        shape("lamplighter.swing",3,"",{});
        t=attack("Hurl Torch","Throw your torch: it bursts on impact, burning everything within a tile, and lies there burning as a light. Your own light goes with it (L lights another).",6,3,6,true,1);
        t.needsLight=true; t.hurlTorch=true; t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,3,2}; add(32,"lamplighter.hurl",1,t);
        shape("lamplighter.hurl",3,"path",{"lamplighter.swing"});
        t=attack("Brandish","Wave your torch: the foes beside you are blinded and driven back a tile. Needs your light lit.",3,3,6,false,1);
        t.needsLight=true; t.pushDistance=1; t.onHitEffect=StatusEffectInstance{StatusEffectType::Blinded,2,0}; add(32,"lamplighter.brandish",1,t);
        shape("lamplighter.brandish",3,"path",{"lamplighter.swing"});
        add(32,"lamplighter.ward",2,passive("Lantern Ward","While your light burns, every foe that ends its turn beside you is seared for 4.",PassiveKind::LanternWard,4));
        shape("lamplighter.ward",1,"",{"lamplighter.hurl"});
        add(32,"lamplighter.bearer",2,passive("Lantern Bearer","Your light reaches two tiles further.",PassiveKind::LanternBearer,2));
        shape("lamplighter.bearer",1,"",{"lamplighter.brandish"});
        t=attack("Bonfire","Plant your torch and let it roar: everything within two tiles burns, the ground around you catches, and the spot stays lit. Needs your light lit.",7,7,10,false,2);
        t.needsLight=true; t.bonfire=true; t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,3,2}; add(32,"lamplighter.bonfire",3,t);
        shape("lamplighter.bonfire",3,"capstone",{});
        t=attack("Pyre","Set one foe fiercely ablaze: it burns for 5 a turn and the ground under it catches. Needs your light lit.",7,6,9,true);
        t.projectile=false; t.needsLight=true; t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,5,5}; t.splashSurface=3; t.splashTurns=5;
        add(32,"lamplighter.pyre",3,t);
        shape("lamplighter.pyre",3,"capstone",{});
        // Stormlance (STR; Spear + Lightning): the spear carries the storm. Needs a spear.
        t=attack("Charged Thrust","Strike up to two tiles away with a spear full of lightning: the foe is Shocked for four turns.",6,2,3);
        t.reach=2; t.onHitEffect=StatusEffectInstance{StatusEffectType::Shock,4,0}; add(33,"stormlance.thrust",0,t);
        shape("stormlance.thrust",3,"",{});
        t=attack("Lightning Javelin","Hurl a bolt-tipped spear that leaps to a second foe nearby. Shocks.",8,4,6,true);
        t.chain=true; t.onHitEffect=StatusEffectInstance{StatusEffectType::Shock,4,0}; add(33,"stormlance.javelin",1,t);
        shape("stormlance.javelin",3,"path",{"stormlance.thrust"});
        t=attack("Lightning Rod","Plant the spear: lightning strikes the foes around you now, and again as each of your next two turns begins.",5,5,9,false,1);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Shock,3,0}; t.lingerTurns=2; add(33,"stormlance.rod",1,t);
        shape("stormlance.rod",3,"path",{"stormlance.thrust"});
        add(33,"stormlance.static_edge",2,passive("Static Edge","Spear and Stormlance attacks deal +4 damage to Shocked enemies.",PassiveKind::StaticEdge,4));
        shape("stormlance.static_edge",1,"",{"stormlance.javelin"});
        add(33,"stormlance.overcharge",2,passive("Overcharge","Burn, chill and shock you apply last two turns longer.",PassiveKind::LingeringElements,2));
        shape("stormlance.overcharge",1,"",{"stormlance.rod"});
        t=move("Thunder Vault","Vault up to 3/3/4/4/5 tiles over foes and chasms and come down in a burst of lightning: everything beside you takes 6 and is Shocked.",3,5,8);
        t.vault=true; t.landingBurst=6; add(33,"stormlance.vault",3,t);
        shape("stormlance.vault",3,"capstone",{});
        t=move("Ride the Lightning","Dash up to four tiles as lightning: everything beside your path is struck and shocked.",4,4,8);
        t.blitz=true; t.power=6; t.onHitEffect=StatusEffectInstance{StatusEffectType::Shock,3,0}; add(33,"stormlance.ride",3,t);
        shape("stormlance.ride",3,"capstone",{});
        // Hexblade (STR; One-Handed + Hexes): a cursed blade.
        t=attack("Cursed Edge","A strike that curses: Misfortune (25%) for three enemy turns.",5,1,2);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Misfortune,3,25}; add(34,"hexblade.edge",0,t);
        shape("hexblade.edge",3,"",{});
        t=attack("Soul Rend","Double damage against a cursed foe (Misfortune, Soul Link, Plague, Wither or Doom).",8,3,5);
        t.curseBonus=true; add(34,"hexblade.rend",1,t);
        shape("hexblade.rend",3,"path",{"hexblade.edge"});
        t=attack("Cursed Cleave","A sweep through every foe beside you, cursing each with Misfortune (25%) for three enemy turns.",5,4,6,false,1);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Misfortune,3,25}; add(34,"hexblade.cleave",1,t);
        shape("hexblade.cleave",3,"path",{"hexblade.edge"});
        add(34,"hexblade.lingering",2,passive("Lingering Hex","Your melee hits make the curses on a foe last 3 turns longer (not Doom).",PassiveKind::LingeringHex,3));
        shape("hexblade.lingering",1,"",{"hexblade.rend"});
        add(34,"hexblade.hunger",2,passive("Hungering Blade","Your hits on cursed foes heal you 2, more with Strength.",PassiveKind::HungeringBlade,2));
        shape("hexblade.hunger",1,"",{"hexblade.cleave"});
        t=attack("Doom Blade","A strike that dooms: in four enemy turns the foe takes 15.",7,5,8);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Doom,4,15}; add(34,"hexblade.doom",3,t);
        shape("hexblade.doom",3,"capstone",{});
        t=attack("Soul Reap","A strike that tears every curse off the foe: +50% damage for each one.",8,5,9); t.consumeCurses=true;
        add(34,"hexblade.reap",3,t);
        shape("hexblade.reap",3,"capstone",{});
        // Saboteur (DEX; Stealth + Alchemy): dirty work.
        add(35,"saboteur.caltrops",0,trap("Caltrops","Scatter caltrops over a tile and its neighbours: whatever steps on one takes 2 and bleeds.",4,2,5,1,true));
        shape("saboteur.caltrops",3,"",{});
        t=buff("Smoke Bomb","Vanish in smoke: Concealed for three responses, and everything within two tiles is blinded for two enemy turns.",StatusEffectType::Concealed,3,3,3,8);
        t.smokeBomb=true; add(35,"saboteur.smoke",1,t);
        shape("saboteur.smoke",3,"path",{"saboteur.caltrops"});
        t=attack("Firecracker","A thrown cracker bursts over a tile and its neighbours: everything there is blinded and set burning.",3,3,6,false,1);
        t.targeting=TargetingMode::RangedEnemyInSight; t.shape=EffectShape::AreaAroundTarget;
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Blinded,2,0}; t.secondHitEffect=StatusEffectInstance{StatusEffectType::Burn,3,2};
        add(35,"saboteur.firecracker",1,t);
        shape("saboteur.firecracker",3,"path",{"saboteur.caltrops"});
        add(35,"saboteur.tricks",2,passive("Dirty Tricks","Attacks made from concealment poison the target for 3 a turn over three turns.",PassiveKind::DirtyTricks,3));
        shape("saboteur.tricks",1,"",{"saboteur.smoke"});
        add(35,"saboteur.trap_sense",2,passive("Trap Sense","You can keep 12 traps set instead of 8.",PassiveKind::TrapSense,4));
        shape("saboteur.trap_sense",1,"",{"saboteur.firecracker"});
        add(35,"saboteur.booby",3,trap("Booby Trap","Hide a gas charge: when a foe steps on it, poison gas bursts out and catches fire, the blast running through the cloud.",5,4,9));
        shape("saboteur.booby",3,"capstone",{});
        t=attack("Demolition","Throw a hissing charge: as your next turn begins it blows, striking everything within two tiles and setting it burning.",14,6,12,false,2);
        t.targeting=TargetingMode::RangedEnemyInSight; t.shape=EffectShape::AreaAroundTarget; t.delayedBlast=true; t.lingerTurns=1;
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,3,2}; add(35,"saboteur.demolition",3,t);
        shape("saboteur.demolition",3,"capstone",{});
        // Stonefist (STR; Brawling + Earth): fists of stone.
        t=attack("Rock Fist","A stone-heavy punch that knocks the foe back a tile.",6,1,2);
        t.pushDistance=1; add(36,"stonefist.fist",0,t);
        shape("stonefist.fist",3,"",{});
        t=attack("Pillar Slam","Raise a stone pillar right behind an adjacent foe and drive it into it.",5,3,6);
        t.pillarSlam=true; add(36,"stonefist.slam",1,t);
        shape("stonefist.slam",3,"path",{"stonefist.fist"});
        t=attack("Tremor Punch","Punch the ground: stone grips the feet of every foe beside you, pinning them for two enemy turns.",5,3,7,false,1);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Pinned,2,0}; add(36,"stonefist.tremor",1,t);
        shape("stonefist.tremor",3,"path",{"stonefist.fist"});
        add(36,"stonefist.granite",2,passive("Granite Fists","Your melee attacks deal +4 damage to foes standing against a wall, pillar or fixture.",PassiveKind::GraniteFists,4));
        shape("stonefist.granite",1,"",{"stonefist.slam"});
        add(36,"stonefist.rockhide",2,passive("Rockhide","Direct hits on you deal 2 less damage.",PassiveKind::Stoneskin,2));
        shape("stonefist.rockhide",1,"",{"stonefist.tremor"});
        t=attack("Landslide","Charge up to 4/4/5/5/6 tiles in a straight line, smash the foe two tiles back and stun it for a turn.",9,5,9);
        t.chargeDistance=4; t.pushDistance=2; t.onHitEffect=StatusEffectInstance{StatusEffectType::Stun,1,0}; add(36,"stonefist.landslide",3,t);
        shape("stonefist.landslide",3,"capstone",{});
        t=move("Rockfall","Leap up to three tiles, over anything in the way, and come down like a boulder on everything beside you.",3,4,9);
        t.vault=true; t.landingSlam=10; add(36,"stonefist.rockfall",3,t);
        shape("stonefist.rockfall",3,"capstone",{});

        // Ascendancy nodes (Ascendancy.hpp): one rank, bought with ascendancy points.
        auto node=[&](const char* treeId,const char* id,ScalingStat stat,Talent t) {
            t.id=id; t.tree=TalentTree::Blade; t.scalingStat=stat; t.scalingCooldown=t.cooldownTurns;
            TalentDefinition d; d.id=id; d.treeId=treeId; d.tier=0; d.ranks={t};
            out.push_back(d);
        };
        constexpr auto Str=ScalingStat::Strength, Dex=ScalingStat::Dexterity, Int=ScalingStat::Intelligence;
        t=buff("Unstoppable","Shake off Poison, Burn, Chill, Marked and curses, then brace: Guard 5 for four turns.",StatusEffectType::Guard,4,5,4,14);
        t.cleanse=true; node("juggernaut","juggernaut.unstoppable",Str,t);
        t=attack("Earthshaker","Slam the ground: hits everything within two tiles, pushes survivors back and stuns them. Bosses resist repeated stuns.",8,6,12,false,2);
        t.pushDistance=1; t.onHitEffect=StatusEffectInstance{StatusEffectType::Stun,1,0}; node("juggernaut","juggernaut.earthshaker",Str,t);
        node("juggernaut","juggernaut.rampage",Str,passive("Rampage","Every enemy you kill reduces all your running cooldowns by one turn.",PassiveKind::Rampage,1));
        node("juggernaut","juggernaut.last_stand",Str,passive("Last Stand","Below a third of your maximum life, your attacks deal +5 damage and direct hits on you deal 3 less.",PassiveKind::LastStand,5));
        node("juggernaut","juggernaut.iron_skin",Str,passive("Iron Skin","+15% maximum life.",PassiveKind::IronSkin,15));
        node("juggernaut","juggernaut.crushing_blows",Str,passive("Crushing Blows","Your attacks deal +50% damage to stunned enemies.",PassiveKind::CrushingBlows,50));
        node("elementalist","elementalist.fury",Int,buff("Elemental Fury","Your attacks and spells deal +6 damage for four turns.",StatusEffectType::Empowered,4,6,8,15));
        node("elementalist","elementalist.ward",Int,buff("Elemental Ward","Wrap yourself in elements: Guard 6 for five turns.",StatusEffectType::Guard,5,6,8,14));
        node("elementalist","elementalist.conduit",Int,passive("Conduit","Consuming Burn, Chill or Shock (Meteor, Shatter, Discharge and the like) restores 5 mana.",PassiveKind::Conduit,5));
        node("elementalist","elementalist.lingering",Int,passive("Lingering Elements","Burn, Chill and Shock you apply last two turns longer.",PassiveKind::LingeringElements,2));
        node("elementalist","elementalist.overload",Int,passive("Elemental Overload","Applying Burn, Chill or Shock to an enemy already suffering a different one bursts for 6 damage.",PassiveKind::Overload,6));
        node("elementalist","elementalist.attunement",Int,passive("Attunement","Spells deal +3 damage to enemies suffering Burn, Chill or Shock.",PassiveKind::Attunement,3));
        t=buff("Smoke Veil","Vanish in smoke: Concealed for three responses, as hard to spot as a fully trained Conceal.",StatusEffectType::Concealed,3,3,4,12);
        node("trickster","trickster.smoke_veil",Dex,t);
        t=attack("Fan of Knives","Hit every adjacent enemy and Mark them: their next direct hit taken deals +25%.",7,5,8,false,1);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Marked,3,1}; node("trickster","trickster.fan_of_knives",Dex,t);
        node("trickster","trickster.opportunist",Dex,passive("Opportunist","Critical hits while Concealed or with Opening deal +50% critical damage.",PassiveKind::Opportunist,50));
        node("trickster","trickster.slippery",Dex,passive("Slippery","Dodging an attack grants Opening.",PassiveKind::Slippery,1));
        node("trickster","trickster.quick_hands",Dex,passive("Quick Hands","+8% dodge chance. Total dodge is capped at 75%.",PassiveKind::QuickHands,8));
        node("trickster","trickster.killer_instinct",Dex,passive("Killer Instinct","Your attacks deal +4 damage to enemies below half their life.",PassiveKind::KillerInstinct,4));
        // Templar (STR/INT).
        t=attack("Consecrate","Smite everything within two tiles with holy fire, then stand guarded: Guard 3 for three responses.",7,8,10,false,2);
        t.selfBuffEffect=StatusEffectInstance{StatusEffectType::Guard,3,3}; node("templar","templar.consecrate",Int,t);
        node("templar","templar.aegis",Int,buff("Aegis","A wall of light: Guard 8 for three enemy responses.",StatusEffectType::Guard,3,8,12,16));
        node("templar","templar.zeal",Int,passive("Zeal","Every spell you cast grants Guard 2 for the next enemy response.",PassiveKind::Zeal,2));
        node("templar","templar.retribution",Int,passive("Retribution","While you are guarded, enemies that hit you in melee take 4 damage.",PassiveKind::Retribution,4));
        node("templar","templar.devotion",Int,passive("Devotion","+20% maximum mana.",PassiveKind::Devotion,20));
        node("templar","templar.righteous",Int,passive("Righteous","Above half mana, your attacks and spells deal +3 damage.",PassiveKind::Righteous,3));
        // Shadowcaster (DEX/INT).
        t=move("Veil","Step up to three tiles through shadow and vanish: Concealed for two responses.",3,5,10);
        t.selfBuffEffect=StatusEffectInstance{StatusEffectType::Concealed,2,2}; node("shadowcaster","shadowcaster.veil",Dex,t);
        t=buff("Hex","Curse a visible enemy: its next two direct hits taken deal +25%.",StatusEffectType::Marked,4,2,5,7);
        t.targeting=TargetingMode::RangedEnemyInSight; node("shadowcaster","shadowcaster.hex",Int,t);
        node("shadowcaster","shadowcaster.hidden_casting",Int,passive("Hidden Casting","Spells cast while Concealed deal +5 damage.",PassiveKind::HiddenCasting,5));
        node("shadowcaster","shadowcaster.lingering_shadow",Dex,passive("Lingering Shadow","Attacking while Concealed has a 35% chance not to reveal you.",PassiveKind::LingeringShadow,35));
        node("shadowcaster","shadowcaster.shade_step",Dex,passive("Shade Step","Movement abilities leave you Concealed for one response.",PassiveKind::ShadeStep,1));
        node("shadowcaster","shadowcaster.spell_thief",Int,passive("Spell Thief","Killing an enemy that suffers an ailment restores 4 mana.",PassiveKind::SpellThief,4));
        // Duelist (STR/DEX).
        t=buff("Challenge","Call out a visible enemy: its next three direct hits taken deal +25%.",StatusEffectType::Marked,6,3,4,9);
        t.targeting=TargetingMode::RangedEnemyInSight; node("duelist","duelist.challenge",Dex,t);
        t=attack("Flurry","A storm of quick cuts on one adjacent enemy with +30% critical chance.",11,4,5); t.bonusCritChance=.3f;
        node("duelist","duelist.flurry",Dex,t);
        node("duelist","duelist.momentum",Dex,passive("Momentum","Each consecutive hit on the same enemy deals +2 more damage, up to +6. Switching targets resets it.",PassiveKind::Momentum,2));
        node("duelist","duelist.finisher",Str,passive("Finisher","Your attacks deal +6 damage to enemies below a quarter of their life.",PassiveKind::Finisher,6));
        node("duelist","duelist.counter",Dex,passive("Counter","Dodging a melee attack strikes the attacker back for 5.",PassiveKind::Counter,5));
        node("duelist","duelist.en_garde",Str,passive("En Garde","While you have Opening, direct hits on you deal 3 less.",PassiveKind::EnGarde,3));
        // Paragon (all three).
        t=buff("Exalt","Shake off ailments and curses and surge: +4 damage for four turns.",StatusEffectType::Empowered,4,4,6,14); t.cleanse=true;
        node("paragon","paragon.exalt",Str,t);
        t=Talent{}; t.name="Renewal"; t.description="Recover 30% of your maximum life at once."; t.targeting=TargetingMode::Self;
        t.effectKind=TalentEffectKind::SelfBuff; t.restoreHpPercent=30; t.manaCost=10; t.cooldownTurns=20; node("paragon","paragon.renewal",Int,t);
        node("paragon","paragon.balance",Str,passive("Balance","+2 Strength, Dexterity and Intelligence.",PassiveKind::Balance,2));
        node("paragon","paragon.versatility",Dex,passive("Versatility","+5% dodge and +5% critical chance.",PassiveKind::Versatility,5));
        node("paragon","paragon.resilience",Str,passive("Resilience","Direct hits on you deal 2 less.",PassiveKind::Resilience,2));
        node("paragon","paragon.wellspring",Int,passive("Wellspring","+10% maximum life and maximum mana.",PassiveKind::Wellspring,10));
        // Forgeknight (STR): heat as armour.
        node("forgeknight","forgeknight.heat_engine",Str,passive("Heat Engine","Each blow that lands on you gives you 1 Heat, and every 2 Heat you hold is 1 armour.",PassiveKind::HeatEngine,1));
        t=move("Furnace Slam","Leap up to three tiles and slam everything beside where you land, spending all your Heat for +2 damage each point. The ground around you burns.",3,5,8);
        t.vault=true; t.landingSlam=6; t.ventHeat=2; node("forgeknight","forgeknight.slam",Str,t);
        t=Talent{}; t.name="Quench"; t.description="Drop all your Heat at once, and heal 3 life for each point."; t.targeting=TargetingMode::Self;
        t.effectKind=TalentEffectKind::SelfBuff; t.quench=3; t.manaCost=2; t.cooldownTurns=10; node("forgeknight","forgeknight.quench",Str,t);
        node("forgeknight","forgeknight.burning_plate",Str,passive("Burning Plate","Foes that strike you in melee burn for 3 turns.",PassiveKind::BurningPlate,2));
        node("forgeknight","forgeknight.overheat",Str,passive("Overheat","While you hold 10 Heat or more, your hits deal +50%, and Heat no longer burns you.",PassiveKind::Overheat,50));
        t=Talent{}; t.name="Anvil Stance"; t.description="For 3 turns you can't be moved, take 30% less damage from blows, and gain 2 Heat a turn.";
        t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.anvil=3; t.manaCost=4; t.cooldownTurns=12; node("forgeknight","forgeknight.anvil",Str,t);

        // The pilot trees' colours: Fire is Flame, Arcane is Arcane, and
        // One-Handed is Steel except for its guard (Parry, Riposte).
        for (auto& d:out) {
            if (d.treeId=="fire") d.affinity=Affinity::Flame;
            else if (d.treeId=="arcane") d.affinity=Affinity::Arcane;
            else if (d.treeId=="ice") d.affinity=Affinity::Frost;
            else if (d.treeId=="lightning") d.affinity=Affinity::Storm;
            else if (d.treeId=="two_handed") d.affinity=Affinity::Steel;
            else if (d.treeId=="shadow") d.affinity=Affinity::Dark;
            else if (d.treeId=="radiance") d.affinity=Affinity::Light;
            else if (d.treeId=="shield") d.affinity=Affinity::Guard;
            else if (d.treeId=="bow") d.affinity=Affinity::Hunt;
            else if (d.treeId=="stealth") d.affinity=Affinity::Guile;
            else if (d.treeId=="daggers") d.affinity=(d.id=="daggers.ambush" || d.id=="daggers.assassinate")?Affinity::Guile:Affinity::Steel;
            else if (d.treeId=="earth") d.affinity=Affinity::Earth;
            else if (d.treeId=="tide") d.affinity=Affinity::Water;
            else if (d.treeId=="venom") d.affinity=Affinity::Rot;
            else if (d.treeId=="spear" || d.treeId=="mace") d.affinity=Affinity::Steel;
            else if (d.treeId=="crossbow") d.affinity=Affinity::Hunt;
            else if (d.treeId=="brawling" || d.treeId=="whip" || d.treeId=="skirmish") d.affinity=Affinity::Motion;
            else if (d.treeId=="alchemy" || d.treeId=="traps") d.affinity=Affinity::Guile;
            else if (d.treeId=="hexes") d.affinity=Affinity::Dark;
            else if (d.treeId=="acrobatics" || d.treeId=="light_armour") d.affinity=Affinity::Motion;
            else if (d.treeId=="cloth") d.affinity=Affinity::Arcane;
            else if (d.treeId=="heavy_armour") d.affinity=Affinity::Guard;
            else if (d.treeId=="spellblade")
                d.affinity=(d.id=="spellblade.cleave" || d.id=="spellblade.ward" || d.id=="spellblade.blink_strike")?Affinity::Steel:Affinity::Arcane;
            else if (d.treeId=="animation") d.affinity=Affinity::Death;
            else if (d.treeId=="blood_magic") d.affinity=Affinity::Blood;
            else if (d.treeId=="shadow_archer")
                d.affinity=(d.id=="shadow_archer.mark" || d.id=="shadow_archer.smoke" || d.id=="shadow_archer.unseen")?Affinity::Guile:Affinity::Hunt;
            else if (d.treeId=="lamplighter")
                d.affinity=(d.id=="lamplighter.ward" || d.id=="lamplighter.bearer" || d.id=="lamplighter.brandish")?Affinity::Light:Affinity::Flame;
            else if (d.treeId=="hexblade")
                d.affinity=(d.id=="hexblade.lingering" || d.id=="hexblade.hunger" || d.id=="hexblade.reap")?Affinity::Dark:Affinity::Steel;
            else if (d.treeId=="saboteur") d.affinity=Affinity::Guile;
            else if (d.treeId=="stonefist")
                d.affinity=(d.id=="stonefist.tremor" || d.id=="stonefist.granite" || d.id=="stonefist.rockhide")?Affinity::Earth:Affinity::Motion;
            else if (d.treeId=="tempest") d.affinity=Affinity::Storm;
            else if (d.treeId=="briarheart") d.affinity=(d.id=="briarheart.lash" || d.id=="briarheart.thornborn" || d.id=="briarheart.heart")?Affinity::Hunt:Affinity::Rot;
            else if (d.treeId=="rimeheart") d.affinity=(d.id=="rimeheart.path" || d.id=="rimeheart.freeze")?Affinity::Water:Affinity::Frost;
            else if (d.treeId=="bonewright") d.affinity=(d.id=="bonewright.wall" || d.id=="bonewright.marrow")?Affinity::Earth:Affinity::Death;
            else if (d.treeId=="slagcaller")
                d.affinity=(d.id=="slagcaller.hail" || d.id=="slagcaller.pyroclasm" || d.id=="slagcaller.eruption")?Affinity::Flame:Affinity::Earth;
            else if (d.treeId=="forgeborn")
                d.affinity=(d.id=="forgeborn.searing" || d.id=="forgeborn.tempered" || d.id=="forgeborn.forgeheart")?Affinity::Steel:Affinity::Flame;
            else if (d.treeId=="warbanner")
                d.affinity=(d.id=="warbanner.bash" || d.id=="warbanner.ranks" || d.id=="warbanner.charge")?Affinity::Steel:Affinity::Guard;
            else if (d.treeId=="stormlance")
                d.affinity=(d.id=="stormlance.thrust" || d.id=="stormlance.vault" || d.id=="stormlance.static_edge")?Affinity::Steel:Affinity::Storm;
            else if (d.treeId=="one_handed") d.affinity=(d.id=="one_handed.parry" || d.id=="one_handed.riposte")?Affinity::Guard:Affinity::Steel;
        }
        // Flat passive numbers grow with an attribute: damage, healing, mana,
        // Guard and damage taken by 1 per 5 points; the strength of a lingering
        // effect by 1 per 10. Percentages and on/off passives stay as they are.
        const auto growth=[](PassiveKind k) {
            switch (k) {
                case PassiveKind::Riposte: case PassiveKind::Bloodlust: case PassiveKind::ShieldTraining: case PassiveKind::Ambush:
                case PassiveKind::Frostbite: case PassiveKind::Spellweave: case PassiveKind::HeavyBrace: case PassiveKind::BattleRhythm:
                case PassiveKind::Kindling: case PassiveKind::Exploit: case PassiveKind::Dread: case PassiveKind::Skewer:
                case PassiveKind::Hemorrhage: case PassiveKind::ArcFlash: case PassiveKind::Aftershock: case PassiveKind::Malediction:
                case PassiveKind::ToxicRuin: case PassiveKind::Riptide: case PassiveKind::InnerLight: case PassiveKind::Umbral:
                case PassiveKind::LongReach: case PassiveKind::Bonebreaker: case PassiveKind::Windlass: case PassiveKind::RunningStart:
                case PassiveKind::Flay: case PassiveKind::HardLanding: case PassiveKind::PotentBrews: case PassiveKind::Trapper:
                case PassiveKind::Lunge: case PassiveKind::PassStrike: case PassiveKind::LanternWard: case PassiveKind::SearingEdge:
                case PassiveKind::FollowThrough: case PassiveKind::Stoneskin: case PassiveKind::HallowedGuard: case PassiveKind::Tidecaller:
                case PassiveKind::Bulwark: case PassiveKind::FlowingMana: case PassiveKind::BladeWard: case PassiveKind::Transfusion:
                case PassiveKind::GravePact: case PassiveKind::LongShadow: case PassiveKind::StaticEdge:
                case PassiveKind::HungeringBlade: case PassiveKind::GraniteFists: case PassiveKind::HoldTheLine: case PassiveKind::BreakRanks: case PassiveKind::Tempered: case PassiveKind::Overcharge: case PassiveKind::Marrow: case PassiveKind::BrittleCold: case PassiveKind::Thornborn:
                    return 5;
                case PassiveKind::BurningPlate:
                    return 10;
                case PassiveKind::FireArrows: case PassiveKind::Incendiary: case PassiveKind::WastingCurse: case PassiveKind::Witchfire:
                case PassiveKind::FoulWater: case PassiveKind::EnvenomedBlades: case PassiveKind::GraveLight: case PassiveKind::BoilingBlood:
                case PassiveKind::Bloodletter: case PassiveKind::Ionise: case PassiveKind::DirtyTricks:
                    return 10;
                default: return 0;
            }
        };

        // Resonances: one-rank passives that exist only between two colours.
        const auto resonance=[&](const char* id,Affinity a,Affinity b,Talent t) {
            t.id=id; t.tree=TalentTree::Blade; t.scalingCooldown=t.cooldownTurns;
            TalentDefinition d; d.id=id; d.treeId="resonance"; d.ranks={t}; d.resonance[0]=a; d.resonance[1]=b;
            out.push_back(d);
        };
        resonance("resonance.searing_edge",Affinity::Steel,Affinity::Flame,
            passive("Searing Edge","When a melee ability strikes a burning foe, the burn flares: it is spent for 6 extra damage, and the ground behind the foe catches fire.",PassiveKind::SearingEdge,6));
        resonance("resonance.spellsword",Affinity::Steel,Affinity::Arcane,
            passive("Spellsword","Casting a spell readies +6 damage for your next melee attack, and a landed melee attack takes a turn off your longest spell cooldown.",PassiveKind::BattleRhythm,6));
        resonance("resonance.thermal_shock",Affinity::Frost,Affinity::Storm,
            passive("Thermal Shock","A foe that is chilled and shocked at once locks up: your hit stuns it for a turn.",PassiveKind::ThermalShock,1));
        resonance("resonance.cold_steel",Affinity::Steel,Affinity::Frost,
            passive("Cold Steel","Your melee abilities chill what they strike.",PassiveKind::ColdSteel,1));
        resonance("resonance.twilight",Affinity::Light,Affinity::Dark,
            passive("Twilight","Each school takes the other's edge: your Shadow spells sear undead and darkvision creatures (+50%), and your Radiance spells strike +50% harder at foes standing in darkness.",PassiveKind::Twilight,1));
        resonance("resonance.templars_edge",Affinity::Steel,Affinity::Light,
            passive("Templar's Edge","Your melee abilities blind foes standing in light.",PassiveKind::TemplarsEdge,1));
        resonance("resonance.hallowed_guard",Affinity::Guard,Affinity::Light,
            passive("Hallowed Guard","Whenever you take up Guard, you also heal 3.",PassiveKind::HallowedGuard,3));
        resonance("resonance.fire_arrows",Affinity::Hunt,Affinity::Flame,
            passive("Fire Arrows","Your bow attacks set what they hit burning.",PassiveKind::FireArrows,2));
        resonance("resonance.unseen_hand",Affinity::Guile,Affinity::Dark,
            passive("Unseen Hand","Attacks you make from an unlit tile don't break your concealment.",PassiveKind::UnseenHand,1));
        resonance("resonance.assassins_edge",Affinity::Steel,Affinity::Guile,
            passive("Assassin's Edge","Your melee hits from concealment deal +50% damage.",PassiveKind::AssassinsEdge,50));
        resonance("resonance.mire",Affinity::Earth,Affinity::Water,
            passive("Mire","Your hits pin foes standing in water.",PassiveKind::Mire,1));
        resonance("resonance.foul_water",Affinity::Rot,Affinity::Water,
            passive("Foul Water","Foes standing in water are poisoned each turn.",PassiveKind::FoulWater,2));
        resonance("resonance.envenomed_blades",Affinity::Steel,Affinity::Rot,
            passive("Envenomed Blades","Your melee abilities poison what they strike.",PassiveKind::EnvenomedBlades,2));
        resonance("resonance.bonecrusher",Affinity::Steel,Affinity::Earth,
            passive("Bonecrusher","Your melee abilities knock foes back a tile: into walls, fire, each other or chasms.",PassiveKind::Bonecrusher,1));
        resonance("resonance.storm_bolts",Affinity::Hunt,Affinity::Storm,
            passive("Storm Bolts","Your bow and crossbow attacks shock what they hit.",PassiveKind::StormBolts,1));
        resonance("resonance.bedrock",Affinity::Guard,Affinity::Earth,
            passive("Bedrock","While you have Guard, nothing can push you or knock you back.",PassiveKind::Bedrock,1));
        resonance("resonance.ghost_step",Affinity::Motion,Affinity::Guile,
            passive("Ghost Step","Your movement abilities leave you concealed for a turn.",PassiveKind::ShadeStep,2));
        resonance("resonance.lightning_feet",Affinity::Motion,Affinity::Storm,
            passive("Lightning Feet","Your movement abilities shock every foe beside your path.",PassiveKind::LightningFeet,1));
        resonance("resonance.tremor",Affinity::Motion,Affinity::Earth,
            passive("Tremor","When a charge or leap brings you down, the foes beside you are knocked back a tile.",PassiveKind::Tremor,1));
        resonance("resonance.incendiary",Affinity::Guile,Affinity::Flame,
            passive("Incendiary","Your traps and flasks set what they catch burning.",PassiveKind::Incendiary,2));
        resonance("resonance.wasting_curse",Affinity::Dark,Affinity::Rot,
            passive("Wasting Curse","Foes you curse are also poisoned.",PassiveKind::WastingCurse,2));
        resonance("resonance.witchfire",Affinity::Dark,Affinity::Flame,
            passive("Witchfire","Your curses also set the foe burning.",PassiveKind::Witchfire,2));
        resonance("resonance.flowing_mana",Affinity::Motion,Affinity::Arcane,
            passive("Flowing Mana","Your movement abilities restore 3 mana, more with Intelligence.",PassiveKind::FlowingMana,3));
        resonance("resonance.arcane_bulwark",Affinity::Guard,Affinity::Arcane,
            passive("Arcane Bulwark","Whenever you take up Guard, you also gain that much spell ward.",PassiveKind::ArcaneBulwark,1));
        resonance("resonance.unstoppable",Affinity::Guard,Affinity::Motion,
            passive("Unstoppable","Taking up Guard also makes you 25% faster for two turns.",PassiveKind::Unstoppable,25));
        resonance("resonance.grave_light",Affinity::Light,Affinity::Death,
            passive("Grave Light","Your skeletons glow, and set what they hit burning.",PassiveKind::GraveLight,2));
        resonance("resonance.boiling_blood",Affinity::Blood,Affinity::Flame,
            passive("Boiling Blood","Whenever you spend life, the foes beside you catch fire.",PassiveKind::BoilingBlood,2));
        resonance("resonance.bloodletter",Affinity::Steel,Affinity::Blood,
            passive("Bloodletter","Your melee abilities make foes bleed.",PassiveKind::Bloodletter,2));
        resonance("resonance.thunderflash",Affinity::Light,Affinity::Storm,
            passive("Thunderflash","Your lightning also blinds what it hits.",PassiveKind::Thunderflash,1));
        resonance("resonance.ionise",Affinity::Flame,Affinity::Storm,
            passive("Ionise","Your lightning sets what it hits burning.",PassiveKind::Ionise,2));
        resonance("resonance.ghost_arrows",Affinity::Hunt,Affinity::Guile,
            passive("Ghost Arrows","Bow attacks from hiding can't be dodged.",PassiveKind::GhostArrows,1));
        resonance("resonance.soul_harvest",Affinity::Dark,Affinity::Death,
            passive("Soul Harvest","When a cursed foe dies, it rises as your skeleton, if you have room for one more.",PassiveKind::SoulHarvest,1));
        resonance("resonance.magma",Affinity::Earth,Affinity::Flame,
            passive("Magma","Your Earth spells leave the ground under what they hit burning.",PassiveKind::Magma,1));
        resonance("resonance.bloodhound",Affinity::Hunt,Affinity::Blood,
            passive("Bloodhound","Your bow and crossbow hits on bleeding foes deal +50% damage.",PassiveKind::Bloodhound,50));
        // Passives grow with their tree's attribute; resonances with your highest.
        for (auto& d:out) for (auto& r:d.ranks) if (r.passive) {
            r.scalePer=growth(r.passiveKind);
            r.scaleHighest=d.treeId=="resonance" && d.id!="resonance.flowing_mana";
            if (d.id=="resonance.flowing_mana") r.scalingStat=ScalingStat::Intelligence;
        }
        return out;
    }();
    return catalog;
}
inline bool isImbueVariant(const std::string& id) { return id=="spellblade.flame" || id=="spellblade.frost" || id=="spellblade.storm" || id=="spellblade.arcane"; }
inline const TalentDefinition* findTalentDefinition(const std::string& id) {
    if (isImbueVariant(id)) {
        static const auto variants=[] {
            std::array<TalentDefinition,4> result;
            const TalentDefinition* base=nullptr;
            for (const auto& d:talentCatalog()) if (d.id=="spellblade.imbue") base=&d;
            const char* ids[]{"spellblade.flame","spellblade.frost","spellblade.storm","spellblade.arcane"};
            const char* names[]{"Flame Blade","Frost Blade","Storm Blade","Arcane Blade"};
            for (int i=0;i<4;++i) {
                result[i]=*base; result[i].id=ids[i];
                for (auto& t:result[i].ranks) { t.id=ids[i]; t.name=names[i]; t.imbueElement=i+1; t.selfBuffEffect->type=static_cast<StatusEffectType>(static_cast<int>(StatusEffectType::FlameBlade)+i); }
            }
            return result;
        }();
        for (const auto& d:variants) if (d.id==id) return &d;
    }
    for (const auto& t : talentCatalog()) if (t.id == id) return &t;
    return nullptr;
}
// The nodes of one tree, in catalogue order.
inline const std::vector<const TalentDefinition*>& treeNodes(const std::string& treeId) {
    static const auto byTree = [] {
        std::map<std::string, std::vector<const TalentDefinition*>> out;
        for (const auto& d : talentCatalog()) out[d.treeId].push_back(&d);
        return out;
    }();
    static const std::vector<const TalentDefinition*> none;
    const auto found = byTree.find(treeId);
    return found == byTree.end() ? none : found->second;
}
inline const std::vector<const TalentDefinition*>& treeNodes(std::size_t treeIndex) { return treeNodes(kTalentTrees[treeIndex].id); }

// Why a node can't be learned yet, or empty if it can: the one rule shared by
// purchases and save validation. `rankOf` reports the rank held in a node.
// `nodes` is the node's tree (a test can pass one of its own).
inline std::string nodeRequirementReason(const TalentDefinition& d, const std::vector<const TalentDefinition*>& nodes, int level,
                                         const std::function<int(const std::string&)>& rankOf) {
    // What closes it outright first, then what it is waiting for.
    if (!d.fork.empty())
        for (const auto* other : nodes)
            if (other != &d && other->fork == d.fork && rankOf(other->id) > 0)
                return "You took " + other->ranks.front().name + " instead.";
    if (!d.prerequisites.empty() &&
        std::none_of(d.prerequisites.begin(), d.prerequisites.end(), [&](const std::string& id) { return rankOf(id) > 0; })) {
        std::string names;
        for (const auto& id : d.prerequisites) {
            const auto it = std::find_if(nodes.begin(), nodes.end(), [&](const TalentDefinition* n) { return n->id == id; });
            names += (names.empty() ? "" : " or ") + (it == nodes.end() ? id : (*it)->ranks.front().name);
        }
        return "Requires " + names + ".";
    }
    constexpr int levels[]{1,1,4,5}, investments[]{0,1,3,4};
    const auto tier = static_cast<std::size_t>(std::clamp(d.tier, 0, 3));
    if (level < levels[tier]) return "Requires character level " + std::to_string(levels[tier]) + ".";
    int invested = 0;
    for (const auto* other : nodes) if (other->tier < d.tier) invested += rankOf(other->id);
    if (invested < investments[tier]) return "Requires " + std::to_string(investments[tier]) + " ability points invested in this tree.";
    return {};
}
inline std::string nodeRequirementReason(const TalentDefinition& d, int level, const std::function<int(const std::string&)>& rankOf) {
    return nodeRequirementReason(d, treeNodes(d.treeId), level, rankOf);
}

inline Talent basicAttack() {
    Talent t; t.id="basic.attack"; t.name="Basic Attack"; t.description="A free adjacent strike. Also available by bumping an enemy.";
    t.power=4; t.cooldownTurns=1; t.scalingCooldown=1; return t;
}
// Everyone's shove: no damage, just a push, so any class can use the ground.
inline Talent basicShove() {
    Talent t; t.id="basic.shove"; t.name="Shove";
    t.description="Push an adjacent enemy one tile: into fire, live water, a brazier, a wall, another foe, or a chasm. No damage of its own.";
    t.power=0; t.damagePercent=0; t.pushDistance=1; t.cooldownTurns=3; t.scalingCooldown=3; return t;
}
// The Mage's starting light: a wisp that hangs where it was cast.
inline Talent basicLight() {
    Talent t; t.id="basic.light"; t.name="Conjure Light";
    t.description="Leave a wisp of light hovering where you stand, lighting five tiles around it for 40 turns. Only one wisp at a time; a new one replaces the old.";
    t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
    t.manaCost=3; t.cooldownTurns=10; t.scalingCooldown=10; t.conjureLight=true; return t;
}
// Granted when you swear to a patron (entities/Patrons.hpp).
inline Talent basicPray() {
    Talent t; t.id="basic.pray"; t.name="Pray";
    t.description="Call on your patron god. Needs 60 favor and spends 40; each god answers differently (see the shrine). Cooldown 20.";
    t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
    t.cooldownTurns=20; t.scalingCooldown=20; return t;
}
inline Talent basicCleanse() {
    Talent t; t.id="basic.cleanse"; t.name="Cleanse";
    t.description="Remove Poison, Burn, Chill, Marked and curses (Mana Drain / Doom). Free of mana; costs one turn. Cooldown 8. C always activates it. Does not remove Stun or stun recovery.";
    t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
    t.cooldownTurns=8; t.scalingCooldown=8; t.cleanse=true; return t;
}
} // namespace engine
