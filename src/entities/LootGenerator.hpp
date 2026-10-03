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
        // Gentle bias: favored bases have twice the weight, all normal bases
        // remain possible. Training equipment stays outside the reward pool.
        const auto bases=rewardItemDefinitions();
        unsigned totalWeight=0;
        for(const auto* d:bases) totalWeight+=favoredByTheme(*d,theme)?2:1;
        unsigned choice=roll(totalWeight);
        std::size_t selected=0;
        for(;selected+1<bases.size();++selected) {
            const unsigned weight=favoredByTheme(*bases[selected],theme)?2:1;
            if(choice<weight) break;
            choice-=weight;
        }
        const auto& definition=*bases[selected];
        const int tier = std::clamp((floor - 1) / 3 + quality, 0, 5);
        const unsigned int rarityRoll = roll(100);
        int count = rarityRoll < static_cast<unsigned>(15 + tier * 5) ? 2 : rarityRoll < 75 ? 1 : 0;
        count = std::max(count, static_cast<int>(minimum));
        std::vector<RolledAffix> affixes;
        std::vector<const AffixDefinition*> pool;
        for (const auto& affix : kAffixes)
            if (affix.slots & (1u << static_cast<unsigned>(definition.slot))) pool.push_back(&affix);
        for (int i = 0; i < count; ++i) {
            const auto* affix = pool[roll(static_cast<unsigned>(pool.size()))];
            const int value = affix->minimum + tier * affix->perTier +
                              static_cast<int>(roll(affix->maximum - affix->minimum + 1));
            affixes.push_back({affix->id, value});
            pool.erase(std::remove_if(pool.begin(), pool.end(), [&](const auto* candidate) {
                return candidate->stat == affix->stat;
            }), pool.end());
        }
        return std::make_unique<Item>(definition, id, position, std::move(affixes), tier);
    }
private:
    std::uint64_t state_;
};
}
