#include "core/Application.hpp"
#include "core/PlayLayout.hpp"
#include "world/FloorTheme.hpp"

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
    inventoryDragSource_.reset();
    inventoryEquipTarget_.reset();
    inventoryBagPage_=0;
    inventorySelection_ = std::min(inventorySelection_, player_.inventory().items().size() + kEquipmentSlotCount - 1);
}

void Application::finishInventoryTurn() {
    inventoryDragSource_.reset();
    inventoryEquipTarget_.reset();
    enforceMinionCap();
    inventoryOpen_ = false; // reveal the enemies' response to the committed action
    cancelTargeting();
    if (mode_==GameMode::Town) return;
    advanceEnemyIntents();
    player_.talents().tickCooldowns();
    currentActor_ = &scheduler_.nextTurn();
    processMonsterTurns();
    if (mode_ != GameMode::GameOver) advanceTurnsUntilPlayerCanAct();
    updateFieldOfView();
}

void Application::pickupItem() {
    if (interactVault()) return;
    const auto position = player_.position();
    if (chestExists_ && !chestClaimed_ && position.x == chestPosition_.x && position.y == chestPosition_.y &&
        nextItemId_ < std::numeric_limits<std::uint64_t>::max()) {
        if (player_.inventory().full()) { log("Bag full (50). Drop or sell an item first."); return; }
        chestClaimed_ = true;
        const auto region=floorTheme(currentFloor_).region;
        const auto theme=region==FloorRegion::Barracks ? LootTheme::Barracks :
            region==FloorRegion::Sanctum ? LootTheme::Sanctum : LootTheme::Crypts;
        auto reward = loot_.generate(currentFloor_, 0, nextItemId_++, position, ItemRarity::Magic,theme);
        log(floorTheme(currentFloor_).name, " chest: ", reward->name(), " (in your bag).");
        player_.inventory().add(std::move(reward));
        finishInventoryTurn();
        return;
    }
    const auto found = std::find_if(groundItems_.begin(), groundItems_.end(), [&](const auto& item) {
        return item->position().x == position.x && item->position().y == position.y;
    });
    if (found == groundItems_.end()) {
        if (interactStairs()) return;
        log("Nothing here to pick up.");
        return;
    }
    if (player_.inventory().full()) { log("Bag full (50). Drop or sell an item first."); return; }
    const auto name = (*found)->name();
    player_.inventory().add(std::move(*found));
    groundItems_.erase(found);
    log("Picked up ", name, ". B: inventory.");
    finishInventoryTurn();
}

void Application::handleInventoryKey(sf::Keyboard::Key key) {
    inventoryDragSource_.reset();
    const std::size_t rows = player_.inventory().items().size() + kEquipmentSlotCount;
    if (key == sf::Keyboard::Key::D && inventorySelection_ >= kEquipmentSlotCount) {
        if (mode_==GameMode::Town) { log("Use the town shop to sell unwanted gear."); return; }
        auto item=player_.inventory().take(inventorySelection_-kEquipmentSlotCount);
        if (!item) return;
        item->setPosition(player_.position());
        log("Dropped ",item->name(),"."); groundItems_.push_back(std::move(item));
        finishInventoryTurn(); return;
    }
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
        if (inventorySelection_ < kEquipmentSlotCount) {
            if (key == sf::Keyboard::Key::E) return;
            const auto slot = static_cast<EquipmentSlot>(inventorySelection_);
            const auto* item = player_.inventory().equipped(slot);
            if (!item) return;
            const auto name = item->name();
            if (player_.unequip(slot)) {
                log("Removed ", name, ".");
                finishInventoryTurn();
            } else log("Bag full (50). Make space before removing equipment.");
        } else {
            if (key == sf::Keyboard::Key::U) return;
            const auto index = inventorySelection_ - kEquipmentSlotCount;
            if (index >= player_.inventory().items().size()) return;
            const auto name = player_.inventory().items()[index]->name();
            if (player_.equip(index, inventoryEquipTarget_)) {
                log("Equipped ", name, ".");
                finishInventoryTurn();
            } else log("Remove your shield or two-handed/bow weapon before equipping this item.");
        }
    }
    inventoryEquipTarget_.reset();
    if(inventorySelection_>=kEquipmentSlotCount) inventoryBagPage_=(inventorySelection_-kEquipmentSlotCount)/kBagPageSize;
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
        drawText(currentFloor_<=3 ? "Chest: martial [G, 1 turn]" :
            currentFloor_<=6 ? "Chest: casting [G, 1 turn]" :
            "Chest: jewellery [G, 1 turn]", 630.f, 49.f, 11, kAccent);
}


