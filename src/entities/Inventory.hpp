#pragma once

#include <memory>
#include <array>
#include <vector>

#include "entities/Item.hpp"

namespace engine {

// Owns bag items and three equipment slots. unique_ptr transfers make ownership
// exclusive; replacing equipment swaps it back into the selected bag row.
// Player::equip/unequip also refresh effective stats. No stacking/weight limits.
class Inventory {
public:
    void add(std::unique_ptr<Item> item) { if (item) items_.push_back(std::move(item)); }

    const std::vector<std::unique_ptr<Item>>& items() const { return items_; }
    const Item* equipped(EquipmentSlot slot) const {
        const auto index = static_cast<std::size_t>(slot);
        return index < equipment_.size() ? equipment_[index].get() : nullptr;
    }
    bool equip(std::size_t index) {
        if (index >= items_.size() || !items_[index]->definition()) return false;
        const auto slot = static_cast<std::size_t>(items_[index]->definition()->slot);
        if (slot >= equipment_.size()) return false;
        std::swap(items_[index], equipment_[slot]);
        if (!items_[index]) items_.erase(items_.begin() + index);
        return true;
    }
    bool unequip(EquipmentSlot slot) {
        const auto index = static_cast<std::size_t>(slot);
        if (index >= equipment_.size() || !equipment_[index]) return false;
        items_.push_back(std::move(equipment_[index]));
        return true;
    }

private:
    std::vector<std::unique_ptr<Item>> items_;
    std::array<std::unique_ptr<Item>, 3> equipment_;
};

} // namespace engine
