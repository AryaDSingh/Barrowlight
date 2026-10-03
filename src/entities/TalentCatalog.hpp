#pragma once

#include <array>
#include <algorithm>
#include <vector>
#include "entities/Talent.hpp"
#include "entities/PlayerClass.hpp"

namespace engine {
struct TreeDefinition {
    const char* id;
    const char* name;
    TalentTree tree;
    const char* description;
    const char* starterItem;
};
inline constexpr std::array<TreeDefinition, 22> kTalentTrees{{
    {"one_handed", "One-Handed", TalentTree::OneHanded, "Efficient strikes, defensive openings and finishing blows.", "iron_sword"},
    {"two_handed", "Two-Handed", TalentTree::TwoHanded, "Heavy swings, surrounding enemies and blood-fuelled attacks.", "greatsword"},
    {"shield", "Shield", TalentTree::Shield, "Block damage, interrupt attacks and control space.", "wooden_shield"},
    {"bow", "Bow", TalentTree::Bow, "Ranged pressure, clustered targets and critical shots.", "hunting_bow"},
    {"stealth", "Stealth", TalentTree::Stealth, "Concealment and ambush. Rank, DEX and distance oppose enemy detection rolls.", ""},
    {"acrobatics", "Acrobatics", TalentTree::Acrobatics, "Reposition, disengage and evade after movement abilities.", ""},
    {"fire", "Fire", TalentTree::Fire, "Burn enemies, spread fire and ignite movement abilities.", ""},
    {"ice", "Ice", TalentTree::Ice, "Chill reduces outgoing damage and slows movement; shatter chilled enemies.", ""},
    {"lightning", "Lightning", TalentTree::Lightning, "Burst, chaining and consuming Shock for a decisive hit.", ""},
    {"arcane", "Arcane", TalentTree::Arcane, "Flexible force, Blink and efficient spellcasting.", ""},
    {"cloth", "Cloth / Unarmoured", TalentTree::Cloth, "Requires cloth or no armour. Recover mana, evade and exploit elemental ailments. Available from level 5.", ""},
    {"light_armour", "Light Armour", TalentTree::LightArmour, "Requires light armour. Reposition for Opening, dodge and critical hits. Available from level 5.", ""},
    {"heavy_armour", "Heavy Armour", TalentTree::HeavyArmour, "Requires heavy armour. Wait to brace, withstand stuns and recover in battle. Available from level 5.", ""},
    {"spellblade", "Spellblade", TalentTree::Spellblade, "Imbue melee weapons and weave spells into strikes.", ""},
    {"animation", "Animation", TalentTree::Animation, "Raise undead allies and command the battlefield.", ""},
    {"blood_magic", "Blood Magic", TalentTree::BloodMagic, "Spend life to cast, drain enemies and defy death.", ""},
    {"shadow_archer", "Shadow Archer", TalentTree::ShadowArcher, "Bow and concealment: ambush, mark, and vanish.", ""},
    {"brawling", "Brawling", TalentTree::Brawling, "Charge, grab and throw: put enemies into fire, walls, each other and chasms. Any weapon or none.", ""},
    {"whip", "Whip", TalentTree::Whip, "Strike from two tiles away and drag enemies to you, through whatever lies between.", "leather_whip"},
    {"shadow", "Shadow", TalentTree::Shadow, "Darkness magic: snuff out the lights, blind your foes and strike from the dark.", ""},
    {"radiance", "Radiance", TalentTree::Radiance, "Light magic: sear the undead, blind with flares and bring back the dawn.", ""},
    {"alchemy", "Alchemy", TalentTree::Alchemy, "Thrown flasks of oil, fire and acid: make the ground fight for you.", ""},
}};
inline const TreeDefinition* findTree(const std::string& id) {
    for (const auto& t : kTalentTrees) if (id == t.id) return &t;
    return nullptr;
}
inline bool startingTreeAllowed(PlayerClass cls, TalentTree tree) {
    if (cls == PlayerClass::Warrior) return tree == TalentTree::OneHanded || tree == TalentTree::TwoHanded || tree == TalentTree::Shield || tree == TalentTree::Brawling;
    if (cls == PlayerClass::Thief) return tree == TalentTree::Stealth || tree == TalentTree::Bow || tree == TalentTree::Acrobatics || tree == TalentTree::Whip;
    return cls == PlayerClass::Mage && (tree == TalentTree::Fire || tree == TalentTree::Ice || tree == TalentTree::Lightning || tree == TalentTree::Arcane);
}
struct TalentDefinition {
    std::string id;
    std::string treeId;
    int tier = 0;
    std::array<Talent, kMaxTalentRank> ranks;
    std::string mastery; // what rank 5 adds beyond bigger numbers, if anything
};

// Rank 5 masteries: mostly utility rather than raw damage. Applied to the
// fifth rank only, after the ordinary rank curve.
inline void applyMastery(TalentDefinition& d) {
    Talent& m = d.ranks[kMaxTalentRank - 1];
    const auto mark = [&] { m.onHitEffect = StatusEffectInstance{StatusEffectType::Marked, 3, 1}; m.onHitChance = 1.f; };
    const auto longer = [&] { if (m.selfBuffEffect) ++m.selfBuffEffect->turnsRemaining; };
    const auto dodge = [&](int amount) { m.selfBuffEffect = StatusEffectInstance{StatusEffectType::Evasion, 1, amount}; };
    const auto stun = [&](int turns) { m.onHitEffect = StatusEffectInstance{StatusEffectType::Stun, turns, 0}; m.onHitChance = 1.f; };
    const std::string& id = d.id;
    if (id == "one_handed.quick_strike") { mark(); d.mastery = "Marks the target: its next direct hit taken deals +25%."; }
    else if (id == "one_handed.parry") { longer(); d.mastery = "Guard lasts one enemy response longer."; }
    else if (id == "one_handed.execution") { m.conditionalHpFraction = .4f; d.mastery = "Double damage from 40% HP instead of 30%."; }
    else if (id == "two_handed.cleave") { m.pushDistance = 1; d.mastery = "Pushes surviving targets one tile away."; }
    else if (id == "two_handed.fury") { m.hpCost = 0; d.mastery = "Costs no life."; }
    else if (id == "two_handed.whirlwind") { m.pushDistance = 2; d.mastery = "Pushes surviving targets two tiles away."; }
    else if (id == "shield.bash") { m.pushDistance = 2; d.mastery = "Pushes two tiles."; }
    else if (id == "shield.guard") { longer(); d.mastery = "Guard lasts one enemy response longer."; }
    else if (id == "shield.shockwave") { m.areaRadius = 2; d.mastery = "Reaches enemies up to two tiles away."; }
    else if (id == "bow.quick_shot") { mark(); d.mastery = "Marks the target: its next direct hit taken deals +25%."; }
    else if (id == "bow.volley") { m.areaRadius = 3; d.mastery = "The burst covers three tiles."; }
    else if (id == "bow.piercing_shot") { m.bonusCritChance += .2f; d.mastery = "A further +20% critical chance."; }
    else if (id == "stealth.conceal") { longer(); d.mastery = "Hide for four responses."; }
    else if (id == "stealth.strike") { m.retreatDistance = 2; d.mastery = "Slip up to two tiles away after striking."; }
    else if (id == "stealth.vanish_strike") { longer(); d.mastery = "Stay concealed for three responses."; }
    else if (id == "acrobatics.tumble") { dodge(15); d.mastery = "+15% dodge for one enemy response after tumbling."; }
    else if (id == "acrobatics.vault_kick") { stun(1); d.mastery = "The kick stuns for one enemy turn."; }
    else if (id == "acrobatics.leap") { if (m.selfBuffEffect) m.selfBuffEffect->magnitude = 30; d.mastery = "+30% dodge instead of +20%."; }
    else if (id == "fire.ember_bolt") { if (m.onHitEffect) m.onHitEffect->magnitude = 2; d.mastery = "Burn deals 2 damage per turn."; }
    else if (id == "fire.fireball") { m.areaRadius = 3; d.mastery = "The explosion covers three tiles."; }
    else if (id == "fire.meteor") { m.statusBonusPercent = 100; d.mastery = "Consuming Burn doubles the hit (+100%)."; }
    else if (id == "ice.shard") { if (m.onHitEffect) m.onHitEffect->turnsRemaining = 4; d.mastery = "Chill lasts four turns."; }
    else if (id == "ice.nova") { m.areaRadius = 2; d.mastery = "Reaches enemies up to two tiles away."; }
    else if (id == "ice.shatter") { m.statusBonusPercent = 100; d.mastery = "Consuming Chill doubles the hit (+100%)."; }
    else if (id == "lightning.bolt") { m.cooldownTurns = 1; d.mastery = "Cooldown 1."; }
    else if (id == "lightning.chain") { m.cooldownTurns = std::max(1, m.cooldownTurns - 2); d.mastery = "Cooldown two turns shorter."; }
    else if (id == "lightning.discharge") { m.statusBonusPercent = 100; d.mastery = "Consuming Shock doubles the hit (+100%)."; }
    else if (id == "arcane.bolt") { m.manaCost = std::max(1, m.manaCost / 2); d.mastery = "Costs half as much mana."; }
    else if (id == "arcane.blink") { m.cooldownTurns = std::max(1, m.cooldownTurns - 2); d.mastery = "Cooldown two turns shorter."; }
    else if (id == "arcane.mind_shatter") { stun(2); d.mastery = "Stuns for two enemy turns (bosses still resist repeats)."; }
    else if (id == "cloth.gather_mana") { m.restoreHpPercent = 10; d.mastery = "Also restores 10% of maximum life."; }
    else if (id == "cloth.pulse") { m.pushDistance = 3; d.mastery = "Pushes survivors three tiles."; }
    else if (id == "light_armour.sidestep") { dodge(15); d.mastery = "+15% dodge for one enemy response after the step."; }
    else if (id == "light_armour.parting_strike") { m.retreatDistance = 3; d.mastery = "Retreat three tiles instead of two."; }
    else if (id == "heavy_armour.shoulder_check") { stun(1); d.mastery = "The check stuns for one enemy turn."; }
    else if (id == "heavy_armour.second_wind") { m.cleanse = true; d.mastery = "Also removes Poison, Burn, Chill, Marked and curses."; }
    else if (id == "brawling.tackle") { m.pushDistance = 2; d.mastery = "Knocks the target two tiles."; }
    else if (id == "brawling.grapple") { m.selfBuffEffect = StatusEffectInstance{StatusEffectType::Guard, 2, 2}; d.mastery = "Bracing against your catch grants Guard 2 for two enemy responses."; }
    else if (id == "brawling.hurl") { m.domino = true; d.mastery = "Domino: a hurled enemy knocks whatever it hits one tile further."; }
    else if (id == "whip.lash") { m.pullDistance = 2; d.mastery = "Pulls two tiles."; }
    else if (id == "whip.trip") { stun(2); d.mastery = "The fall stuns for two enemy turns (bosses still resist repeats)."; }
    else if (id == "whip.snare") { if (m.onHitEffect) m.onHitEffect->turnsRemaining = 4; d.mastery = "Holds the snared enemy for four turns."; }
    else if (id == "shadow.bolt") { m.darkBonusPercent = 100; d.mastery = "+100% against a target in darkness."; }
    else if (id == "shadow.snuff") { m.selfBuffEffect = StatusEffectInstance{StatusEffectType::Concealed, 2, 2}; d.mastery = "You vanish into the new dark: Concealed for two responses."; }
    else if (id == "shadow.veil") { m.areaRadius = 2; d.mastery = "Blinds everything within two tiles."; }
    else if (id == "radiance.sear") { m.onHitEffect = StatusEffectInstance{StatusEffectType::Burn, 3, 2}; m.onHitChance = 1.f; d.mastery = "Sets the target burning (2 per turn)."; }
    else if (id == "radiance.flare") { m.areaRadius = 2; d.mastery = "The flare covers two tiles."; }
    else if (id == "radiance.dawn") { m.areaRadius = 4; d.mastery = "Reaches enemies up to four tiles away."; }
    else if (id == "alchemy.oil") { m.areaRadius = 2; d.mastery = "The flask splashes two tiles."; }
    else if (id == "alchemy.firebomb") { if (m.onHitEffect) m.onHitEffect->magnitude = 2; d.mastery = "Burn deals 2 damage per turn."; }
    else if (id == "alchemy.acid") { m.areaRadius = 2; d.mastery = "The flask splashes two tiles."; }
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
            if (tree == 10) t.armourRequirement=ArmourRequirement::Cloth;
            if (tree == 11) t.armourRequirement=ArmourRequirement::Light;
            if (tree == 12) t.armourRequirement=ArmourRequirement::Heavy;
            if (tree==13) t.weaponRequirement=WeaponRequirement::Melee;
            if (tree==16) t.weaponRequirement=WeaponRequirement::Bow;
            if (hybrid) t.scalingStat=tree==16?ScalingStat::Dexterity:ScalingStat::Intelligence;
            TalentDefinition d{id,kTalentTrees[tree].id,tier,{t,t,t,t,t}};
            // The rank curve, by rank index 1-4 (ranks 2-5).
            for (int rank=1; rank<kMaxTalentRank; ++rank) {
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
                for (int rank=1; rank<kMaxTalentRank; ++rank) d.ranks[rank].damagePercent=d.id=="bow.piercing_shot" ? piercing[rank] : fury[rank];
                for (int rank=2; rank<kMaxTalentRank; ++rank) d.ranks[rank].cooldownTurns=t.cooldownTurns-1;
            }
            for (int rank=0; rank<kMaxTalentRank; ++rank) {
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
            if ((tree>=6 && tree<=9) || tree==19 || tree==20) for (auto& r:d.ranks) if (!r.passive) r.manaCost*=2;
            if (tree>=10 && tree<=12) for (int rank=0; rank<kMaxTalentRank; ++rank) {
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
            if (hybrid) for (int rank=0;rank<kMaxTalentRank;++rank) {
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
            for (int rank=1; rank<kMaxTalentRank; ++rank) if (!d.ranks[rank].passive && d.ranks[0].manaCost>0)
                d.ranks[rank].manaCost=(d.ranks[0].manaCost*(100+10*rank)+50)/100;
            if (!hybrid) applyMastery(d);
            for (auto& r:d.ranks) r.tags=talentTags(r);
            out.push_back(std::move(d));
        };
        Talent t;
        add(0,"one_handed.quick_strike",0,attack("Quick Strike","A free, efficient melee strike.",4,0,1));
        add(0,"one_handed.parry",1,buff("Parry","Reduce incoming direct damage by 3 for two enemy responses.",StatusEffectType::Guard,2,3,2,5));
        add(0,"one_handed.riposte",2,passive("Riposte","While using one-handed weapons, Guard enables +4/5/6/7/8 direct melee damage.",PassiveKind::Riposte,4));
        t=attack("Execution","Double damage against an enemy at or below 30% HP.",8,4,5); t.conditionalHpFraction=.3f; t.conditionalMultiplier=2; add(0,"one_handed.execution",3,t);
        add(1,"two_handed.cleave",0,attack("Cleave","A heavy swing hitting all four adjacent tiles.",5,3,3,false,1));
        t=attack("Berserker's Fury","A heavy blow paid for with 5 HP.",14,0,4); t.hpCost=5; add(1,"two_handed.fury",1,t);
        add(1,"two_handed.bloodlust",2,passive("Bloodlust","With a two-handed weapon, direct damage gains +4/5/6/7/8 while at or below half HP.",PassiveKind::Bloodlust,4));
        t=attack("Whirlwind","Strike in a two-tile circle and push surviving targets one tile away.",8,6,6,false,2); t.pushDistance=1; add(1,"two_handed.whirlwind",3,t);
        t=attack("Shield Bash","Strike, push one tile and Mark for the next direct hit (+25%, 3 enemy turns).",4,1,3); t.pushDistance=1; t.onHitEffect=StatusEffectInstance{StatusEffectType::Marked,3,1}; add(2,"shield.bash",0,t);
        add(2,"shield.guard",1,buff("Guard","Reduce incoming direct damage by 4 for two enemy responses.",StatusEffectType::Guard,2,4,3,6));
        add(2,"shield.training",2,passive("Shield Training","While a shield is equipped, Guard blocks 4/5/6/7/8 extra damage per direct hit.",PassiveKind::ShieldTraining,4));
        t=attack("Shield Shockwave","Strike adjacent enemies and stun successful hits for one enemy turn.",5,5,7,false,1); t.onHitEffect=StatusEffectInstance{StatusEffectType::Stun,1,0}; add(2,"shield.shockwave",3,t);
        add(3,"bow.quick_shot",0,attack("Quick Shot","An efficient projectile intercepted by the first enemy.",5,2,1,true));
        t=attack("Volley","Burst in a two-tile circle and Mark survivors for the next direct hit (+25%, 3 enemy turns).",5,6,4,true,2);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Marked,3,1}; add(3,"bow.volley",1,t);
        add(3,"bow.marksmanship",2,passive("Marksmanship","Bow attacks gain 10/12/15/18/20% critical chance while you have Opening from waiting or movement abilities.",PassiveKind::Marksmanship,10));
        t=attack("Piercing Shot","A precision shot with +20% critical chance and 2x critical damage; stops at first enemy.",10,5,7,true); t.bonusCritChance=.2f; t.bonusCritDamageMultiplier=.5f; add(3,"bow.piercing_shot",3,t);
        add(4,"stealth.conceal",0,buff("Conceal","Hide for three responses. Nearby enemies roll to detect you: rank, your DEX and distance help; enemy DEX increases risk. Detection, attacks and damage reveal you.",StatusEffectType::Concealed,3,1,3,7));
        t=attack("Ambush Strike","A melee strike requiring Concealment; attacking reveals you.",9,2,4); t.requiresStealth=true; add(4,"stealth.strike",1,t);
        add(4,"stealth.ambush",2,passive("Ambush","Direct damage from Concealment gains +4/5/6/7/8 damage, including spells and ranged attacks.",PassiveKind::Ambush,4));
        t=attack("Vanish Strike","Strike, then retreat up to three tiles and gain Concealment for two enemy responses, using this ability's rank for detection.",8,5,7); t.retreatDistance=3; t.selfBuffEffect=StatusEffectInstance{StatusEffectType::Concealed,2,1}; add(4,"stealth.vanish_strike",3,t);
        add(5,"acrobatics.tumble",0,move("Tumble","Move up to three visible tiles, stopping before obstacles and actors.",3,2,4));
        t=attack("Vault Kick","Kick, then retreat up to three tiles even if the hit is dodged.",4,3,4); t.retreatDistance=3; add(5,"acrobatics.vault_kick",1,t);
        add(5,"acrobatics.footwork",2,passive("Footwork","Successful movement abilities grant +10/12/15/18/20% dodge for one enemy response.",PassiveKind::Footwork,10));
        t=move("Evasive Leap","Move up to four tiles and gain +20% dodge for two enemy responses.",4,4,6); t.selfBuffEffect=StatusEffectInstance{StatusEffectType::Evasion,2,20}; add(5,"acrobatics.leap",3,t);
        t=attack("Ember Bolt","A flame projectile; successful hits Burn for 1 damage per turn over three enemy turns.",4,2,1,true); t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,3,1}; add(6,"fire.ember_bolt",0,t);
        t=attack("Fireball","Explode at first impact, burning enemies for 1 damage per turn over three enemy turns.",5,6,5,true,2); t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,3,1}; add(6,"fire.fireball",1,t);
        add(6,"fire.kindle",2,passive("Kindle","Successful movement abilities Burn visible adjacent enemies for 4/5/6/7/8 damage per turn over two turns. Once per action; reveals you.",PassiveKind::Kindle,4));
        t=attack("Meteor","Consume an existing Burn on a successful hit for +50% direct damage.",11,9,7,true); t.consumeBurn=true; t.statusBonusPercent=50; add(6,"fire.meteor",3,t);
        t=attack("Ice Shard","Chill on hit: -20% outgoing damage and movement on alternate turns for three enemy turns.",4,2,2,true); t.onHitEffect=StatusEffectInstance{StatusEffectType::Chill,3,20}; add(7,"ice.shard",0,t);
        t=attack("Frost Nova","Chill adjacent enemies for three turns; helps create an escape.",4,4,5,false,1); t.onHitEffect=StatusEffectInstance{StatusEffectType::Chill,3,20}; add(7,"ice.nova",1,t);
        add(7,"ice.frostbite",2,passive("Frostbite","Direct hits against Chilled enemies deal +4/5/6/7/8 damage, from any tree.",PassiveKind::Frostbite,4));
        t=attack("Shatter","Consume Chill on hit for +50% direct damage and a one-turn Stun.",9,6,7,true); t.consumeChill=true; t.statusBonusPercent=50; add(7,"ice.shatter",3,t);
        add(8,"lightning.bolt",0,attack("Lightning Bolt","A direct lightning projectile.",5,2,2,true));
        t=attack("Chain Lightning","Jump to one visible enemy within three tiles of impact, for half damage. Terrain blocks the jump.",6,5,5,true); t.chain=true; add(8,"lightning.chain",1,t);
        add(8,"lightning.static_charge",2,passive("Static Charge","Direct Lightning hits apply Shock for 4/5/6/7/8 turns. Shock does not stack and empowers Discharge.",PassiveKind::StaticCharge,4));
        t=attack("Discharge","Consume Shock on a successful hit for +50% direct damage. Cannot reapply Shock.",11,6,7,true); t.consumeShock=true; t.statusBonusPercent=50; add(8,"lightning.discharge",3,t);
        t=attack("Arcane Bolt","Mark on a successful hit: next direct hit gains +25% damage, within 3 enemy turns.",3,2,1,true);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Marked,3,1}; add(9,"arcane.bolt",0,t);
        add(9,"arcane.blink",1,move("Blink","Move up to three visible tiles; terrain and actors block travel.",3,4,4));
        add(9,"arcane.efficiency",2,passive("Arcane Efficiency","Magic abilities cost 10/20/30/35/40% less mana (rounded down, minimum one).",PassiveKind::ArcaneEfficiency,10));
        t=attack("Mind Shatter","A direct force spell that stuns on a successful hit. Does not chain or travel as a projectile.",7,6,7,true); t.projectile=false; t.onHitEffect=StatusEffectInstance{StatusEffectType::Stun,1,0}; add(9,"arcane.mind_shatter",3,t);
        t=Talent{}; t.name="Gather Mana"; t.description="Spend a turn restoring 6/7/9/10/12 mana. Requires cloth or no armour.";
        t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.restoreMana=6; t.cooldownTurns=8;
        add(10,"cloth.gather_mana",0,t);
        add(10,"cloth.ward",1,passive("Loose Weave","With cloth or no armour and at least half mana, gain +8/10/12/14/16% dodge. Total dodge is capped at 60%.",PassiveKind::ClothWard,8));
        add(10,"cloth.spellweave",2,passive("Spellweave","With cloth or no armour, magic-tree hits against Burn, Chill or Shock gain +4/5/6/7/8 damage. Multiple ailments do not stack this bonus.",PassiveKind::Spellweave,4));
        t=attack("Repelling Pulse","With cloth or no armour, hit adjacent enemies and push survivors two tiles. Intelligence-scaled; creates room to cast.",5,6,7,false,1); t.pushDistance=2;
        add(10,"cloth.pulse",3,t);
        add(11,"light_armour.sidestep",0,move("Sidestep","Requires light armour. Move up to 2/2/3/3/4 visible tiles, gaining Opening and triggering movement talents.",2,0,5));
        add(11,"light_armour.evasion",1,passive("Agile Fit","With light armour and Opening from waiting or movement abilities, gain +8/10/12/14/16% dodge. Total dodge is capped at 60%.",PassiveKind::LightEvasion,8));
        add(11,"light_armour.precision",2,passive("Moving Aim","With light armour and Opening, all direct attacks gain +10/12/15/17/20% critical chance. Combines with Bow's Marksmanship.",PassiveKind::LightPrecision,10));
        t=attack("Parting Strike","Requires light armour. Strike and Mark an adjacent enemy, then retreat two tiles even on a miss. No weapon requirement.",6,3,6); t.retreatDistance=2; t.onHitEffect=StatusEffectInstance{StatusEffectType::Marked,3,1};
        add(11,"light_armour.parting_strike",3,t);
        t=attack("Shoulder Check","Requires heavy armour. Strike an adjacent enemy and push them one tile. No shield required.",5,2,4); t.pushDistance=1;
        add(12,"heavy_armour.shoulder_check",0,t);
        add(12,"heavy_armour.brace",1,passive("Brace","With heavy armour and Opening from waiting or movement abilities, reduce direct hits by 2/2/3/3/4 damage. Stacks with Guard; does not block damage over time.",PassiveKind::HeavyBrace,2));
        add(12,"heavy_armour.resolve",2,passive("Unyielding","While wearing heavy armour, gain a 20/25/30/35/40% chance to resist an incoming Stun. Existing stuns are not removed.",PassiveKind::HeavyResolve,20));
        t=Talent{}; t.name="Second Wind"; t.description="Requires heavy armour. Spend a turn recovering 10/12/15/17/20% of maximum HP (rounded up). Costs 4/4/5/5/6 mana, cooldown 10.";
        t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.restoreHpPercent=10; t.manaCost=4; t.cooldownTurns=10;
        add(12,"heavy_armour.second_wind",3,t);
        t=buff("Imbue Weapon","V: choose an owned element, then bind 1-9. Five turns or three landed melee hits; all variants share rank and cooldown. Melee weapon required.",StatusEffectType::FlameBlade,6,3,4,8);
        add(13,"spellblade.imbue",0,t);
        t=attack("Spellstrike","INT-scaled melee spell. Triggers Kindle on this hit, Frostbite, Static Charge and Spellweave when learned.",7,6,4); t.spellstrike=true;
        add(13,"spellblade.strike",1,t);
        add(13,"spellblade.rhythm",2,passive("Battle Rhythm","Casting a spell grants +4/5/6/7/8 damage to your next melee attack. A landed melee attack reduces one running spell cooldown by one, once per action.",PassiveKind::BattleRhythm,4));
        t=attack("Elemental Release","Consume Burn, Chill, Shock, Marked, Poison, Stun, Wither and Hunter's Mark on adjacent enemies: +25% damage per type consumed. Guard and buffs are preserved.",7,10,8,false,1); t.releaseAilments=true;
        add(13,"spellblade.release",3,t);
        t=Talent{}; t.name="Raise Skeleton"; t.description="Raise an adjacent ally. Cap: 1 + INT/10, maximum 5. Rank and INT improve its stats. Allies dissolve on travel."; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.summonCount=1; t.manaCost=8; t.cooldownTurns=5;
        add(14,"animation.raise",0,t);
        t=Talent{}; t.name="Bone Swap"; t.description="Swap with a visible allied skeleton. Empty ground still spends the cast. Counts as movement for your movement talents."; t.targeting=TargetingMode::RangedEnemyInSight; t.effectKind=TalentEffectKind::SelfBuff; t.boneSwap=true; t.manaCost=4; t.cooldownTurns=4;
        add(14,"animation.swap",1,t);
        add(14,"animation.pact",2,passive("Grave Pact","Slain minions explode for 4/5/6/7/8 damage to adjacent enemies. Expiration, gear-cap dissolution and travel never explode.",PassiveKind::GravePact,4));
        t=Talent{}; t.name="Army of the Dead"; t.description="Raise up to three adjacent temporary skeletons for 5/6/7/8/9 actions. They do not count toward the normal cap. Only one army at a time."; t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.summonCount=3; t.summonDuration=5; t.manaCost=16; t.cooldownTurns=12;
        add(14,"animation.army",3,t);
        add(15,"blood_magic.pact",0,buff("Blood Pact","For 3/4/5/6/7 subsequent actions, spells spend HP instead of mana. A spell cannot spend your last HP. Activating this pact costs mana.",StatusEffectType::BloodPact,4,1,2,9));
        t=attack("Drain Life","Direct spell: heal for 40/50/60/70/80% of actual HP removed, excluding overkill. Empty ground or a miss gives no healing.",7,8,5,true); t.projectile=false; t.drainPercent=40;
        add(15,"blood_magic.drain",1,t);
        add(15,"blood_magic.deathless",2,passive("Deathless","Once per newly explored floor, survive lethal damage at 1 HP. Revisiting floors or town does not recharge it. Ranks 2-5 also grant 1-4 Guard for one response.",PassiveKind::Deathless,1));
        t=buff("Wither","Curse visible ground for 3/4/5/6/7 enemy turns. Each direct hit you land on the cursed enemy heals 2 HP, capped by HP actually removed.",StatusEffectType::Wither,3,2,8,9); t.targeting=TargetingMode::RangedEnemyInSight;
        add(15,"blood_magic.wither",3,t);
        t=attack("Shadow Shot","Bow shot requiring Concealment: 35/45/55/65/75% chance to remain concealed after firing. Detection and damage can still reveal you.",7,4,4,true); t.requiresStealth=true; t.stayHiddenPercent=35;
        add(16,"shadow_archer.shot",0,t);
        t=buff("Hunter's Mark","Mark visible ground for 3 turns. That enemy's stealth detection chance is halved, with a 5% minimum when it can check. Casting reveals you.",StatusEffectType::HuntersMark,3,50,3,5); t.targeting=TargetingMode::RangedEnemyInSight; t.huntersMark=true;
        add(16,"shadow_archer.mark",1,t);
        add(16,"shadow_archer.unseen",2,passive("Unseen","A direct kill begun while Concealed refreshes concealment to 2/3/4/5/6 responses. At most once per Conceal cast; a qualifying kill can preserve your concealment.",PassiveKind::Unseen,2));
        t=attack("Death from Shadows","A heavy bow shot with +50% damage while Concealed. After firing, gain Concealment for two responses. Long cooldown; does not reset Unseen.",12,8,12,true); t.returnConcealed=true; t.selfBuffEffect=StatusEffectInstance{StatusEffectType::Concealed,2,1};
        add(16,"shadow_archer.death",3,t);
        // Brawling (STR, any weapon or none): putting enemies where they hurt.
        t=attack("Tackle","Charge up to 3/3/4/4/5 tiles in a straight line at an enemy, strike it and knock it back a tile. Counts as movement.",5,3,5);
        t.chargeDistance=3; t.pushDistance=1; add(17,"brawling.tackle",0,t);
        t=attack("Grapple","Seize an adjacent enemy for three enemy turns: it can't walk away, and when you step, you drag it into the tile you left (through fire, water, anything). Bosses and champions are too massive to hold.",3,2,6);
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Grappled,3,0}; add(17,"brawling.grapple",1,t);
        add(17,"brawling.hard_landing",2,passive("Hard Landing","Creatures you push, drag or throw take +2/3/4/5/6 more damage when they crash into a wall, a fixture or another creature.",PassiveKind::HardLanding,2));
        t=attack("Hurl","Heave an adjacent enemy over your shoulder: it lands up to 2/2/3/3/4 tiles behind you, crashing into whatever is there. A grappled enemy flies one tile further. Bosses and champions are too heavy to lift.",8,5,7);
        t.hurlDistance=2; add(17,"brawling.hurl",3,t);
        // Whip (DEX, needs a whip): reach and pull.
        t=attack("Lash","Strike an enemy up to two tiles away in a straight line and pull it one tile toward you, through fire, water or whatever lies between.",4,1,2);
        t.reach=2; t.pullDistance=1; add(18,"whip.lash",0,t);
        t=attack("Trip","Crack the whip at the legs of an enemy up to two tiles away: it falls, stunned for one enemy turn. Bosses resist repeated stuns.",4,3,6);
        t.reach=2; t.onHitEffect=StatusEffectInstance{StatusEffectType::Stun,1,0}; add(18,"whip.trip",1,t);
        add(18,"whip.flay",2,passive("Flay","Whip attacks deal +2/3/4/5/6 damage to enemies that are stunned, held, blinded, burning, chilled or shocked.",PassiveKind::Flay,2));
        t=attack("Snare","Lasso an enemy up to 3/3/4/4/5 tiles away, drag it right up to you and hold it fast for two turns (as Grapple). Bosses and champions won't budge.",6,4,7);
        t.reach=3; t.pullDistance=3; t.onHitEffect=StatusEffectInstance{StatusEffectType::Grappled,2,0}; add(18,"whip.snare",3,t);
        // Shadow (INT spells): darkness as a weapon.
        t=attack("Shadow Bolt","A bolt of darkness: +50% damage against a target standing in darkness.",5,2,1,true);
        t.darkBonusPercent=50; add(19,"shadow.bolt",0,t);
        t=Talent{}; t.name="Snuff"; t.description="Every torch, brazier, wisp and burning tile within four tiles goes out, your own light too (L relights it). Creatures that need light lose sight of you.";
        t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff; t.snuffRadius=4; t.manaCost=2; t.cooldownTurns=6;
        add(19,"shadow.snuff",1,t);
        add(19,"shadow.umbral",2,passive("Umbral Shroud","While your own light is out, you see three tiles into the dark, your spells deal +2/3/4/5/6 damage and you gain three times that much dodge (%).",PassiveKind::Umbral,2));
        t=attack("Veil of Night","Darkness swallows an enemy and those beside it: blinded for three enemy turns, they see only what is next to them.",4,5,8,true,1);
        t.projectile=false; t.onHitEffect=StatusEffectInstance{StatusEffectType::Blinded,3,0}; add(19,"shadow.veil",3,t);
        // Radiance (INT spells): light as a weapon.
        t=attack("Sear","A ray of light: +50% damage against undead and creatures that see in the dark.",5,2,1,true);
        t.searing=true; add(20,"radiance.sear",0,t);
        t=attack("Flare","A blinding burst: enemies within a tile are blinded for two enemy turns, hidden ones are revealed, and the spot stays lit for a while.",3,4,6,true,1);
        t.projectile=false; t.flare=true; t.onHitEffect=StatusEffectInstance{StatusEffectType::Blinded,2,0}; add(20,"radiance.flare",1,t);
        add(20,"radiance.inner_light",2,passive("Inner Light","Your light reaches one tile further, and your spells deal +2/3/4/5/6 damage to enemies standing in light.",PassiveKind::InnerLight,2));
        t=attack("Dawn","Light floods out three tiles, searing enemies (+50% against undead and darkvision), relighting every torch and brazier within six tiles and breaking any smothering darkness on you.",7,8,12,false,3);
        t.searing=true; t.dawn=true; add(20,"radiance.dawn",3,t);
        // Alchemy (DEX): flasks that leave something on the ground.
        t=attack("Oil Flask","Lob a flask that splashes oil over a tile and its neighbours. Oil burns long once lit.",2,2,4,false,1);
        t.targeting=TargetingMode::RangedEnemyInSight; t.shape=EffectShape::AreaAroundTarget; t.splashSurface=1; add(21,"alchemy.oil",0,t);
        t=attack("Firebomb","A flask of fire: burns everything within a tile for 1 per turn and sets the ground alight.",5,4,6,false,1);
        t.targeting=TargetingMode::RangedEnemyInSight; t.shape=EffectShape::AreaAroundTarget; t.splashSurface=3; t.splashTurns=3;
        t.onHitEffect=StatusEffectInstance{StatusEffectType::Burn,3,1}; add(21,"alchemy.firebomb",1,t);
        add(21,"alchemy.brews",2,passive("Potent Brews","Your flasks deal +2/3/4/5/6 damage.",PassiveKind::PotentBrews,2));
        t=attack("Acid Flask","Splash acid within a tile for eight turns: anything standing in it takes 2 a turn and is Marked (its next direct hit taken deals +25%).",4,5,8,false,1);
        t.targeting=TargetingMode::RangedEnemyInSight; t.shape=EffectShape::AreaAroundTarget; t.splashSurface=7; t.splashTurns=8; add(21,"alchemy.acid",3,t);

        // Ascendancy nodes (Ascendancy.hpp): one rank, bought with ascendancy points.
        auto node=[&](const char* treeId,const char* id,ScalingStat stat,Talent t) {
            t.id=id; t.tree=TalentTree::Blade; t.scalingStat=stat; t.scalingCooldown=t.cooldownTurns;
            TalentDefinition d; d.id=id; d.treeId=treeId; d.tier=0; d.ranks={t,t,t,t,t};
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
        node("trickster","trickster.quick_hands",Dex,passive("Quick Hands","+8% dodge chance. Total dodge is capped at 60%.",PassiveKind::QuickHands,8));
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
inline Talent basicCleanse() {
    Talent t; t.id="basic.cleanse"; t.name="Cleanse";
    t.description="Remove Poison, Burn, Chill, Marked and curses (Mana Drain / Doom). Free of mana; costs one turn. Cooldown 8. C always activates it. Does not remove Stun or stun recovery.";
    t.targeting=TargetingMode::Self; t.effectKind=TalentEffectKind::SelfBuff;
    t.cooldownTurns=8; t.scalingCooldown=8; t.cleanse=true; return t;
}
} // namespace engine
