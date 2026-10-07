#pragma once
#include <algorithm>
#include <memory>
#include "entities/Item.hpp"

namespace engine {
enum class LootTheme { General, Barracks, Sanctum, Crypts };
inline bool favoredByTheme(const ItemDefinition& item, LootTheme theme) {
    switch (theme) {
        case LootTheme::Barracks:
            return item.weaponKind==WeaponKind::OneHanded || item.weaponKind==WeaponKind::TwoHanded ||
                item.weaponKind==WeaponKind::Bow || item.weaponKind==WeaponKind::Shield || item.weaponKind==WeaponKind::Whip ||
                item.weaponKind==WeaponKind::Spear || item.weaponKind==WeaponKind::Dagger || item.weaponKind==WeaponKind::Mace || item.weaponKind==WeaponKind::Crossbow ||
                std::string_view(item.id)=="chain_coat" || std::string_view(item.id)=="scout_leathers";
        case LootTheme::Sanctum:
            return item.weaponKind==WeaponKind::Staff || std::string_view(item.id)=="woven_robes" ||
                std::string_view(item.id)=="focus_charm";
        case LootTheme::Crypts: return item.slot==EquipmentSlot::Charm || ringSlot(item.slot);
        default: return false;
    }
}
// An independent, explicitly serialized stream. Only committed rewards draw from it.
// xorshift64* has fixed unsigned arithmetic, independent of standard-library distributions.
class LootGenerator {
public:
    explicit LootGenerator(std::uint64_t seed = 1) : state_(seed ? seed : 1) {}
    std::uint64_t state() const { return state_; }
    void restore(std::uint64_t state) { state_ = state ? state : 1; }
    unsigned int roll(unsigned int bound) {
        state_ ^= state_ >> 12; state_ ^= state_ << 25; state_ ^= state_ >> 27;
        return static_cast<unsigned int>((state_ * UINT64_C(2685821657736338717)) % bound);
    }
    std::unique_ptr<Item> generate(int floor, int quality, std::uint64_t id, Position position,
                                   ItemRarity minimum = ItemRarity::Normal,
                                   LootTheme theme = LootTheme::General) {
        // Bases: only those this deep, the newest ones three times as likely,
        // the theme's favourites twice as likely. Training gear never drops.
        const auto bases=rewardItemDefinitions();
        const auto weight=[&](const ItemDefinition& d) -> unsigned {
            if (d.depth>std::max(1,floor)) return 0;
            return (d.depth+4>floor?3u:1u)*(favoredByTheme(d,theme)?2u:1u);
        };
        unsigned totalWeight=0;
        for(const auto* d:bases) totalWeight+=weight(*d);
        unsigned choice=roll(std::max(1u,totalWeight));
        std::size_t selected=0;
        for(;selected+1<bases.size();++selected) {
            const unsigned w=weight(*bases[selected]);
            if(choice<w) break;
            choice-=w;
        }
        const auto& definition=*bases[selected];
        const int tier = std::clamp((floor - 1) / 3 + quality, 0, 5);
        // Rarity: most finds are plain; a rare is an event.
        const unsigned int rarityRoll = roll(100);
        const unsigned rareChance = static_cast<unsigned>(3 + tier * 2 + quality * 3);
        const unsigned magicChance = static_cast<unsigned>(25 + tier * 3);
        ItemRarity rarity = rarityRoll < rareChance ? ItemRarity::Rare : rarityRoll < rareChance + magicChance ? ItemRarity::Magic : ItemRarity::Normal;
        if (static_cast<int>(minimum) > static_cast<int>(rarity)) rarity = minimum == ItemRarity::Unique ? ItemRarity::Rare : minimum;
        return roll(definition, tier, rarity, id, position);
    }
    // A chosen base at a chosen rarity, its affixes rolled as a drop's would be
    // at this depth (the sandbox's item spawner).
    std::unique_ptr<Item> make(const ItemDefinition& definition, int floor, ItemRarity rarity, std::uint64_t id, Position position) {
        if (definition.unique) return std::make_unique<Item>(definition, id, position);
        return roll(definition, std::clamp((floor - 1) / 3, 0, 5), rarity == ItemRarity::Unique ? ItemRarity::Rare : rarity, id, position);
    }
private:
    std::unique_ptr<Item> roll(const ItemDefinition& definition, int tier, ItemRarity rarity, std::uint64_t id, Position position) {
        int count = rarity == ItemRarity::Magic ? 1 + static_cast<int>(roll(2)) : 0;
        if (rarity == ItemRarity::Rare) {
            count = 3;
            if (roll(100) < static_cast<unsigned>(35 + tier * 8)) ++count;
            if (count == 4 && roll(100) < static_cast<unsigned>(15 + tier * 6)) ++count;
            if (count == 5 && roll(100) < static_cast<unsigned>(5 + tier * 4)) ++count;
        }
        const int cap = rarity == ItemRarity::Rare ? 3 : 1; // prefixes, and suffixes, each
        std::vector<const AffixDefinition*> pool;
        for (const auto& affix : kAffixes)
            if (!affix.cursed && (affix.slots & (1u << static_cast<unsigned>(definition.slot)))) pool.push_back(&affix);
        std::vector<RolledAffix> affixes;
        int prefixes = 0, suffixes = 0;
        const auto take = [&](const AffixDefinition& affix) {
            const int value = affix.minimum + tier * affix.perTier + static_cast<int>(roll(affix.maximum - affix.minimum + 1));
            affixes.push_back({affix.id, value});
            (affix.prefix ? prefixes : suffixes)++;
            pool.erase(std::remove_if(pool.begin(), pool.end(), [&](const auto* c) { return c->stat == affix.stat; }), pool.end());
        };
        // A rare may carry one cursed affix: more power, at a price.
        if (rarity == ItemRarity::Rare && roll(100) < 12) {
            std::vector<const AffixDefinition*> cursed;
            for (const auto& affix : kAffixes)
                if (affix.cursed && (affix.slots & (1u << static_cast<unsigned>(definition.slot)))) cursed.push_back(&affix);
            if (!cursed.empty()) take(*cursed[roll(static_cast<unsigned>(cursed.size()))]);
        }
        while (static_cast<int>(affixes.size()) < count) {
            std::vector<const AffixDefinition*> open;
            for (const auto* a : pool) if ((a->prefix ? prefixes : suffixes) < cap) open.push_back(a);
            if (open.empty()) break;
            take(*open[roll(static_cast<unsigned>(open.size()))]);
        }
        return std::make_unique<Item>(definition, id, position, std::move(affixes), tier);
    }
    std::uint64_t state_;
};
}