namespace {
// Positions follow the body, while indices preserve save-file slot IDs.
sf::FloatRect gearRect(int slot) {
    static const std::array<sf::Vector2f,kEquipmentSlotCount> positions{{
        {25,270},{175,270},{175,195},{325,270},{175,120},{325,195},
        {25,355},{175,355},{175,440},{25,440},{325,440}}};
    return {positions[slot],{140,68}};
}
bool inBag(sf::Vector2i p) { return p.x>=485 && p.x<830 && p.y>=155 && p.y<491; }
std::optional<std::size_t> inventoryHit(sf::Vector2i p,std::size_t page,std::size_t count) {
    for(int i=0;i<kEquipmentSlotCount;++i)
        if(gearRect(i).contains(sf::Vector2f(p))) return i;
    if(inBag(p)) {
        const auto index=page*kBagPageSize+(p.y-155)/28;
        if(index<count) return index+kEquipmentSlotCount;
    }
    return {};
}
std::string clipped(std::string text,std::size_t size) {
    if(text.size()>size) text=text.substr(0,size-3)+"...";
    return text;
}
}

void Application::handleInventoryMouse(const sf::Event& event) {
    const auto count=player_.inventory().items().size();
    const auto pages=std::max<std::size_t>(1,(count+kBagPageSize-1)/kBagPageSize);
    inventoryBagPage_=std::min(inventoryBagPage_,pages-1);
    if(event.is<sf::Event::FocusLost>()) inventoryDragSource_.reset();
    if(const auto* move=event.getIf<sf::Event::MouseMoved>()) {
        mousePixel_=move->position;
        const auto hit=inventoryHit(move->position,inventoryBagPage_,count);
        if(!inventoryDragSource_ && hit) inventorySelection_=*hit;
        inventoryEquipTarget_.reset();
        if(inventoryDragSource_ && *inventoryDragSource_>=kEquipmentSlotCount && hit && *hit<kEquipmentSlotCount)
            inventoryEquipTarget_=static_cast<EquipmentSlot>(*hit);
    }
    if(const auto* wheel=event.getIf<sf::Event::MouseWheelScrolled>()) {
        if(inventoryDragSource_) return;
        if(wheel->delta<0) inventoryBagPage_=std::min(inventoryBagPage_+1,pages-1);
        else if(wheel->delta>0 && inventoryBagPage_) --inventoryBagPage_;
    }
    if(const auto* click=event.getIf<sf::Event::MouseButtonPressed>()) {
        const auto p=click->position;
        if(click->button==sf::Mouse::Button::Left) {
            if(p.x>=1160 && p.y<65) { inventoryOpen_=false; inventoryDragSource_.reset(); return; }
            if(p.y>=510 && p.y<550 && p.x>=485 && p.x<830) {
                if(p.x<650 && inventoryBagPage_) --inventoryBagPage_;
                else if(p.x>=650) inventoryBagPage_=std::min(inventoryBagPage_+1,pages-1);
                return;
            }
        }
        const auto hit=inventoryHit(p,inventoryBagPage_,count);
        if(!hit) return;
        inventorySelection_=*hit; inventoryEquipTarget_.reset();
        if(click->button==sf::Mouse::Button::Right) handleInventoryKey(sf::Keyboard::Key::Enter);
        else if(click->button==sf::Mouse::Button::Left) inventoryDragSource_=hit;
    }
    if(const auto* release=event.getIf<sf::Event::MouseButtonReleased>(); release && release->button==sf::Mouse::Button::Left) {
        if(!inventoryDragSource_) return;
        const auto source=*inventoryDragSource_; inventoryDragSource_.reset();
        const auto target=inventoryHit(release->position,inventoryBagPage_,count);
        inventoryEquipTarget_.reset();
        inventorySelection_=source;
        if(source>=kEquipmentSlotCount && target && *target<kEquipmentSlotCount) {
            const auto slot=static_cast<EquipmentSlot>(*target);
            const auto index=source-kEquipmentSlotCount;
            if(index>=count || !slotAccepts(slot,player_.inventory().items()[index]->definition()->slot)) {
                log("That item does not fit this slot."); return;
            }
            inventoryEquipTarget_=slot;
            handleInventoryKey(sf::Keyboard::Key::E);
        } else if(source<kEquipmentSlotCount && inBag(release->position)) handleInventoryKey(sf::Keyboard::Key::U);
    }
}

