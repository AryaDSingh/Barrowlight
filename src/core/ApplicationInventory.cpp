#include "core/Application.hpp"
#include "core/GameIcons.hpp"
#include "core/PlayLayout.hpp"
#include "world/FloorTheme.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <queue>

namespace engine {
namespace {
constexpr std::size_t kBagPageSize = 50; // the whole bag fits one 10x5 grid; pages only for overflow
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
        inventorySelection_ = std::min<std::size_t>(inventorySelection_ + 10, rows - 1); // one bag row
    else if (key == sf::Keyboard::Key::PageUp)
        inventorySelection_ = inventorySelection_ > 10 ? inventorySelection_ - 10 : 0;
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
        if (onMap(screen)) {
            static constexpr SpriteFrame kChestFrame{"calciumtrice/tiles/dungeon_tileset_calciumtrice.png",
                                                     sf::IntRect({32, 304}, {16, 16})};
            if (!sprites_.draw(window_, kChestFrame, screen, static_cast<float>(playLayout::tileSize))) {
                sf::RectangleShape chest({18.f, 14.f}); chest.setPosition({screen.x+5.f, screen.y+7.f});
                chest.setFillColor(sf::Color(210,145,55)); chest.setOutlineThickness(2.f);
                chest.setOutlineColor(sf::Color(255,220,100)); window_.draw(chest);
            }
        }
    }
    for (const auto& item : groundItems_) {
        const auto p = item->position();
        if (exploredMap_.at(p.x, p.y) != Visibility::Visible) continue;
        const auto screen = worldToScreen(p.x, p.y);
        if (!onMap(screen)) continue;
        sf::CircleShape marker(6.f, 4);
        marker.setPosition({screen.x + 7.f, screen.y + 7.f});
        marker.setFillColor(item->rarity() == ItemRarity::Rare ? sf::Color(255,211,95) :
                            item->rarity() == ItemRarity::Magic ? sf::Color(110,165,255) : sf::Color(210,210,215));
        window_.draw(marker);
    }
    for (const auto& item : groundItems_) {
        if (item->position().x == player_.position().x && item->position().y == player_.position().y) {
            mapHints_.push_back({item->name() + " at your feet. G: pick up", ui::kGold});
            break;
        }
    }
    if (chestExists_ && !chestClaimed_ && chestPosition_.x == player_.position().x && chestPosition_.y == player_.position().y)
        mapHints_.push_back({currentFloor_<=3 ? "Chest of martial gear. G: open (1 turn)" :
            currentFloor_<=6 ? "Chest of casting gear. G: open (1 turn)" :
            "Chest of jewellery. G: open (1 turn)", ui::kGold});
}


