#pragma once

#include <memory>
#include <array>
#include <optional>
#include <algorithm>
#include <vector>

#include "entities/Item.hpp"

namespace engine {

// Owns bag items and eleven equipment slots. unique_ptr transfers make ownership
// exclusive; replacing equipment swaps it back into the selected bag row.
// Player::equip/unequip also refresh effective stats. No stacking/weight limits.
class Inventory {
public:
    static constexpr std::size_t capacity=50;
    bool full() const { return items_.size()>=capacity; }
    // Restoration and guaranteed rewards may retain overflow; normal acquisition
    // checks full() before transferring ownership. Never discard an existing item.
    void add(std::unique_ptr<Item> item) { if (item) items_.push_back(std::move(item)); }

    const std::vector<std::unique_ptr<Item>>& items() const { return items_; }
    // An effect affix summed over everything worn (rings, weapon, armour...).
    int affixTotal(BonusStat stat) const {
        int total = 0;
        for (const auto& item : equipment_) if (item) total += item->affixValue(stat);
        return total;
    }
    const Item* equipped(EquipmentSlot slot) const {
        const auto index = static_cast<std::size_t>(slot);
        return index < equipment_.size() ? equipment_[index].get() : nullptr;
    }
    ArmourKind armourKind() const {
        std::array<int,4> count{};
        for(auto slot:{EquipmentSlot::Head,EquipmentSlot::Armour,EquipmentSlot::Hands,EquipmentSlot::Feet}) {
            const auto* item=equipped(slot);
            const auto kind=item && item->definition()?item->definition()->armourKind:ArmourKind::Cloth;
            ++count[static_cast<int>(kind==ArmourKind::Unarmoured?ArmourKind::Cloth:kind)];
        }
        const auto* body=equipped(EquipmentSlot::Armour);
        auto tie=body?body->definition()->armourKind:ArmourKind::Cloth;
        if(tie==ArmourKind::Unarmoured) tie=ArmourKind::Cloth;
        const int most=*std::max_element(count.begin(),count.end());
        if(count[static_cast<int>(tie)]==most) return tie;
        for(auto kind:{ArmourKind::Cloth,ArmourKind::Light,ArmourKind::Heavy}) if(count[static_cast<int>(kind)]==most) return kind;
        return ArmourKind::Cloth;
    }
    EquipmentSlot preferredSlot(const ItemDefinition& d) const {
        if(ringSlot(d.slot)) return !equipped(EquipmentSlot::Ring1)?EquipmentSlot::Ring1:
            !equipped(EquipmentSlot::Ring2)?EquipmentSlot::Ring2:EquipmentSlot::Ring1;
        return d.slot;
    }
    std::unique_ptr<Item> take(std::size_t index) {
        if (index>=items_.size()) return {};
        auto item=std::move(items_[index]); items_.erase(items_.begin()+index); return item;
    }
    bool equip(std::size_t index, std::optional<EquipmentSlot> target={}) {
        if (index >= items_.size() || !items_[index]->definition()) return false;
        const auto chosen=target.value_or(preferredSlot(*items_[index]->definition()));
        if(!slotAccepts(chosen,items_[index]->definition()->slot)) return false;
        const auto slot = static_cast<std::size_t>(chosen);
        if (slot >= equipment_.size()) return false;
        if (const auto* weapon=equipped(EquipmentSlot::Weapon);
            slot==3 && weapon && (weapon->definition()->weaponKind==WeaponKind::TwoHanded || weapon->definition()->weaponKind==WeaponKind::Bow)) return false;
        const auto kind=items_[index]->definition()->weaponKind;
        if (slot==0 && (kind==WeaponKind::TwoHanded || kind==WeaponKind::Bow) && equipped(EquipmentSlot::OffHand)) return false;
        std::swap(items_[index], equipment_[slot]);
        if (!items_[index]) items_.erase(items_.begin() + index);
        return true;
    }
    bool unequip(EquipmentSlot slot) {
        const auto index = static_cast<std::size_t>(slot);
        if (full() || index >= equipment_.size() || !equipment_[index]) return false;
        items_.push_back(std::move(equipment_[index]));
        return true;
    }

private:
    std::vector<std::unique_ptr<Item>> items_;
    std::array<std::unique_ptr<Item>, kEquipmentSlotCount> equipment_;
};

} // namespace engine