void Application::renderInventory() {
    sf::RectangleShape backdrop({1280.f,720.f});
    backdrop.setFillColor(sf::Color(8,12,20,250)); window_.draw(backdrop);
    drawText("INVENTORY & EQUIPMENT",25,20,24,kAccent);
    drawText("[ Close ]",1160,25,17,kAccent);
    drawText("Drag to equip/remove | Right-click: equip/remove | Hover: compare | Wheel/arrows: bag page",25,60,15,kText);
    drawText(mode_==GameMode::Town ? "Equipment changes are free in town." : "Equip, remove and drop cost one turn. Viewing is free.",25,83,14,sf::Color(255,211,130));
    const auto& inventory=player_.inventory(); const auto& bag=inventory.items();
    inventorySelection_=std::min(inventorySelection_,bag.size()+kEquipmentSlotCount-1);
    const auto pages=std::max<std::size_t>(1,(bag.size()+kBagPageSize-1)/kBagPageSize);
    inventoryBagPage_=std::min(inventoryBagPage_,pages-1);
    const Item* dragged=inventoryDragSource_ && *inventoryDragSource_>=kEquipmentSlotCount ? bag[*inventoryDragSource_-kEquipmentSlotCount].get():nullptr;
    for(int i=0;i<kEquipmentSlotCount;++i) {
        const auto rect=gearRect(i); const auto slot=static_cast<EquipmentSlot>(i);
        const auto* item=inventory.equipped(slot);
        sf::RectangleShape card(rect.size); card.setPosition(rect.position);
        card.setFillColor(sf::Color(24,34,49)); card.setOutlineThickness(1);
        card.setOutlineColor(dragged && slotAccepts(slot,dragged->definition()->slot) ? sf::Color(120,240,150):
            inventorySelection_==i?kAccent:sf::Color(65,80,100)); window_.draw(card);
        drawText(slotName(slot),rect.position.x+6,rect.position.y+5,14,kAccent);
        float y=rect.position.y+26;
        drawWrapped(item?clipped(item->name(),34):"(empty)",rect.position.x+6,y,16,kText,rect.position.y+65);
    }
    float y=550;
    drawWrapped(std::string("Armour: ")+armourName(inventory.armourKind())+". Majority of head, body, hands and feet. Empty pieces count as cloth; ties favour body.",25,y,54,kText,630);
    drawText("B/Esc: close | Up/Down: select | Enter: equip/remove",25,652,14,kText);
    drawText("D: drop bag item (dungeon) | F5/F9: save/load",25,677,14,kText);
    drawText("BAG "+std::to_string(bag.size())+" / 50",485,120,20,kAccent);
    const auto first=inventoryBagPage_*kBagPageSize;
    for(std::size_t r=0;r<kBagPageSize;++r) {
        const auto i=first+r;
        sf::RectangleShape row({340,26}); row.setPosition({485,155+28.f*r});
        row.setFillColor(inventorySelection_==i+kEquipmentSlotCount?sf::Color(45,65,80):sf::Color(20,29,42)); window_.draw(row);
        if(i<bag.size()) drawText(clipped(bag[i]->name(),37),491,158+28.f*r,14,
            bag[i]->rarity()==ItemRarity::Rare?sf::Color(255,215,110):bag[i]->rarity()==ItemRarity::Magic?sf::Color(135,180,255):kText);
    }
    drawText("< Previous       Next >",490,515,17,kAccent);
    drawText("Page "+std::to_string(inventoryBagPage_+1)+" / "+std::to_string(pages),490,545,15,kText);
    if(!logMessages_.empty()) { y=575; drawWrapped(logMessages_.back(),485,y,43,sf::Color(255,211,130),626); }
    if(bag.size()>Inventory::capacity) { y=632; drawWrapped("Overflow preserved. Drop or sell items before picking up more.",485,y,40,sf::Color(255,211,130),710); }
    const bool removing=inventorySelection_<kEquipmentSlotCount;
    const auto* selected=removing?inventory.equipped(static_cast<EquipmentSlot>(inventorySelection_)):bag[inventorySelection_-kEquipmentSlotCount].get();
    y=120;
    if(!selected) { drawWrapped("Select or hover an item to see its stats and comparison.",860,y,44,kText,260); return; }
    const auto& definition=*selected->definition();
    const auto slot=removing?static_cast<EquipmentSlot>(inventorySelection_):inventoryEquipTarget_.value_or(inventory.preferredSlot(definition));
    const auto* current=inventory.equipped(slot);
    drawWrapped(selected->name(),860,y,43,kAccent,180);
    drawWrapped(equipmentTypeName(definition),860,y,43,kText,230);
    drawWrapped(removing?"Preview: remove item":std::string("Equip to ")+slotName(slot)+"; replaces "+(current?current->name():"empty slot"),860,y,43,kText,300);
    const auto before=current?current->bonuses():ItemBonuses{};
    const auto after=removing?ItemBonuses{}:selected->bonuses();
    const auto bonus=selected->bonuses();
    y=310;
    drawText("Stat           Item   Change",860,y,16,kAccent); y+=30;
    const auto stat=[&](const char* name,int value,int delta) {
        drawText(name,860,y,16,kText); drawText(signedNumber(value),1020,y,16,kText);
        drawText(signedNumber(delta),1130,y,16,delta>0?sf::Color(135,240,160):delta<0?sf::Color(255,140,120):kText); y+=28;
    };
    stat("Strength",bonus.strength,after.strength-before.strength);
    stat("Dexterity",bonus.dexterity,after.dexterity-before.dexterity);
    stat("Intelligence",bonus.intelligence,after.intelligence-before.intelligence);
    stat("Max HP",bonus.maxHp,after.maxHp-before.maxHp);
    stat("Max mana",bonus.maxMana,after.maxMana-before.maxMana);
    std::string affixes="Affixes: ";
    for(const auto& roll:selected->affixes()) affixes+=std::string(findAffix(roll.id)->name)+" +"+std::to_string(roll.value)+"; ";
    if(selected->affixes().empty()) affixes+="none";
    y+=12; drawWrapped(affixes,860,y,43,kAccent,570);
    drawWrapped("Equipment never refills HP or mana. Lower maximum values clamp your current pools.",860,y,43,kText,640);
    if(dragged) drawWrapped("Green slots accept this item. Release elsewhere to cancel.",860,y,43,kAccent,700);
}
} // namespace engine
