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

enum class WeaponKind { None, OneHanded, TwoHanded, Bow, Staff, Shield };
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
inline const char* rarityName(ItemRarity rarity) {
    return rarity == ItemRarity::Unique ? "Unique" : rarity == ItemRarity::Rare ? "Rare" : rarity == ItemRarity::Magic ? "Magic" : "Normal";
}
enum class BonusStat { Strength, Dexterity, Intelligence, Hp, Mana };
struct AffixDefinition {
    const char* id;
    const char* name;
    BonusStat stat; // same-stat affixes are mutually exclusive
    unsigned int slots; // weapon=1, armour=2, charm=4
    int minimum, maximum, perTier;
};
inline constexpr std::array<AffixDefinition, 8> kAffixes{{
    {"might", "Strength", BonusStat::Strength, 5, 2, 4, 1},
    {"agility", "Dexterity", BonusStat::Dexterity, 7 | 2032, 1, 3, 1},
    {"knowledge", "Intelligence", BonusStat::Intelligence, 5, 2, 4, 1},
    {"vitality", "Max HP", BonusStat::Hp, 14 | 2032, 3, 6, 2},
    {"reservoir", "Max mana", BonusStat::Mana, 14 | 2032, 3, 6, 2},
    {"brawn", "Strength", BonusStat::Strength, 10, 1, 2, 1},
    {"insight", "Intelligence", BonusStat::Intelligence, 2, 1, 2, 1},
    {"vigor", "Max HP", BonusStat::Hp, 1, 2, 3, 2},
}};
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
};

inline std::string equipmentTypeName(const ItemDefinition& item) {
    return armourSlot(item.slot) ? std::string(slotName(item.slot))+" / "+armourName(item.armourKind) : slotName(item.slot);
}

