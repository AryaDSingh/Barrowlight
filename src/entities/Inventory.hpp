#pragma once

#include <memory>
#include <vector>

#include "entities/Item.hpp"

namespace engine {

// A minimal container of Items. No stacking, no weight limits, no equip
// slots -- just enough for an Actor to have somewhere for picked-up items
// to go. Owns its Items via unique_ptr since an Item that leaves the
// ground and enters an Inventory shouldn't have two owners.
class Inventory {
public:
    void add(std::unique_ptr<Item> item) { items_.push_back(std::move(item)); }

    const std::vector<std::unique_ptr<Item>>& items() const { return items_; }

private:
    std::vector<std::unique_ptr<Item>> items_;
};

} // namespace engine
