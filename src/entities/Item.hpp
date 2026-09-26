#pragma once

#include "entities/Entity.hpp"
#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

namespace engine {

// Fixed equipment definitions plus uniquely identified, movable item instances.
enum class EquipmentSlot { Weapon, Armour, Charm };
inline const char* slotName(EquipmentSlot slot) {
    switch (slot) {
        case EquipmentSlot::Weapon: return "Weapon";
        case EquipmentSlot::Armour: return "Armour";
        case EquipmentSlot::Charm: return "Charm";
    }
    return "Unknown";
}

struct ItemBonuses {
    int strength = 0, dexterity = 0, intelligence = 0;
    int maxHp = 0, maxMana = 0;
};
enum class ItemRarity { Normal, Magic, Rare };
inline const char* rarityName(ItemRarity rarity) {
    return rarity == ItemRarity::Rare ? "Rare" : rarity == ItemRarity::Magic ? "Magic" : "Normal";
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
    {"agility", "Dexterity", BonusStat::Dexterity, 7, 1, 3, 1},
    {"knowledge", "Intelligence", BonusStat::Intelligence, 5, 2, 4, 1},
    {"vitality", "Max HP", BonusStat::Hp, 6, 3, 6, 2},
    {"reservoir", "Max mana", BonusStat::Mana, 6, 3, 6, 2},
    {"brawn", "Strength", BonusStat::Strength, 2, 1, 2, 1},
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
};

// IDs are persistent identities. Display names can change independently.
inline constexpr std::array<ItemDefinition, 9> kItemDefinitions{{
    {"iron_sword", "Iron Sword", EquipmentSlot::Weapon, {5, 0, 0, 0, 0}},
    {"ash_staff", "Ash Staff", EquipmentSlot::Weapon, {0, 0, 5, 0, 3}},
    {"hunting_bow", "Hunting Bow", EquipmentSlot::Weapon, {0, 5, 0, 0, 0}},
    {"chain_coat", "Chain Coat", EquipmentSlot::Armour, {0, 0, 0, 6, 0}},
    {"scout_leathers", "Scout Leathers", EquipmentSlot::Armour, {0, 3, 0, 3, 0}},
    {"woven_robes", "Woven Robes", EquipmentSlot::Armour, {0, 0, 2, 0, 6}},
    {"vitality_charm", "Vitality Charm", EquipmentSlot::Charm, {2, 0, 0, 4, 0}},
    {"focus_charm", "Focus Charm", EquipmentSlot::Charm, {0, 0, 2, 0, 5}},
    {"agility_charm", "Agility Charm", EquipmentSlot::Charm, {0, 3, 0, 2, 0}},
}};

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
        : Entity(std::string(rarityName(static_cast<ItemRarity>(affixes.size()))) + " " + definition.name, '!', position),
          definition_(&definition), instanceId_(instanceId), affixes_(std::move(affixes)), rollTier_(rollTier) {}
    const ItemDefinition* definition() const { return definition_; }
    std::uint64_t instanceId() const { return instanceId_; }
    const std::vector<RolledAffix>& affixes() const { return affixes_; }
    int rollTier() const { return rollTier_; }
    ItemRarity rarity() const { return static_cast<ItemRarity>(affixes_.size()); }
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