// IDs are persistent identities. Display names can change independently.
inline constexpr std::array<ItemDefinition, 42> kItemDefinitions{{
    {"iron_sword", "Iron Sword", EquipmentSlot::Weapon, {5, 0, 0, 0, 0}, WeaponKind::OneHanded},
    {"ash_staff", "Ash Staff", EquipmentSlot::Weapon, {0, 0, 5, 0, 3}, WeaponKind::Staff},
    {"hunting_bow", "Hunting Bow", EquipmentSlot::Weapon, {0, 5, 0, 0, 0}, WeaponKind::Bow},
    {"chain_coat", "Chain Coat", EquipmentSlot::Armour, {0, 0, 0, 6, 0}, WeaponKind::None, ArmourKind::Heavy},
    {"scout_leathers", "Scout Leathers", EquipmentSlot::Armour, {0, 3, 0, 3, 0}, WeaponKind::None, ArmourKind::Light},
    {"woven_robes", "Woven Robes", EquipmentSlot::Armour, {0, 0, 2, 0, 6}, WeaponKind::None, ArmourKind::Cloth},
    {"vitality_charm", "Vitality Amulet", EquipmentSlot::Charm, {2, 0, 0, 4, 0}},
    {"focus_charm", "Focus Amulet", EquipmentSlot::Charm, {0, 0, 2, 0, 5}},
    {"agility_charm", "Agility Amulet", EquipmentSlot::Charm, {0, 3, 0, 2, 0}},
    {"greatsword", "Greatsword", EquipmentSlot::Weapon, {5,0,0,0,0}, WeaponKind::TwoHanded},
    {"wooden_shield", "Wooden Shield", EquipmentSlot::OffHand, {0,0,0,4,0}, WeaponKind::Shield},
    {"training_sword", "Training Sword", EquipmentSlot::Weapon, {}, WeaponKind::OneHanded},
    {"training_greatsword", "Training Greatsword", EquipmentSlot::Weapon, {}, WeaponKind::TwoHanded},
    {"training_bow", "Training Bow", EquipmentSlot::Weapon, {}, WeaponKind::Bow},
    {"training_shield", "Training Shield", EquipmentSlot::OffHand, {}, WeaponKind::Shield},
    {"cloth_hood", "Cloth Hood", EquipmentSlot::Head, {0,0,0,0,1}, WeaponKind::None, ArmourKind::Cloth},
    {"leather_cap", "Leather Cap", EquipmentSlot::Head, {0,0,0,1,0}, WeaponKind::None, ArmourKind::Light},
    {"iron_helm", "Iron Helm", EquipmentSlot::Head, {0,0,0,2,0}, WeaponKind::None, ArmourKind::Heavy},
    {"cloth_gloves", "Cloth Gloves", EquipmentSlot::Hands, {0,0,0,0,1}, WeaponKind::None, ArmourKind::Cloth},
    {"leather_gloves", "Leather Gloves", EquipmentSlot::Hands, {0,1,0,0,0}, WeaponKind::None, ArmourKind::Light},
    {"iron_gauntlets", "Iron Gauntlets", EquipmentSlot::Hands, {1,0,0,0,0}, WeaponKind::None, ArmourKind::Heavy},
    {"cloth_slippers", "Cloth Slippers", EquipmentSlot::Feet, {0,0,0,0,1}, WeaponKind::None, ArmourKind::Cloth},
    {"leather_boots", "Leather Boots", EquipmentSlot::Feet, {0,0,0,1,0}, WeaponKind::None, ArmourKind::Light},
    {"iron_boots", "Iron Boots", EquipmentSlot::Feet, {0,0,0,2,0}, WeaponKind::None, ArmourKind::Heavy},
    {"traveler_cloak", "Traveler Cloak", EquipmentSlot::Cloak, {0,0,0,2,0}},
    {"sturdy_belt", "Sturdy Belt", EquipmentSlot::Belt, {0,0,0,2,0}},
    {"copper_ring", "Copper Ring", EquipmentSlot::Ring1, {1,0,0,0,0}},
    {"silver_ring", "Silver Ring", EquipmentSlot::Ring1, {0,0,1,0,0}},
    // Uniques: far above a rare's budget, most with a price attached.
    {"unique_lichbane", "Lichbane", EquipmentSlot::Weapon, {18,0,6,10,0}, WeaponKind::TwoHanded, ArmourKind::Unarmoured, true,
     "Forged to end the first Lich. It remembers how."},
    {"unique_saintsbane", "Saintsbane", EquipmentSlot::Weapon, {12,8,0,0,0}, WeaponKind::OneHanded, ArmourKind::Unarmoured, true,
     "It fell from a saint's grave, edge first."},
    {"unique_crypt_whisper", "Whisper of the Crypt", EquipmentSlot::Weapon, {0,16,4,-10,0}, WeaponKind::Bow, ArmourKind::Unarmoured, true,
     "Arrows loosed from it make no sound at all."},
    {"unique_pale_choir", "Staff of the Pale Choir", EquipmentSlot::Weapon, {0,0,18,-15,25}, WeaponKind::Staff, ArmourKind::Unarmoured, true,
     "Its hollow keys still sing for the dead."},
    {"unique_last_captain", "Aegis of the Last Captain", EquipmentSlot::OffHand, {4,0,0,30,0}, WeaponKind::Shield, ArmourKind::Unarmoured, true,
     "The barracks fell. The shield did not."},
    {"unique_bloodbound_mail", "Bloodbound Mail", EquipmentSlot::Armour, {6,0,0,40,-15}, WeaponKind::None, ArmourKind::Heavy, true,
     "The rings are warm, and they drink."},
    {"unique_nightstalker", "Nightstalker Leathers", EquipmentSlot::Armour, {0,12,0,15,0}, WeaponKind::None, ArmourKind::Light, true,
     "Cured in a place the sun has never reached."},
    {"unique_unseen_choir", "Robe of the Unseen Choir", EquipmentSlot::Armour, {0,0,12,0,30}, WeaponKind::None, ArmourKind::Cloth, true,
     "Faint hymns follow its hem through empty halls."},
    {"unique_gravewarden", "Gravewarden's Oath", EquipmentSlot::Head, {6,0,0,20,-5}, WeaponKind::None, ArmourKind::Heavy, true,
     "Sworn to keep the dead below. Broken once."},
    {"unique_ossuary_grips", "Ossuary Grips", EquipmentSlot::Hands, {8,0,0,12,0}, WeaponKind::None, ArmourKind::Heavy, true,
     "Knuckles of the bone-wardens, still clenched."},
    {"unique_veilwalker", "Veilwalker Treads", EquipmentSlot::Feet, {0,10,0,10,0}, WeaponKind::None, ArmourKind::Light, true,
     "Each step lands a heartbeat early."},
    {"unique_nameless_shroud", "Shroud of the Nameless", EquipmentSlot::Cloak, {0,8,8,8,0}, WeaponKind::None, ArmourKind::Unarmoured, true,
     "Whoever wore it last is not remembered."},
    {"unique_ninth_seal", "Ring of the Ninth Seal", EquipmentSlot::Ring1, {5,5,5,10,10}, WeaponKind::None, ArmourKind::Unarmoured, true,
     "Eight seals broke. This one held."},
    {"unique_abyss_heart", "Heart of the Abyss", EquipmentSlot::Charm, {-4,-4,0,30,30}, WeaponKind::None, ArmourKind::Unarmoured, true,
     "It beats only when you are still."},
}};

inline bool trainingItem(const ItemDefinition& d) { return std::string_view(d.id).find("training_")==0; }
inline std::vector<const ItemDefinition*> rewardItemDefinitions() {
    std::vector<const ItemDefinition*> result;
    for(const auto& d:kItemDefinitions) if(!trainingItem(d) && !d.unique) result.push_back(&d);
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
        : Entity(affixes.empty() ? std::string(definition.name)
                                 : std::string(rarityName(static_cast<ItemRarity>(affixes.size()))) + " " + definition.name,
                 '!', position),
          definition_(&definition), instanceId_(instanceId), affixes_(std::move(affixes)), rollTier_(rollTier) {}
    const ItemDefinition* definition() const { return definition_; }
    std::uint64_t instanceId() const { return instanceId_; }
    const std::vector<RolledAffix>& affixes() const { return affixes_; }
    int rollTier() const { return rollTier_; }
    ItemRarity rarity() const {
        return definition_ && definition_->unique ? ItemRarity::Unique : static_cast<ItemRarity>(affixes_.size());
    }
    ItemBonuses bonuses() const {
        ItemBonuses result = definition_ ? definition_->bonuses : ItemBonuses{};
        for (const auto& rolled : affixes_)
            if (const auto* affix = findAffix(rolled.id)) addBonus(result, affix->stat, rolled.value);
        return result;
    }
private:
    const ItemDefinition* definition_ = nullptr;
    std::uint64_t instanceId_ = 0;
    std::vector<RolledAffix> affixes_;
    int rollTier_ = 0;
};

} // namespace engine
