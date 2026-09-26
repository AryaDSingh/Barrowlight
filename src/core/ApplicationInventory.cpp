#include "core/Application.hpp"
#include "core/PlayLayout.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <queue>

namespace engine {
namespace {
constexpr std::size_t kBagPageSize = 12;
const sf::Color kText(220, 228, 240), kAccent(115, 225, 215);
std::string signedNumber(int value) {
    return (value > 0 ? "+" : "") + std::to_string(value);
}
}

void Application::openInventory() {
    cancelTargeting();
    mousePixel_.reset();
    inventoryOpen_ = true;
    runesOpen_ = false;
    inventorySelection_ = std::min(inventorySelection_, player_.inventory().items().size() + 2);
}

void Application::finishInventoryTurn() {
    inventoryOpen_ = false; // reveal the enemies' response to the committed action
    runesOpen_ = false;
    cancelTargeting();
    player_.talents().tickCooldowns();
    currentActor_ = &scheduler_.nextTurn();
    processMonsterTurns();
    if (mode_ != GameMode::GameOver) advanceTurnsUntilPlayerCanAct();
    updateFieldOfView();
}

void Application::pickupItem() {
    const auto position = player_.position();
    if (chestExists_ && !chestClaimed_ && position.x == chestPosition_.x && position.y == chestPosition_.y &&
        nextItemId_ < std::numeric_limits<std::uint64_t>::max()) {
        chestClaimed_ = true;
        auto reward = loot_.generate(currentFloor_, 0, nextItemId_++, position, ItemRarity::Magic);
        log("Chest reward: ", reward->name(), " (in your bag).");
        player_.inventory().add(std::move(reward));
        if (currentFloor_ == 1) {
            runeChoiceAvailable_ = true;
            log("Your first rune choice is ready. V, then 1-4 to choose.");
        } else if (currentFloor_ % 2 == 0) giveRune(kRunes[loot_.roll(4)].id);
        finishInventoryTurn();
        return;
    }
    const auto found = std::find_if(groundItems_.begin(), groundItems_.end(), [&](const auto& item) {
        return item->position().x == position.x && item->position().y == position.y;
    });
    if (found == groundItems_.end()) {
        log("Nothing here to pick up.");
        return;
    }
    const auto name = (*found)->name();
    player_.inventory().add(std::move(*found));
    groundItems_.erase(found);
    log("Picked up ", name, ". B: inventory.");
    finishInventoryTurn();
}

void Application::handleInventoryKey(sf::Keyboard::Key key) {
    const std::size_t rows = player_.inventory().items().size() + 3;
    if (key == sf::Keyboard::Key::B) inventoryOpen_ = false;
    else if (key == sf::Keyboard::Key::F5) saveGame();
    else if (key == sf::Keyboard::Key::Up || key == sf::Keyboard::Key::W)
        inventorySelection_ = (inventorySelection_ + rows - 1) % rows;
    else if (key == sf::Keyboard::Key::Down || key == sf::Keyboard::Key::S)
        inventorySelection_ = (inventorySelection_ + 1) % rows;
    else if (key == sf::Keyboard::Key::PageDown)
        inventorySelection_ = std::min(inventorySelection_ + kBagPageSize, rows - 1);
    else if (key == sf::Keyboard::Key::PageUp)
        inventorySelection_ = inventorySelection_ > kBagPageSize ? inventorySelection_ - kBagPageSize : 0;
    else if (key == sf::Keyboard::Key::Enter || key == sf::Keyboard::Key::E || key == sf::Keyboard::Key::U) {
        if (inventorySelection_ < 3) {
            if (key == sf::Keyboard::Key::E) return;
            const auto slot = static_cast<EquipmentSlot>(inventorySelection_);
            const auto* item = player_.inventory().equipped(slot);
            if (!item) return;
            const auto name = item->name();
            if (player_.unequip(slot)) {
                log("Removed ", name, ".");
                finishInventoryTurn();
            }
        } else {
            if (key == sf::Keyboard::Key::U) return;
            const auto index = inventorySelection_ - 3;
            if (index >= player_.inventory().items().size()) return;
            const auto name = player_.inventory().items()[index]->name();
            if (player_.equip(index)) {
                log("Equipped ", name, ".");
                finishInventoryTurn();
            }
        }
    }
}

void Application::spawnFixedItems() {
    // Three fixed rewards per ordinary floor, near its entrance. BFS guarantees
    // reachable placement and consumes no combat/generation random numbers.
    if (currentFloor_ != 1) return; // starter set only; later floors use paced randomized rewards
    const int startingVariant = playerClass_ == PlayerClass::Mage ? 1 :
                                playerClass_ == PlayerClass::Thief ? 2 : 0;
    const int variant = (startingVariant + currentFloor_ - 1) % 3;
    const std::array<int, 3> armourDefinitions{3, 5, 4};
    const std::array<int, 3> definitions{variant, armourDefinitions[variant], 6 + variant};
    std::queue<Position> frontier;
    std::vector<bool> seen(static_cast<std::size_t>(map_.width()) * map_.height(), false);
    const auto enqueue = [&](Position p) {
        if (!map_.isWalkable(p.x, p.y)) return;
        const auto index = static_cast<std::size_t>(p.y) * map_.width() + p.x;
        if (seen[index]) return;
        seen[index] = true; frontier.push(p);
    };
    enqueue(player_.position());
    std::size_t placed = 0;
    while (!frontier.empty() && placed < definitions.size()) {
        const auto p = frontier.front(); frontier.pop();
        if (map_.tileAt(p.x, p.y).type == TileType::Floor && !isOccupied(p, &player_) &&
            nextItemId_ < std::numeric_limits<std::uint64_t>::max()) {
            groundItems_.push_back(std::make_unique<Item>(kItemDefinitions[definitions[placed]], nextItemId_++, p));
            ++placed;
        }
        enqueue({p.x + 1, p.y}); enqueue({p.x - 1, p.y});
        enqueue({p.x, p.y + 1}); enqueue({p.x, p.y - 1});
    }
    if (placed) log("Supplies near the entrance. G: pick up at your feet. B: inventory.");
}

void Application::renderGroundItems() {
    if (chestExists_ && !chestClaimed_ && exploredMap_.at(chestPosition_.x, chestPosition_.y) == Visibility::Visible) {
        const auto screen = worldToScreen(chestPosition_.x, chestPosition_.y);
        if (screen.x >= 0 && screen.x < playLayout::mapWidth && screen.y >= playLayout::mapTop &&
            screen.y < playLayout::mapTop + playLayout::mapHeight) {
            sf::RectangleShape chest({18.f, 14.f}); chest.setPosition({screen.x+5.f, screen.y+7.f});
            chest.setFillColor(sf::Color(210,145,55)); chest.setOutlineThickness(2.f);
            chest.setOutlineColor(sf::Color(255,220,100)); window_.draw(chest);
        }
    }
    for (const auto& item : groundItems_) {
        const auto p = item->position();
        if (exploredMap_.at(p.x, p.y) != Visibility::Visible) continue;
        const auto screen = worldToScreen(p.x, p.y);
        if (screen.x < 0 || screen.x >= playLayout::mapWidth || screen.y < playLayout::mapTop ||
            screen.y >= playLayout::mapTop + playLayout::mapHeight) continue;
        sf::CircleShape marker(6.f, 4);
        marker.setPosition({screen.x + 7.f, screen.y + 7.f});
        marker.setFillColor(item->rarity() == ItemRarity::Rare ? sf::Color(255,211,95) :
                            item->rarity() == ItemRarity::Magic ? sf::Color(110,165,255) : sf::Color(210,210,215));
        window_.draw(marker);
    }
    for (const auto& item : groundItems_) {
        if (item->position().x == player_.position().x && item->position().y == player_.position().y) {
            drawText(item->name() + " at your feet [G: pickup]", 430.f, 49.f, 12, kAccent);
            break;
        }
    }
    if (chestExists_ && !chestClaimed_ && chestPosition_.x == player_.position().x && chestPosition_.y == player_.position().y)
        drawText("Reward chest [G: open, 1 turn]", 430.f, 49.f, 12, kAccent);
}

void Application::renderInventory() {
    sf::RectangleShape backdrop({1280.f, 720.f});
    backdrop.setFillColor(sf::Color(8, 12, 20, 245)); window_.draw(backdrop);
    drawText("INVENTORY & EQUIPMENT", 40.f, 22.f, 24, kAccent);
    drawText("Up/Down: select   PgUp/PgDn: scroll   Enter: equip/remove   B/Esc: close   F5/F9: save/load",
             40.f, 58.f, 15, kText);
    drawText("Viewing is free. Pickup, equip and remove cost 1 turn; the inventory closes when enemies act.",
             40.f, 82.f, 14, sf::Color(255, 211, 130));

    const auto& bag = player_.inventory().items();
    inventorySelection_ = std::min(inventorySelection_, bag.size() + 2);
    const auto row = [&](std::size_t index, const std::string& label, float y) {
        drawText((index == inventorySelection_ ? "> " : "  ") + label, 40.f, y, 17,
                 index == inventorySelection_ ? sf::Color(255, 220, 100) : kText);
    };
    drawText("EQUIPPED", 40.f, 126.f, 18, kAccent);
    for (int i = 0; i < 3; ++i) {
        const auto slot = static_cast<EquipmentSlot>(i);
        const auto* item = player_.inventory().equipped(slot);
        row(i, std::string(slotName(slot)) + ": " + (item ? item->name() : "(empty)"), 160.f + i * 28.f);
    }
    const auto first = inventorySelection_ >= 3 ? ((inventorySelection_ - 3) / kBagPageSize) * kBagPageSize : 0;
    drawText("BAG (" + std::to_string(bag.size()) + ")", 40.f, 266.f, 18, kAccent);
    if (bag.empty()) drawText("Stand on a gold diamond and press G.", 40.f, 302.f, 15, kText);
    for (std::size_t i = first; i < bag.size() && i < first + kBagPageSize; ++i)
        row(i + 3, bag[i]->name(), 302.f + (i - first) * 28.f);
    if (!bag.empty()) drawText("Showing " + std::to_string(first + 1) + "-" +
        std::to_string(std::min(first + kBagPageSize, bag.size())) + " of " + std::to_string(bag.size()),
        40.f, 650.f, 14, kText);

    const bool removing = inventorySelection_ < 3;
    const Item* selected = removing ? player_.inventory().equipped(static_cast<EquipmentSlot>(inventorySelection_)) :
                                    bag[inventorySelection_ - 3].get();
    float y = 126.f;
    if (selected && selected->definition()) {
        const auto& definition = *selected->definition();
        drawWrapped(selected->name(), 570.f, y, 70, kAccent, 240.f);
        const auto* current = player_.inventory().equipped(definition.slot);
        drawWrapped(std::string(slotName(definition.slot)) + (removing ? " | Enter/U: remove" : " | Enter/E: equip"),
                    570.f, y, 70, kText, 240.f);
        drawWrapped(removing ? "Preview: removing this item" :
                    "Replaces: " + (current ? current->name() : std::string("empty slot")),
                    570.f, y, 70, kText, 240.f);
        std::string modifiers = "Affixes: ";
        for (const auto& rolled : selected->affixes())
            modifiers += "+" + std::to_string(rolled.value) + " " + findAffix(rolled.id)->name + "  ";
        if (selected->affixes().empty()) modifiers += "none";
        drawText(modifiers, 570.f, 197.f, 13, kAccent);
        ItemBonuses oldBonus = current ? current->bonuses() : ItemBonuses{};
        ItemBonuses newBonus = removing ? ItemBonuses{} : selected->bonuses();
        const auto& base = player_.baseStats();
        const auto& effective = player_.stats();
        drawText("Stat          Base   Now   After  Change", 570.f, 230.f, 16, kAccent);
        const auto stat = [&](const char* name, int baseValue, int now, int delta, float rowY) {
            drawText(name, 570.f, rowY, 16, kText);
            drawText(std::to_string(baseValue), 715.f, rowY, 16, kText);
            drawText(std::to_string(now), 790.f, rowY, 16, kText);
            drawText(std::to_string(now + delta), 865.f, rowY, 16, kText);
            drawText(signedNumber(delta), 950.f, rowY, 16,
                     delta > 0 ? sf::Color(135, 240, 160) : delta < 0 ? sf::Color(255, 140, 120) : kText);
        };
        stat("Strength", base.strength, effective.strength, newBonus.strength - oldBonus.strength, 267.f);
        stat("Dexterity", base.dexterity, effective.dexterity, newBonus.dexterity - oldBonus.dexterity, 298.f);
        stat("Intelligence", base.intelligence, effective.intelligence, newBonus.intelligence - oldBonus.intelligence, 329.f);
        const int hpDelta = newBonus.maxHp - oldBonus.maxHp, manaDelta = newBonus.maxMana - oldBonus.maxMana;
        stat("Max HP", base.maxHp, effective.maxHp, hpDelta, 360.f);
        stat("Max mana", base.maxMana, effective.maxMana, manaDelta, 391.f);
        y = 445.f;
        drawWrapped("Before turn effects: HP " + std::to_string(std::min(effective.hp, effective.maxHp + hpDelta)) +
            "/" + std::to_string(effective.maxHp + hpDelta) + " | Mana " +
            std::to_string(std::min(effective.mana, effective.maxMana + manaDelta)) + "/" +
            std::to_string(effective.maxMana + manaDelta), 570.f, y, 70, kAccent, 610.f);
        drawWrapped("Gear changes never refill HP/mana. Lower maxima clamp current pools. Normal mana regeneration and enemy turns still occur.",
                    570.f, y, 70, kText, 610.f);
        drawWrapped("Attributes improve existing talent scaling, dodge and critical chance. Gear attributes do not also grant HP/mana; pool bonuses are explicit. Armour grants its listed stats only.",
                    570.f, y, 70, kText, 650.f);
    } else {
        drawWrapped("Empty equipment slot. Select an item in your bag to compare it against equipped gear.",
                    570.f, y, 65, kText, 400.f);
    }
}
} // namespace engine
