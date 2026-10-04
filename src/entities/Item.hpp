#pragma once

#include "entities/Entity.hpp"
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace engine {

// Fixed equipment definitions plus uniquely identified, movable item instances.
// Preserve the original four serialized slot numbers.
enum class EquipmentSlot { Weapon, Armour, Charm, OffHand, Head, Cloak, Hands, Belt, Feet, Ring1, Ring2 };
inline constexpr int kEquipmentSlotCount=11;
inline bool ringSlot(EquipmentSlot s) { return s==EquipmentSlot::Ring1 || s==EquipmentSlot::Ring2; }
inline bool slotAccepts(EquipmentSlot target,EquipmentSlot item) { return target==item || (ringSlot(target) && ringSlot(item)); }
inline bool armourSlot(EquipmentSlot s) { return s==EquipmentSlot::Armour || s==EquipmentSlot::Head || s==EquipmentSlot::Hands || s==EquipmentSlot::Feet; }
enum class ArmourKind { Unarmoured, Cloth, Light, Heavy };
inline const char* armourName(ArmourKind kind) {
    switch (kind) {
        case ArmourKind::Cloth: return "Cloth";
        case ArmourKind::Light: return "Light armour";
        case ArmourKind::Heavy: return "Heavy armour";
        default: return "Unarmoured";
    }
}

enum class WeaponKind { None, OneHanded, TwoHanded, Bow, Staff, Shield, Whip, Spear, Dagger, Mace, Crossbow };
// Weapons held in both hands: no off-hand with these.
inline bool twoHandedKind(WeaponKind k) { return k==WeaponKind::TwoHanded || k==WeaponKind::Bow || k==WeaponKind::Spear || k==WeaponKind::Crossbow; }
inline const char* slotName(EquipmentSlot slot) {
    switch (slot) {
        case EquipmentSlot::Weapon: return "Main hand";
        case EquipmentSlot::Armour: return "Body";
        case EquipmentSlot::Charm: return "Amulet";
        case EquipmentSlot::OffHand: return "Off-hand";
        case EquipmentSlot::Head: return "Head";
        case EquipmentSlot::Cloak: return "Cloak";
        case EquipmentSlot::Hands: return "Hands";
        case EquipmentSlot::Belt: return "Belt";
        case EquipmentSlot::Feet: return "Feet";
        case EquipmentSlot::Ring1: return "Ring 1";
        case EquipmentSlot::Ring2: return "Ring 2";
    }
    return "Unknown";
}

struct ItemBonuses {
    int strength = 0, dexterity = 0, intelligence = 0;
    int maxHp = 0, maxMana = 0;
};
// Normal/Magic/Rare count rolled affixes. Unique items are hand-made
// definitions with fixed bonuses and no affixes; they only come from the
// very rare deep-floor events.
enum class ItemRarity { Normal, Magic, Rare, Unique };
inline ItemRarity rarityForAffixes(std::size_t count) {
    return count == 0 ? ItemRarity::Normal : count <= 2 ? ItemRarity::Magic : ItemRarity::Rare;
}
// Magic items carry up to one prefix and one suffix; rares up to three of each.
inline constexpr int kMaxAffixes = 6;
inline const char* rarityName(ItemRarity rarity) {
    return rarity == ItemRarity::Unique ? "Unique" : rarity == ItemRarity::Rare ? "Rare" : rarity == ItemRarity::Magic ? "Magic" : "Normal";
}
// The first five raise attributes; the rest are effects read in combat
// (see Inventory::affixTotal and the hooks in TalentEffects/Application).
enum class BonusStat { Strength, Dexterity, Intelligence, Hp, Mana,
                       BurnChance, ChillChance, ShockChance, LifeOnHit, ManaOnKill, Thorns, Dodge, CritChance,
                       CritDamage, Regeneration, Warding, LightRadius, Execution, FlatDamage,
                       // Defences, more on-hit effects, conditional damage and ailment wards.
                       Armour, Evasion, Ward, StunChance, BleedChance, BlindChance,
                       DarkDamage, LightDamage, WaterDamage, LowLifeDamage, UnawareDamage,
                       ResistBurn, ResistChill, ResistShock, ResistPoison };
struct AffixDefinition {
    const char* id;
    const char* name;
    BonusStat stat; // same-stat affixes are mutually exclusive
    unsigned int slots; // weapon=1, armour=2, charm=4
    int minimum, maximum, perTier;
    const char* title = nullptr; // how it names a magic item: "Vampiric" sword, sword "of Embers"
    bool prefix = false;
    // Cursed affixes: strong, but each point also costs penaltyPercent/100
    // of the penalty stat. They only ever roll on rares.
    bool cursed = false;
    BonusStat penalty = BonusStat::Hp;
    int penaltyPercent = 0;
};
// Slot bits: weapon 1, body 2, amulet 4, off-hand 8, head 16, cloak 32,
// hands 64, belt 128, feet 256, rings 512.
inline constexpr std::array<AffixDefinition, 41> kAffixes{{
    {"might", "Strength", BonusStat::Strength, 5, 2, 4, 1, "of Might"},
    {"agility", "Dexterity", BonusStat::Dexterity, 7 | 2032, 1, 3, 1, "of Agility"},
    {"knowledge", "Intelligence", BonusStat::Intelligence, 5, 2, 4, 1, "of Knowledge"},
    {"vitality", "Max HP", BonusStat::Hp, 14 | 2032, 3, 6, 2, "of Vitality"},
    {"reservoir", "Max mana", BonusStat::Mana, 14 | 2032, 3, 6, 2, "of the Well"},
    {"brawn", "Strength", BonusStat::Strength, 10, 1, 2, 1, "Brawny", true},
    {"insight", "Intelligence", BonusStat::Intelligence, 2, 1, 2, 1, "Insightful", true},
    {"vigor", "Max HP", BonusStat::Hp, 1, 2, 3, 2, "Hale", true},
    // Effects, not just numbers.
    {"embers", "Burn chance", BonusStat::BurnChance, 1, 15, 25, 3, "of Embers"},
    {"frost", "Chill chance", BonusStat::ChillChance, 1, 15, 25, 3, "of Frost"},
    {"storms", "Shock chance", BonusStat::ShockChance, 1 | 512, 15, 25, 3, "of Storms"},
    {"vampiric", "Life on hit", BonusStat::LifeOnHit, 1 | 512, 1, 2, 0, "Vampiric", true},
    {"siphon", "Mana on kill", BonusStat::ManaOnKill, 4 | 512 | 1, 3, 5, 1, "of Siphoning"},
    {"thorns", "Thorns", BonusStat::Thorns, 2 | 8 | 16, 2, 4, 1, "of Thorns"},
    {"nimble", "Dodge", BonusStat::Dodge, 256 | 32 | 2, 3, 5, 1, "Nimble", true},
    {"precise", "Critical chance", BonusStat::CritChance, 64 | 512, 3, 6, 1, "Precise", true},
    {"savage", "Critical damage", BonusStat::CritDamage, 1 | 64, 15, 25, 5, "Savage", true},
    {"regeneration", "Regeneration", BonusStat::Regeneration, 128 | 4 | 2, 1, 2, 1, "of Regeneration"},
    {"warding", "Warding", BonusStat::Warding, 8 | 16 | 2, 1, 2, 0, "Warding", true},
    {"radiant", "Light", BonusStat::LightRadius, 16 | 4, 1, 1, 0, "Radiant", true},
    {"execution", "Execution", BonusStat::Execution, 1, 3, 5, 1, "of Execution"},
    {"cruel", "Damage", BonusStat::FlatDamage, 1, 1, 3, 1, "Cruel", true},
    // Defences.
    {"reinforced", "Armour", BonusStat::Armour, 2 | 8 | 16 | 64 | 128 | 256, 3, 6, 2, "Reinforced", true},
    {"supple", "Evasion", BonusStat::Evasion, 2 | 16 | 32 | 64 | 256, 1, 3, 1, "Supple", true},
    {"runed", "Ward", BonusStat::Ward, 2 | 4 | 16 | 64 | 256, 3, 6, 2, "Runed", true},
    // On hit (maces stun, spears bleed, whips blind: their bases carry these).
    {"stunning", "Stun chance", BonusStat::StunChance, 1, 5, 10, 2, "of Concussion"},
    {"rending", "Bleed chance", BonusStat::BleedChance, 1 | 64, 10, 20, 3, "Rending", true},
    {"blinding", "Blind chance", BonusStat::BlindChance, 1 | 8, 5, 12, 2, "of Glare"},
    // Damage, when the moment is right.
    {"nocturnal", "Damage in darkness", BonusStat::DarkDamage, 1 | 16 | 32 | 64 | 512, 2, 4, 1, "Nocturnal", true},
    {"dawnlit", "Damage in light", BonusStat::LightDamage, 1 | 4 | 16 | 64 | 512, 2, 4, 1, "Dawnlit", true},
    {"drowned", "Damage in water", BonusStat::WaterDamage, 1 | 128 | 256 | 512, 2, 4, 1, "Drowned", true},
    {"desperate", "Damage at low life", BonusStat::LowLifeDamage, 1 | 2 | 4 | 128, 3, 5, 1, "Desperate", true},
    {"stalking", "Damage to the unaware", BonusStat::UnawareDamage, 1 | 32 | 64 | 256, 3, 6, 1, "Stalking", true},
    // Shaking off ailments.
    {"salamander", "Burn ward", BonusStat::ResistBurn, 2 | 4 | 8 | 16 | 32 | 128 | 512, 15, 30, 3, "of the Salamander"},
    {"hearth", "Chill ward", BonusStat::ResistChill, 2 | 4 | 8 | 16 | 32 | 128 | 512, 15, 30, 3, "of the Hearth"},
    {"grounding", "Shock ward", BonusStat::ResistShock, 2 | 4 | 8 | 16 | 32 | 128 | 512, 15, 30, 3, "of Grounding"},
    {"antidote", "Poison ward", BonusStat::ResistPoison, 2 | 4 | 8 | 16 | 32 | 128 | 512, 15, 30, 3, "of the Antidote"},
    // Cursed: strong, at a price.
    {"bloodthirsty", "Damage", BonusStat::FlatDamage, 1 | 64 | 512, 3, 5, 1, "Bloodthirsty", true, true, BonusStat::Hp, 400},
    {"reckless", "Critical damage", BonusStat::CritDamage, 1 | 64 | 512, 40, 60, 5, "Reckless", true, true, BonusStat::Dodge, 20},
    {"gaunt", "Max mana", BonusStat::Mana, 2 | 4 | 16 | 512, 10, 15, 3, "of the Gaunt", false, true, BonusStat::Hp, 100},
    {"frenzy", "Life on hit", BonusStat::LifeOnHit, 1 | 512, 3, 4, 1, "of Frenzy", false, true, BonusStat::Armour, 300},
}};
inline bool attributeAffix(BonusStat stat) { return stat <= BonusStat::Mana; }
// How a rolled affix reads in a tooltip.
inline std::string affixText(const AffixDefinition& a, int v) {
    const std::string n = std::to_string(v);
    switch (a.stat) {
        case BonusStat::BurnChance: return n + "% chance to Burn on hit";
        case BonusStat::ChillChance: return n + "% chance to Chill on hit";
        case BonusStat::ShockChance: return n + "% chance to Shock on hit";
        case BonusStat::LifeOnHit: return "+" + n + " life on every hit";
        case BonusStat::ManaOnKill: return "+" + n + " mana on every kill";
        case BonusStat::Thorns: return "Melee attackers take " + n + " damage";
        case BonusStat::Dodge: return "+" + n + "% dodge chance";
        case BonusStat::CritChance: return "+" + n + "% critical chance";
        case BonusStat::CritDamage: return "+" + n + "% critical damage";
        case BonusStat::Regeneration: return "Regenerate " + n + " life every 5 turns";
        case BonusStat::Warding: return "Direct hits on you deal " + n + " less";
        case BonusStat::LightRadius: return "+" + n + " light radius";
        case BonusStat::Execution: return "+" + n + " damage to enemies below 30% life";
        case BonusStat::FlatDamage: return "+" + n + " damage on every attack";
        case BonusStat::Armour: return "+" + n + " armour";
        case BonusStat::Evasion: return "+" + n + " evasion";
        case BonusStat::Ward: return "+" + n + " ward";
        case BonusStat::StunChance: return n + "% chance to Stun on hit";
        case BonusStat::BleedChance: return n + "% chance to cause Bleeding on hit";
        case BonusStat::BlindChance: return n + "% chance to Blind on hit";
        case BonusStat::DarkDamage: return "+" + n + " damage while you stand in darkness";
        case BonusStat::LightDamage: return "+" + n + " damage while you stand in light";
        case BonusStat::WaterDamage: return "+" + n + " damage while you stand in water";
        case BonusStat::LowLifeDamage: return "+" + n + " damage while below half life";
        case BonusStat::UnawareDamage: return "+" + n + " damage to foes that haven't noticed you";
        case BonusStat::ResistBurn: return n + "% chance each turn to shake off burning";
        case BonusStat::ResistChill: return n + "% chance each turn to shake off chill";
        case BonusStat::ResistShock: return n + "% chance each turn to shake off shock";
        case BonusStat::ResistPoison: return n + "% chance each turn to shake off poison";
        default: return std::string(a.name) + " +" + n;
    }
}
// What a cursed affix costs, for the tooltip.
inline std::string affixPenaltyText(const AffixDefinition& a, int v) {
    const int cost = v * a.penaltyPercent / 100;
    switch (a.penalty) {
        case BonusStat::Hp: return "-" + std::to_string(cost) + " max life";
        case BonusStat::Dodge: return "-" + std::to_string(cost) + "% dodge chance";
        case BonusStat::Armour: return "-" + std::to_string(cost) + " armour";
        default: return "-" + std::to_string(cost);
    }
}
struct RolledAffix { std::string id; int value = 0; };
inline const AffixDefinition* findAffix(std::string_view id) {
    for (const auto& affix : kAffixes) if (id == affix.id) return &affix;
    return nullptr;
}
inline void addBonus(ItemBonuses& bonus, BonusStat stat, int value) {
    switch (stat) {
        case BonusStat::Strength: bonus.strength += value; break;
        case BonusStat::Dexterity: bonus.dexterity += value; break;
        case BonusStat::Intelligence: bonus.intelligence += value; break;
        case BonusStat::Hp: bonus.maxHp += value; break;
        case BonusStat::Mana: bonus.maxMana += value; break;
        default: break; // effect affixes are read in combat, not added to attributes
    }
}

struct ItemDefinition {
    const char* id;
    const char* name;
    EquipmentSlot slot;
    ItemBonuses bonuses;
    WeaponKind weaponKind = WeaponKind::None;
    ArmourKind armourKind = ArmourKind::Unarmoured;
    bool unique = false;
    const char* lore = nullptr; // uniques' flavour line
    int damage = 0;   // weapons: added to every attack (a staff's to spells instead)
    int defence = 0;  // armour by kind: heavy = armour, light = evasion, cloth = ward; shields count as armour
    int depth = 1;    // the shallowest depth it drops at; 0 = never (training gear)
    int reqStrength = 0, reqDexterity = 0, reqIntelligence = 0;
    const char* implicit = nullptr; // an affix every one of this base carries
    int implicitValue = 0;
};

inline std::string equipmentTypeName(const ItemDefinition& item) {
    return armourSlot(item.slot) ? std::string(slotName(item.slot))+" / "+armourName(item.armourKind) : slotName(item.slot);
}

// IDs are persistent identities. Display names can change independently.
inline constexpr std::array<ItemDefinition, 105> kItemDefinitions{{
    {"iron_sword", "Iron Sword", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::OneHanded, ArmourKind::Unarmoured, false, nullptr, 2, 0, 1, 0, 0, 0, "precise", 2},
    {"ash_staff", "Ash Staff", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Staff, ArmourKind::Unarmoured, false, nullptr, 2, 0, 1, 0, 0, 0, "reservoir", 4},
    {"hunting_bow", "Hunting Bow", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Bow, ArmourKind::Unarmoured, false, nullptr, 2, 0, 1, 0, 0, 0, "nimble", 1},
    {"chain_coat", "Chain Coat", EquipmentSlot::Armour, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Heavy, false, nullptr, 0, 8, 1, 0, 0, 0, nullptr, 0},
    {"scout_leathers", "Scout Leathers", EquipmentSlot::Armour, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Light, false, nullptr, 0, 4, 1, 0, 0, 0, nullptr, 0},
    {"woven_robes", "Woven Robes", EquipmentSlot::Armour, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Cloth, false, nullptr, 0, 6, 1, 0, 0, 0, nullptr, 0},
    {"vitality_charm", "Vitality Amulet", EquipmentSlot::Charm, {2, 0, 0, 4, 0}, WeaponKind::None, ArmourKind::Unarmoured, false, nullptr, 0, 0, 1, 0, 0, 0, nullptr, 0},
    {"focus_charm", "Focus Amulet", EquipmentSlot::Charm, {0, 0, 2, 0, 5}, WeaponKind::None, ArmourKind::Unarmoured, false, nullptr, 0, 0, 1, 0, 0, 0, nullptr, 0},
    {"agility_charm", "Agility Amulet", EquipmentSlot::Charm, {0, 3, 0, 2, 0}, WeaponKind::None, ArmourKind::Unarmoured, false, nullptr, 0, 0, 1, 0, 0, 0, nullptr, 0},
    {"greatsword", "Greatsword", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::TwoHanded, ArmourKind::Unarmoured, false, nullptr, 3, 0, 1, 0, 0, 0, "savage", 10},
    {"wooden_shield", "Wooden Shield", EquipmentSlot::OffHand, {0, 0, 0, 0, 0}, WeaponKind::Shield, ArmourKind::Unarmoured, false, nullptr, 0, 4, 1, 0, 0, 0, "warding", 1},
    {"training_sword", "Training Sword", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::OneHanded, ArmourKind::Unarmoured, false, nullptr, 1, 0, 0, 0, 0, 0, nullptr, 0},
    {"training_greatsword", "Training Greatsword", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::TwoHanded, ArmourKind::Unarmoured, false, nullptr, 1, 0, 0, 0, 0, 0, nullptr, 0},
    {"training_bow", "Training Bow", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Bow, ArmourKind::Unarmoured, false, nullptr, 1, 0, 0, 0, 0, 0, nullptr, 0},
    {"training_shield", "Training Shield", EquipmentSlot::OffHand, {0, 0, 0, 0, 0}, WeaponKind::Shield, ArmourKind::Unarmoured, false, nullptr, 0, 2, 0, 0, 0, 0, nullptr, 0},
    {"leather_whip", "Leather Whip", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Whip, ArmourKind::Unarmoured, false, nullptr, 2, 0, 1, 0, 0, 0, "blinding", 5},
    {"training_whip", "Training Whip", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Whip, ArmourKind::Unarmoured, false, nullptr, 1, 0, 0, 0, 0, 0, nullptr, 0},
    {"iron_spear", "Iron Spear", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Spear, ArmourKind::Unarmoured, false, nullptr, 3, 0, 1, 0, 0, 0, "rending", 10},
    {"training_spear", "Training Spear", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Spear, ArmourKind::Unarmoured, false, nullptr, 1, 0, 0, 0, 0, 0, nullptr, 0},
    {"steel_dagger", "Steel Dagger", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Dagger, ArmourKind::Unarmoured, false, nullptr, 2, 0, 1, 0, 0, 0, "precise", 4},
    {"training_dagger", "Training Dagger", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Dagger, ArmourKind::Unarmoured, false, nullptr, 1, 0, 0, 0, 0, 0, nullptr, 0},
    {"iron_mace", "Iron Mace", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Mace, ArmourKind::Unarmoured, false, nullptr, 2, 0, 1, 0, 0, 0, "stunning", 5},
    {"training_mace", "Training Mace", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Mace, ArmourKind::Unarmoured, false, nullptr, 1, 0, 0, 0, 0, 0, nullptr, 0},
    {"light_crossbow", "Light Crossbow", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Crossbow, ArmourKind::Unarmoured, false, nullptr, 3, 0, 1, 0, 0, 0, "execution", 2},
    {"training_crossbow", "Training Crossbow", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Crossbow, ArmourKind::Unarmoured, false, nullptr, 1, 0, 0, 0, 0, 0, nullptr, 0},
    {"cloth_hood", "Cloth Hood", EquipmentSlot::Head, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Cloth, false, nullptr, 0, 3, 1, 0, 0, 0, nullptr, 0},
    {"leather_cap", "Leather Cap", EquipmentSlot::Head, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Light, false, nullptr, 0, 1, 1, 0, 0, 0, nullptr, 0},
    {"iron_helm", "Iron Helm", EquipmentSlot::Head, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Heavy, false, nullptr, 0, 3, 1, 0, 0, 0, nullptr, 0},
    {"cloth_gloves", "Cloth Gloves", EquipmentSlot::Hands, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Cloth, false, nullptr, 0, 3, 1, 0, 0, 0, nullptr, 0},
    {"leather_gloves", "Leather Gloves", EquipmentSlot::Hands, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Light, false, nullptr, 0, 1, 1, 0, 0, 0, nullptr, 0},
    {"iron_gauntlets", "Iron Gauntlets", EquipmentSlot::Hands, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Heavy, false, nullptr, 0, 3, 1, 0, 0, 0, nullptr, 0},
    {"cloth_slippers", "Cloth Slippers", EquipmentSlot::Feet, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Cloth, false, nullptr, 0, 3, 1, 0, 0, 0, nullptr, 0},
    {"leather_boots", "Leather Boots", EquipmentSlot::Feet, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Light, false, nullptr, 0, 1, 1, 0, 0, 0, nullptr, 0},
    {"iron_boots", "Iron Boots", EquipmentSlot::Feet, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Heavy, false, nullptr, 0, 3, 1, 0, 0, 0, nullptr, 0},
    {"traveler_cloak", "Traveler Cloak", EquipmentSlot::Cloak, {0, 0, 0, 2, 0}, WeaponKind::None, ArmourKind::Unarmoured, false, nullptr, 0, 0, 1, 0, 0, 0, nullptr, 0},
    {"sturdy_belt", "Sturdy Belt", EquipmentSlot::Belt, {0, 0, 0, 2, 0}, WeaponKind::None, ArmourKind::Unarmoured, false, nullptr, 0, 0, 1, 0, 0, 0, nullptr, 0},
    {"copper_ring", "Copper Ring", EquipmentSlot::Ring1, {1, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Unarmoured, false, nullptr, 0, 0, 1, 0, 0, 0, nullptr, 0},
    {"silver_ring", "Silver Ring", EquipmentSlot::Ring1, {0, 0, 1, 0, 0}, WeaponKind::None, ArmourKind::Unarmoured, false, nullptr, 0, 0, 1, 0, 0, 0, nullptr, 0},
    {"steel_longsword", "Steel Longsword", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::OneHanded, ArmourKind::Unarmoured, false, nullptr, 4, 0, 4, 10, 5, 0, "precise", 3},
    {"knights_blade", "Knight's Blade", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::OneHanded, ArmourKind::Unarmoured, false, nullptr, 6, 0, 8, 16, 8, 0, "precise", 4},
    {"runed_blade", "Runed Blade", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::OneHanded, ArmourKind::Unarmoured, false, nullptr, 9, 0, 13, 24, 12, 0, "precise", 5},
    {"executioners_sword", "Executioner's Sword", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::TwoHanded, ArmourKind::Unarmoured, false, nullptr, 6, 0, 4, 10, 0, 0, "savage", 15},
    {"zweihander", "Zweihander", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::TwoHanded, ArmourKind::Unarmoured, false, nullptr, 9, 0, 8, 16, 0, 0, "savage", 20},
    {"grave_claymore", "Grave Claymore", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::TwoHanded, ArmourKind::Unarmoured, false, nullptr, 13, 0, 13, 24, 0, 0, "savage", 25},
    {"recurve_bow", "Recurve Bow", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Bow, ArmourKind::Unarmoured, false, nullptr, 4, 0, 4, 0, 10, 0, "nimble", 2},
    {"longbow", "Longbow", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Bow, ArmourKind::Unarmoured, false, nullptr, 6, 0, 8, 0, 16, 0, "nimble", 3},
    {"bone_warbow", "Bone Warbow", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Bow, ArmourKind::Unarmoured, false, nullptr, 9, 0, 13, 0, 24, 0, "nimble", 4},
    {"oak_staff", "Oak Staff", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Staff, ArmourKind::Unarmoured, false, nullptr, 4, 0, 4, 0, 0, 10, "reservoir", 6},
    {"bone_staff", "Bone Staff", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Staff, ArmourKind::Unarmoured, false, nullptr, 6, 0, 8, 0, 0, 16, "reservoir", 9},
    {"gravewood_staff", "Gravewood Staff", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Staff, ArmourKind::Unarmoured, false, nullptr, 9, 0, 13, 0, 0, 24, "reservoir", 12},
    {"barbed_whip", "Barbed Whip", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Whip, ArmourKind::Unarmoured, false, nullptr, 3, 0, 4, 0, 10, 0, "blinding", 7},
    {"chain_whip", "Chain Whip", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Whip, ArmourKind::Unarmoured, false, nullptr, 5, 0, 8, 0, 16, 0, "blinding", 9},
    {"flayers_lash", "Flayer's Lash", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Whip, ArmourKind::Unarmoured, false, nullptr, 8, 0, 13, 0, 24, 0, "blinding", 12},
    {"boar_spear", "Boar Spear", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Spear, ArmourKind::Unarmoured, false, nullptr, 5, 0, 4, 10, 5, 0, "rending", 14},
    {"war_pike", "War Pike", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Spear, ArmourKind::Unarmoured, false, nullptr, 8, 0, 8, 16, 8, 0, "rending", 18},
    {"ossuary_pike", "Ossuary Pike", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Spear, ArmourKind::Unarmoured, false, nullptr, 12, 0, 13, 24, 12, 0, "rending", 24},
    {"stiletto", "Stiletto", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Dagger, ArmourKind::Unarmoured, false, nullptr, 3, 0, 4, 0, 10, 0, "precise", 6},
    {"kris", "Kris", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Dagger, ArmourKind::Unarmoured, false, nullptr, 5, 0, 8, 0, 16, 0, "precise", 8},
    {"grave_fang", "Grave Fang", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Dagger, ArmourKind::Unarmoured, false, nullptr, 7, 0, 13, 0, 24, 0, "precise", 10},
    {"flanged_mace", "Flanged Mace", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Mace, ArmourKind::Unarmoured, false, nullptr, 4, 0, 4, 10, 0, 0, "stunning", 7},
    {"morningstar", "Morningstar", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Mace, ArmourKind::Unarmoured, false, nullptr, 7, 0, 8, 16, 0, 0, "stunning", 9},
    {"crypt_maul", "Crypt Maul", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Mace, ArmourKind::Unarmoured, false, nullptr, 10, 0, 13, 24, 0, 0, "stunning", 12},
    {"heavy_crossbow", "Heavy Crossbow", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Crossbow, ArmourKind::Unarmoured, false, nullptr, 5, 0, 4, 5, 10, 0, "execution", 3},
    {"arbalest", "Arbalest", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Crossbow, ArmourKind::Unarmoured, false, nullptr, 8, 0, 8, 8, 16, 0, "execution", 4},
    {"siege_crossbow", "Siege Crossbow", EquipmentSlot::Weapon, {0, 0, 0, 0, 0}, WeaponKind::Crossbow, ArmourKind::Unarmoured, false, nullptr, 11, 0, 13, 12, 24, 0, "execution", 6},
    {"kite_shield", "Kite Shield", EquipmentSlot::OffHand, {0, 0, 0, 0, 0}, WeaponKind::Shield, ArmourKind::Unarmoured, false, nullptr, 0, 8, 4, 8, 0, 0, "warding", 1},
    {"tower_shield", "Tower Shield", EquipmentSlot::OffHand, {0, 0, 0, 0, 0}, WeaponKind::Shield, ArmourKind::Unarmoured, false, nullptr, 0, 13, 8, 14, 0, 0, "warding", 2},
    {"wardens_bulwark", "Warden's Bulwark", EquipmentSlot::OffHand, {0, 0, 0, 0, 0}, WeaponKind::Shield, ArmourKind::Unarmoured, false, nullptr, 0, 20, 13, 20, 0, 0, "warding", 3},
    {"scale_hauberk", "Scale Hauberk", EquipmentSlot::Armour, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Heavy, false, nullptr, 0, 16, 4, 10, 0, 0, nullptr, 0},
    {"plate_cuirass", "Plate Cuirass", EquipmentSlot::Armour, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Heavy, false, nullptr, 0, 26, 8, 16, 0, 0, nullptr, 0},
    {"gothic_plate", "Gothic Plate", EquipmentSlot::Armour, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Heavy, false, nullptr, 0, 38, 13, 24, 0, 0, nullptr, 0},
    {"studded_jerkin", "Studded Jerkin", EquipmentSlot::Armour, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Light, false, nullptr, 0, 7, 4, 0, 10, 0, nullptr, 0},
    {"brigandine", "Brigandine", EquipmentSlot::Armour, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Light, false, nullptr, 0, 10, 8, 0, 16, 0, nullptr, 0},
    {"nightweave_coat", "Nightweave Coat", EquipmentSlot::Armour, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Light, false, nullptr, 0, 14, 13, 0, 24, 0, nullptr, 0},
    {"acolyte_robes", "Acolyte Robes", EquipmentSlot::Armour, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Cloth, false, nullptr, 0, 12, 4, 0, 0, 10, nullptr, 0},
    {"sorcerers_robes", "Sorcerer's Robes", EquipmentSlot::Armour, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Cloth, false, nullptr, 0, 20, 8, 0, 0, 16, nullptr, 0},
    {"shroudweave_robes", "Shroudweave Robes", EquipmentSlot::Armour, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Cloth, false, nullptr, 0, 30, 13, 0, 0, 24, nullptr, 0},
    {"circlet", "Circlet", EquipmentSlot::Head, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Cloth, false, nullptr, 0, 8, 8, 0, 0, 14, nullptr, 0},
    {"hunters_cowl", "Hunter's Cowl", EquipmentSlot::Head, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Light, false, nullptr, 0, 3, 8, 0, 14, 0, nullptr, 0},
    {"great_helm", "Great Helm", EquipmentSlot::Head, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Heavy, false, nullptr, 0, 9, 8, 14, 0, 0, nullptr, 0},
    {"runed_wraps", "Runed Wraps", EquipmentSlot::Hands, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Cloth, false, nullptr, 0, 8, 8, 0, 0, 14, nullptr, 0},
    {"archers_bracers", "Archer's Bracers", EquipmentSlot::Hands, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Light, false, nullptr, 0, 3, 8, 0, 14, 0, nullptr, 0},
    {"plated_gauntlets", "Plated Gauntlets", EquipmentSlot::Hands, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Heavy, false, nullptr, 0, 9, 8, 14, 0, 0, nullptr, 0},
    {"mystic_slippers", "Mystic Slippers", EquipmentSlot::Feet, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Cloth, false, nullptr, 0, 8, 8, 0, 0, 14, nullptr, 0},
    {"stalker_boots", "Stalker Boots", EquipmentSlot::Feet, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Light, false, nullptr, 0, 3, 8, 0, 14, 0, nullptr, 0},
    {"sabatons", "Sabatons", EquipmentSlot::Feet, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Heavy, false, nullptr, 0, 9, 8, 14, 0, 0, nullptr, 0},
    {"gold_ring", "Gold Ring", EquipmentSlot::Ring1, {0, 1, 0, 0, 0}, WeaponKind::None, ArmourKind::Unarmoured, false, nullptr, 0, 0, 1, 0, 0, 0, nullptr, 0},
    {"bone_ring", "Bone Ring", EquipmentSlot::Ring1, {1, 1, 1, 0, 0}, WeaponKind::None, ArmourKind::Unarmoured, false, nullptr, 0, 0, 8, 0, 0, 0, nullptr, 0},
    {"grave_amulet", "Grave Amulet", EquipmentSlot::Charm, {0, 0, 0, 8, 8}, WeaponKind::None, ArmourKind::Unarmoured, false, nullptr, 0, 0, 8, 0, 0, 0, nullptr, 0},
    {"shadow_cloak", "Shadow Cloak", EquipmentSlot::Cloak, {0, 0, 0, 0, 0}, WeaponKind::None, ArmourKind::Unarmoured, false, nullptr, 0, 0, 8, 0, 0, 0, "nimble", 3},
    {"heavy_belt", "Heavy Belt", EquipmentSlot::Belt, {0, 0, 0, 8, 0}, WeaponKind::None, ArmourKind::Unarmoured, false, nullptr, 0, 0, 8, 0, 0, 0, nullptr, 0},
    {"unique_lichbane", "Lichbane", EquipmentSlot::Weapon, {18, 0, 6, 10, 0}, WeaponKind::TwoHanded, ArmourKind::Unarmoured, true,
     "Forged to end the first Lich. It remembers how.", 14, 0, 13, 20, 0, 0, nullptr, 0},
    {"unique_saintsbane", "Saintsbane", EquipmentSlot::Weapon, {12, 8, 0, 0, 0}, WeaponKind::OneHanded, ArmourKind::Unarmoured, true,
     "It fell from a saint's grave, edge first.", 10, 0, 13, 14, 8, 0, nullptr, 0},
    {"unique_crypt_whisper", "Whisper of the Crypt", EquipmentSlot::Weapon, {0, 16, 4, -10, 0}, WeaponKind::Bow, ArmourKind::Unarmoured, true,
     "Arrows loosed from it make no sound at all.", 10, 0, 13, 0, 20, 0, nullptr, 0},
    {"unique_pale_choir", "Staff of the Pale Choir", EquipmentSlot::Weapon, {0, 0, 18, -15, 25}, WeaponKind::Staff, ArmourKind::Unarmoured, true,
     "Its hollow keys still sing for the dead.", 10, 0, 13, 0, 0, 20, nullptr, 0},
    {"unique_last_captain", "Aegis of the Last Captain", EquipmentSlot::OffHand, {4, 0, 0, 30, 0}, WeaponKind::Shield, ArmourKind::Unarmoured, true,
     "The barracks fell. The shield did not.", 0, 22, 13, 16, 0, 0, nullptr, 0},
    {"unique_bloodbound_mail", "Bloodbound Mail", EquipmentSlot::Armour, {6, 0, 0, 40, -15}, WeaponKind::None, ArmourKind::Heavy, true,
     "The rings are warm, and they drink.", 0, 40, 13, 20, 0, 0, nullptr, 0},
    {"unique_nightstalker", "Nightstalker Leathers", EquipmentSlot::Armour, {0, 12, 0, 15, 0}, WeaponKind::None, ArmourKind::Light, true,
     "Cured in a place the sun has never reached.", 0, 15, 13, 0, 20, 0, nullptr, 0},
    {"unique_unseen_choir", "Robe of the Unseen Choir", EquipmentSlot::Armour, {0, 0, 12, 0, 30}, WeaponKind::None, ArmourKind::Cloth, true,
     "Faint hymns follow its hem through empty halls.", 0, 32, 13, 0, 0, 20, nullptr, 0},
    {"unique_gravewarden", "Gravewarden's Oath", EquipmentSlot::Head, {6, 0, 0, 20, -5}, WeaponKind::None, ArmourKind::Heavy, true,
     "Sworn to keep the dead below. Broken once.", 0, 10, 13, 14, 0, 0, nullptr, 0},
    {"unique_ossuary_grips", "Ossuary Grips", EquipmentSlot::Hands, {8, 0, 0, 12, 0}, WeaponKind::None, ArmourKind::Heavy, true,
     "Knuckles of the bone-wardens, still clenched.", 0, 10, 13, 14, 0, 0, nullptr, 0},
    {"unique_veilwalker", "Veilwalker Treads", EquipmentSlot::Feet, {0, 10, 0, 10, 0}, WeaponKind::None, ArmourKind::Light, true,
     "Each step lands a heartbeat early.", 0, 4, 13, 0, 14, 0, nullptr, 0},
    {"unique_nameless_shroud", "Shroud of the Nameless", EquipmentSlot::Cloak, {0, 8, 8, 8, 0}, WeaponKind::None, ArmourKind::Unarmoured, true,
     "Whoever wore it last is not remembered.", 0, 0, 13, 0, 0, 0, nullptr, 0},
    {"unique_ninth_seal", "Ring of the Ninth Seal", EquipmentSlot::Ring1, {5, 5, 5, 10, 10}, WeaponKind::None, ArmourKind::Unarmoured, true,
     "Eight seals broke. This one held.", 0, 0, 13, 0, 0, 0, nullptr, 0},
    {"unique_abyss_heart", "Heart of the Abyss", EquipmentSlot::Charm, {-4, -4, 0, 30, 30}, WeaponKind::None, ArmourKind::Unarmoured, true,
     "It beats only when you are still.", 0, 0, 13, 0, 0, 0, nullptr, 0},
}};

inline bool trainingItem(const ItemDefinition& d) { return std::string_view(d.id).find("training_")==0; }
inline std::vector<const ItemDefinition*> rewardItemDefinitions() {
    std::vector<const ItemDefinition*> result;
    for(const auto& d:kItemDefinitions) if(!trainingItem(d) && !d.unique && d.depth>0) result.push_back(&d);
    return result;
}
inline std::vector<const ItemDefinition*> uniqueItemDefinitions() {
    std::vector<const ItemDefinition*> result;
    for(const auto& d:kItemDefinitions) if(d.unique) result.push_back(&d);
    return result;
}
inline const ItemDefinition* findItemDefinition(std::string_view id) {
    for (const auto& definition : kItemDefinitions)
        if (id == definition.id) return &definition;
    return nullptr;
}

class Item : public Entity {
public:
    // Retained for generic Entity smoke examples; these have no equipment definition.
    using Entity::Entity;
    Item(const ItemDefinition& definition, std::uint64_t instanceId, Position position = {},
         std::vector<RolledAffix> affixes = {}, int rollTier = 0)
        : Entity(itemName(definition, affixes), '!', position),
          definition_(&definition), instanceId_(instanceId), affixes_(std::move(affixes)), rollTier_(rollTier) {}
    const ItemDefinition* definition() const { return definition_; }
    std::uint64_t instanceId() const { return instanceId_; }
    const std::vector<RolledAffix>& affixes() const { return affixes_; }
    int rollTier() const { return rollTier_; }
    ItemRarity rarity() const {
        return definition_ && definition_->unique ? ItemRarity::Unique : rarityForAffixes(affixes_.size());
    }
    // Magic and rare items are named by their affixes: "Vampiric Iron Sword of Embers".
    static std::string itemName(const ItemDefinition& definition, const std::vector<RolledAffix>& affixes) {
        if (affixes.empty()) return definition.name;
        const char* prefix = nullptr; const char* suffix = nullptr;
        for (const auto& rolled : affixes)
            if (const auto* a = findAffix(rolled.id); a && a->title) {
                if (a->prefix && !prefix) prefix = a->title;
                else if (!a->prefix && !suffix) suffix = a->title;
            }
        if (!prefix && !suffix) return std::string(rarityName(rarityForAffixes(affixes.size()))) + " " + definition.name;
        return (prefix ? std::string(prefix) + " " : std::string()) + definition.name + (suffix ? std::string(" ") + suffix : std::string());
    }
    // Rolled affixes, the base's implicit, and what any cursed affix costs.
    int affixValue(BonusStat stat) const {
        int total = 0;
        for (const auto& rolled : affixes_)
            if (const auto* a = findAffix(rolled.id)) {
                if (a->stat == stat) total += rolled.value;
                if (a->cursed && a->penalty == stat) total -= rolled.value * a->penaltyPercent / 100;
            }
        if (definition_ && definition_->implicit)
            if (const auto* a = findAffix(definition_->implicit); a && a->stat == stat) total += definition_->implicitValue;
        return total;
    }
    ItemBonuses bonuses() const {
        ItemBonuses result = definition_ ? definition_->bonuses : ItemBonuses{};
        for (const auto& rolled : affixes_)
            if (const auto* affix = findAffix(rolled.id)) {
                addBonus(result, affix->stat, rolled.value);
                if (affix->cursed) addBonus(result, affix->penalty, -rolled.value * affix->penaltyPercent / 100);
            }
        if (definition_ && definition_->implicit)
            if (const auto* a = findAffix(definition_->implicit)) addBonus(result, a->stat, definition_->implicitValue);
        return result;
    }
private:
    const ItemDefinition* definition_ = nullptr;
    std::uint64_t instanceId_ = 0;
    std::vector<RolledAffix> affixes_;
    int rollTier_ = 0;
};

} // namespace engine