namespace {
// Diablo II-style equipment layout. Positions follow the body; indices
// preserve save-file slot IDs (EquipmentSlot order).
sf::FloatRect gearRect(int slot) {
    static const std::array<sf::FloatRect,kEquipmentSlotCount> rects{{
        {{48,158},{104,196}},   // Weapon: left hand, tall
        {{208,168},{120,164}},  // Armour: centre
        {{340,96},{60,60}},     // Charm (amulet): beside the head
        {{384,158},{104,196}},  // OffHand: right hand, tall
        {{218,72},{100,88}},    // Head
        {{136,96},{60,60}},     // Cloak
        {{48,364},{104,92}},    // Hands
        {{218,340},{100,46}},   // Belt
        {{384,364},{104,92}},   // Feet
        {{156,334},{52,52}},    // Ring 1
        {{328,334},{52,52}}}};  // Ring 2
    return rects[static_cast<std::size_t>(slot)];
}
constexpr float kBagX=572, kBagY=112, kBagSlot=52, kBagStride=56;
constexpr int kBagColumns=10;
sf::FloatRect bagRect(std::size_t cell) {
    return {{kBagX+kBagStride*(cell%kBagColumns),kBagY+kBagStride*(cell/kBagColumns)},{kBagSlot,kBagSlot}};
}
const sf::FloatRect kBagArea{{kBagX,kBagY},{kBagStride*kBagColumns,kBagStride*(kBagPageSize/kBagColumns)}};
const sf::FloatRect kCloseButton{{1170,18},{94,32}};
const sf::FloatRect kPrevPage{{kBagX,kBagY+kBagArea.size.y+8},{120,30}}, kNextPage{{kBagX+436,kBagY+kBagArea.size.y+8},{120,30}};
// Buttons for the selected item, so nothing needs a keyboard.
const sf::FloatRect kUseButton{{kBagX,kBagY+kBagArea.size.y+50},{200,36}}, kDropButton{{kBagX+210,kBagY+kBagArea.size.y+50},{160,36}};
bool inBag(sf::Vector2i p) { return kBagArea.contains(sf::Vector2f(p)); }
std::optional<std::size_t> inventoryHit(sf::Vector2i p,std::size_t page,std::size_t count) {
    for(int i=0;i<kEquipmentSlotCount;++i)
        if(gearRect(i).contains(sf::Vector2f(p))) return i;
    for(std::size_t cell=0;cell<kBagPageSize;++cell)
        if(bagRect(cell).contains(sf::Vector2f(p))) {
            const auto index=page*kBagPageSize+cell;
            if(index<count) return index+kEquipmentSlotCount;
        }
    return {};
}
sf::Color rarityColor(const Item& item) {
    return item.rarity()==ItemRarity::Rare?ui::kRare:item.rarity()==ItemRarity::Magic?ui::kMagic:ui::kText;
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
            if(kCloseButton.contains(sf::Vector2f(p))) { inventoryOpen_=false; inventoryDragSource_.reset(); return; }
            if(kPrevPage.contains(sf::Vector2f(p))) { if(inventoryBagPage_) --inventoryBagPage_; return; }
            if(kNextPage.contains(sf::Vector2f(p))) { inventoryBagPage_=std::min(inventoryBagPage_+1,pages-1); return; }
            if(kUseButton.contains(sf::Vector2f(p))) { handleInventoryKey(sf::Keyboard::Key::Enter); return; }
            if(kDropButton.contains(sf::Vector2f(p))) { handleInventoryKey(sf::Keyboard::Key::D); return; }
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
    const auto mouse=mousePixel_?std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)):std::nullopt;
    ui_.panel(window_,{{0,0},{1280,720}},true);
    ui_.heading(window_,"Inventory",{28,18},28);
    ui_.button(window_,kCloseButton,"Close",mouse && kCloseButton.contains(*mouse));
    ui_.text(window_,mode_==GameMode::Town ? "Equipment changes are free in town." : "Equipping, removing and dropping take a turn. Looking is free.",
        {240,28},15,sf::Color(232,196,130));

    const auto& inventory=player_.inventory(); const auto& bag=inventory.items();
    inventorySelection_=std::min(inventorySelection_,bag.size()+kEquipmentSlotCount-1);
    const auto pages=std::max<std::size_t>(1,(bag.size()+kBagPageSize-1)/kBagPageSize);
    inventoryBagPage_=std::min(inventoryBagPage_,pages-1);
    const Item* dragged=inventoryDragSource_ && *inventoryDragSource_>=kEquipmentSlotCount ? bag[*inventoryDragSource_-kEquipmentSlotCount].get():nullptr;

    // --- Equipment, around a faint figure --------------------------------
    const sf::FloatRect doll{{28,64},{480,410}};
    ui_.inset(window_,doll);
    ui_.icon(window_,"shadow-follower",{{168,110},{200,330}},sf::Color(255,255,255,14));
    for(int i=0;i<kEquipmentSlotCount;++i) {
        const auto rect=gearRect(i); const auto slot=static_cast<EquipmentSlot>(i);
        const auto* item=inventory.equipped(slot);
        const bool fits=dragged && slotAccepts(slot,dragged->definition()->slot);
        const sf::Color glow=fits?ui::kGood:inventorySelection_==static_cast<std::size_t>(i)?ui::kGold:
            item && item->rarity()!=ItemRarity::Normal?sf::Color(rarityColor(*item).r,rarityColor(*item).g,rarityColor(*item).b,110):sf::Color::Transparent;
        ui_.inset(window_,rect,glow);
        const float pad=std::min(rect.size.x,rect.size.y)*0.16f;
        const sf::FloatRect art{{rect.position.x+pad,rect.position.y+pad},{rect.size.x-2*pad,rect.size.y-2*pad}};
        if(item) ui_.icon(window_,itemIcon(*item->definition()),art,rarityColor(*item));
        else ui_.icon(window_,slotIcon(slot),art,sf::Color(255,255,255,28));
    }
    float y=486;
    ui_.text(window_,std::string(armourName(inventory.armourKind()))+" armour",{30,y},18,ui::kGold,ui::Font::Title);
    y+=26;
    ui_.paragraph(window_,"Your armour type is the majority of head, body, hands and feet. Empty pieces count as cloth; ties favour the body.",
        30,y,476,14,ui::kMuted);
    y+=8;
    const auto& stats=player_.stats();
    const auto stat=[&](const char* name,const std::string& value,float x,float row) {
        ui_.text(window_,name,{x,row},15,ui::kMuted); ui_.text(window_,value,{x+92,row},15,ui::kText,ui::Font::Bold);
    };
    stat("Strength",std::to_string(stats.strength),30,y); stat("Life",std::to_string(stats.hp)+" / "+std::to_string(stats.maxHp),270,y); y+=22;
    stat("Dexterity",std::to_string(stats.dexterity),30,y); stat("Mana",std::to_string(stats.mana)+" / "+std::to_string(stats.maxMana),270,y); y+=22;
    stat("Intelligence",std::to_string(stats.intelligence),30,y); stat("Gold",std::to_string(gold_),270,y);

    // --- Bag grid ---------------------------------------------------------
    ui_.heading(window_,"Bag",{kBagX,62},22);
    ui_.text(window_,std::to_string(bag.size())+" / "+std::to_string(Inventory::capacity),{kBagX+70,70},16,
        bag.size()>=Inventory::capacity?ui::kBad:ui::kMuted,ui::Font::Bold);
    const auto first=inventoryBagPage_*kBagPageSize;
    for(std::size_t cell=0;cell<kBagPageSize;++cell) {
        const auto i=first+cell; const auto rect=bagRect(cell);
        const bool selected=inventorySelection_==i+kEquipmentSlotCount;
        const Item* item=i<bag.size()?bag[i].get():nullptr;
        ui_.inset(window_,rect,selected?ui::kGold:item && item->rarity()!=ItemRarity::Normal?
            sf::Color(rarityColor(*item).r,rarityColor(*item).g,rarityColor(*item).b,90):sf::Color::Transparent);
        if(item) ui_.icon(window_,itemIcon(*item->definition()),{{rect.position.x+8,rect.position.y+8},{rect.size.x-16,rect.size.y-16}},rarityColor(*item));
    }
    if(pages>1) {
        ui_.button(window_,kPrevPage,"Previous",mouse && kPrevPage.contains(*mouse),inventoryBagPage_>0);
        ui_.button(window_,kNextPage,"Next",mouse && kNextPage.contains(*mouse),inventoryBagPage_+1<pages);
        ui_.textCentered(window_,"Page "+std::to_string(inventoryBagPage_+1)+" of "+std::to_string(pages),
            {{kPrevPage.position.x+120,kPrevPage.position.y},{316,30}},15,ui::kMuted);
    }
    {
        const bool onGear=inventorySelection_<kEquipmentSlotCount;
        const bool hasItem=onGear?inventory.equipped(static_cast<EquipmentSlot>(inventorySelection_))!=nullptr:
            inventorySelection_-kEquipmentSlotCount<bag.size();
        ui_.button(window_,kUseButton,onGear?"Remove (Enter)":"Equip (Enter)",mouse && kUseButton.contains(*mouse),hasItem);
        ui_.button(window_,kDropButton,mode_==GameMode::Town?"Sell in the shop":"Drop (D)",mouse && kDropButton.contains(*mouse),
            hasItem && !onGear && mode_!=GameMode::Town);
    }
    if(bag.size()>Inventory::capacity)
        ui_.text(window_,"Overflow kept: drop or sell items before picking up more.",{kBagX+390,kBagY+kBagArea.size.y+58},14,sf::Color(232,196,130));

    // --- Help and the latest message --------------------------------------
    y=520;
    ui_.text(window_,"Controls",{kBagX,y},16,ui::kGold,ui::Font::Bold); y+=24;
    for(const char* line:{"Drag an item onto a slot to equip it, or back to the bag to remove it.",
                          "Right-click to equip or remove. Hover to compare with what you wear.",
                          "Arrows: select   Enter: equip or remove   D: drop (in the dungeon)   B or Esc: close"}) {
        ui_.text(window_,line,{kBagX,y},14,ui::kMuted); y+=20;
    }
    if(!logMessages_.empty()) { y+=12; ui_.paragraph(window_,logMessages_.back(),kBagX,y,560,15,sf::Color(232,196,130),ui::Font::Body,712); }

    // --- Tooltip for the hovered/selected item, with comparison ------------
    const bool removing=inventorySelection_<kEquipmentSlotCount;
    const auto* selected=removing?inventory.equipped(static_cast<EquipmentSlot>(inventorySelection_)):bag[inventorySelection_-kEquipmentSlotCount].get();
    if(!selected) return;
    const auto& definition=*selected->definition();
    const auto slot=removing?static_cast<EquipmentSlot>(inventorySelection_):inventoryEquipTarget_.value_or(inventory.preferredSlot(definition));
    const auto* current=inventory.equipped(slot);
    std::vector<ui::Line> lines{{selected->name(),rarityColor(*selected),19,ui::Font::Title},
        {(selected->rarity()==ItemRarity::Normal?std::string():std::string(rarityName(selected->rarity()))+" ")+equipmentTypeName(definition),ui::kMuted,14},{""}};
    const auto before=current?current->bonuses():ItemBonuses{};
    const auto after=removing?ItemBonuses{}:selected->bonuses();
    const auto bonus=selected->bonuses();
    const auto statLine=[&](const char* name,int value,int delta) {
        if(!value && !delta) return;
        std::string text=signedNumber(value)+" "+name;
        if(delta) text+="   ("+signedNumber(delta)+" if "+(removing?"removed":"equipped")+")";
        lines.push_back({text,delta>0?ui::kGood:delta<0?ui::kBad:ui::kText,15});
    };
    statLine("Strength",bonus.strength,after.strength-before.strength);
    statLine("Dexterity",bonus.dexterity,after.dexterity-before.dexterity);
    statLine("Intelligence",bonus.intelligence,after.intelligence-before.intelligence);
    statLine("Max life",bonus.maxHp,after.maxHp-before.maxHp);
    statLine("Max mana",bonus.maxMana,after.maxMana-before.maxMana);
    for(const auto& roll:selected->affixes())
        lines.push_back({std::string(findAffix(roll.id)->name)+" +"+std::to_string(roll.value),ui::kMagic,15});
    lines.push_back({""});
    lines.push_back({removing?std::string("Equipped: ")+slotName(slot):
        std::string("Equips to ")+slotName(slot)+(current?", replacing "+current->name():", which is empty"),ui::kInfo,14});
    if(dragged) lines.push_back({"Green slots accept this item. Release elsewhere to cancel.",ui::kGood,14});
    lines.push_back({"Equipment never refills life or mana; lower maximums clamp your current pools.",ui::kMuted,13});
    const auto anchor=removing?gearRect(static_cast<int>(inventorySelection_)):
        bagRect((inventorySelection_-kEquipmentSlotCount)%kBagPageSize);
    // Follow the mouse while it's over the item; otherwise (keyboard
    // selection) sit beside the selected slot.
    const bool overItem=mousePixel_ && inventoryHit(*mousePixel_,inventoryBagPage_,bag.size())==inventorySelection_;
    const sf::Vector2f at=overItem && !dragged?*mouse:sf::Vector2f{anchor.position.x+anchor.size.x-10,anchor.position.y};
    ui_.tooltip(window_,lines,at,340);
}
} // namespace engine
