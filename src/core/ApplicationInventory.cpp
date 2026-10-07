#include "core/Application.hpp"
#include "entities/MonsterFactory.hpp"
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

void Application::finishInventoryTurn(bool keepOpen) {
    inventoryDragSource_.reset();
    inventoryEquipTarget_.reset();
    enforceMinionCap();
    if (!keepOpen) inventoryOpen_ = false; // reveal the enemies' response to the committed action
    cancelTargeting();
    if (mode_==GameMode::Town) return;
    advanceEnemyIntents();
    player_.talents().tickCooldowns();
    currentActor_ = &scheduler_.nextTurn();
    processMonsterTurns();
    if (mode_ != GameMode::GameOver) advanceTurnsUntilPlayerCanAct();
    updateFieldOfView();
}

bool Application::besideChest() const {
    const auto p = player_.position();
    return chestExists_ && std::max(std::abs(p.x - chestPosition_.x), std::abs(p.y - chestPosition_.y)) == 1;
}

// The lid creaks up and the loot tumbles out onto the floor around it --
// unless the chest was never a chest.
void Application::openChest() {
    if (!chestExists_ || chestClaimed_ || nextItemId_ == std::numeric_limits<std::uint64_t>::max()) return;
    const auto at = chestPosition_;
    if (chestMimic_) {
        chestExists_ = false; chestMimic_ = false;
        map_.setTile(at.x, at.y, Tile{TileType::Floor, true, true});
        auto mimic = createMonster(MonsterType::Mimic, at, floorDepth(currentFloor_) >= 8 ? MonsterTier::Elite : MonsterTier::Base);
        scaleDungeonMonster(*mimic, floorDepth(currentFloor_));
        mimic->lastObservedHp = mimic->stats().hp;
        mimic->tactics.alert = 8; mimic->tactics.lastKnown = player_.position();
        mimic->voicedAlert = true;
        soundManager_.playFamily("mimic", 100.f);
        flashActor(*mimic);
        log("The chest's lid splits into a mouth full of teeth!");
        scheduler_.add(*mimic);
        monsters_.push_back(std::move(mimic));
        combatThisTurn_ = true;
        finishInventoryTurn();
        return;
    }
    chestClaimed_ = true;
    soundManager_.playFamily("chest_open", 95.f);
    spillLoot(at, ItemRarity::Magic, 0);
    log("The chest creaks open.");
    finishInventoryTurn();
}

// Loot spills out of a chest (or a mimic's gut) onto free floor beside it.
void Application::spillLoot(Position from, ItemRarity rarity, int quality) {
    const auto region = floorTheme(currentFloor_).region;
    const auto theme = region == FloorRegion::Barracks ? LootTheme::Barracks :
        region == FloorRegion::Sanctum ? LootTheme::Sanctum : LootTheme::Crypts;
    std::vector<Position> open;
    for (int r = 1; r <= 2 && open.empty(); ++r)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                const Position p{from.x + dx, from.y + dy};
                if (std::max(std::abs(dx), std::abs(dy)) != r || !map_.isWalkable(p.x, p.y) || chestAt(p)) continue;
                if (std::any_of(groundItems_.begin(), groundItems_.end(), [&](const auto& item) {
                        return item->position().x == p.x && item->position().y == p.y; })) continue;
                open.push_back(p);
            }
    const Position spot = open.empty() ? from : open[loot_.roll(static_cast<unsigned>(open.size()))];
    groundItems_.push_back(loot_.generate(floorDepth(currentFloor_), quality, nextItemId_++, spot, rarity, theme));
    spawnVfx({Vfx::Kind::Sparkle, {spot.x + .5f, spot.y + .5f}, {spot.x + .5f, spot.y + .5f}, sf::Color(255, 215, 120), 0, .5f, .6f}, .1f);
}

void Application::pickupItem() {
    if (interactVault()) return;
    const auto position = player_.position();
    for (std::size_t i = 0; i < loreDrops_.size(); ++i)
        if (loreDrops_[i].at.x == position.x && loreDrops_[i].at.y == position.y) { readLore(i); return; }
    if (besideChest() && !chestClaimed_) { openChest(); return; }
    const auto found = std::find_if(groundItems_.begin(), groundItems_.end(), [&](const auto& item) {
        return item->position().x == position.x && item->position().y == position.y;
    });
    if (found == groundItems_.end()) {
        if (interactStairs()) return;
        if (nearAltar()) { openShrine(); return; }
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
        finishInventoryTurn(true); return;
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
                finishInventoryTurn(true);
            } else log("Bag full (50). Make space before removing equipment.");
        } else {
            if (key == sf::Keyboard::Key::U) return;
            const auto index = inventorySelection_ - kEquipmentSlotCount;
            if (index >= player_.inventory().items().size()) return;
            const auto name = player_.inventory().items()[index]->name();
            const auto& wanted = *player_.inventory().items()[index]->definition();
            if (!player_.meetsRequirements(wanted)) {
                const auto& st = player_.stats();
                log(st.strength < wanted.reqStrength ? "It's too heavy for you." : st.dexterity < wanted.reqDexterity ?
                    "It's too unwieldy in your hands." : "Its runes mean nothing to you yet.");
            } else if (player_.equip(index, inventoryEquipTarget_)) {
                log("Equipped ", name, ".");
                finishInventoryTurn(true);
            } else log("Remove your shield or two-handed/bow weapon before equipping this item.");
        }
    }
    inventoryEquipTarget_.reset();
    if(inventorySelection_>=kEquipmentSlotCount) inventoryBagPage_=(inventorySelection_-kEquipmentSlotCount)/kBagPageSize;
}

void Application::spawnFixedItems() {
    // The class's starting set: a weapon, body armour and an amulet. It consumes
    // no combat or generation random numbers.
    if (currentFloor_ != 1) return; // starter set only; later floors use paced randomized rewards
    const int startingVariant = playerClass_ == PlayerClass::Mage ? 1 :
                                playerClass_ == PlayerClass::Thief ? 2 : 0;
    const int variant = (startingVariant + currentFloor_ - 1) % 3;
    const std::array<int, 3> armourDefinitions{3, 5, 4};
    const std::array<int, 3> definitions{variant, armourDefinitions[variant], 6 + variant};
    // You start wearing them; anything you can't wear yet waits in your bag.
    for (const int definition : definitions) {
        if (player_.inventory().full()) break;
        player_.inventory().add(std::make_unique<Item>(kItemDefinitions[definition], nextItemId_++));
        player_.equip(player_.inventory().items().size() - 1);
    }
}

void Application::renderGroundItems() {
    if (chestExists_ && exploredMap_.at(chestPosition_.x, chestPosition_.y) != Visibility::Hidden) {
        auto screen = worldToScreen(chestPosition_.x, chestPosition_.y);
        // A mimic breathes: now and then it rises a hair and settles.
        if (chestMimic_ && exploredMap_.at(chestPosition_.x, chestPosition_.y) == Visibility::Visible) {
            const float t = std::fmod(animationClock_.getElapsedTime().asSeconds(), 3.4f);
            if (t < .6f) screen.y -= 1.5f * std::sin(t / .6f * 3.14159f);
        }
        if (onMap(screen)) {
            const SpriteFrame kChestFrame{"calciumtrice/tiles/dungeon_tileset_calciumtrice.png",
                                          sf::IntRect({chestClaimed_ ? 48 : 32, 304}, {16, 16})};
            if (!sprites_.draw(window_, kChestFrame, screen, static_cast<float>(playLayout::tileSize))) {
                sf::RectangleShape chest({18.f, 14.f}); chest.setPosition({screen.x+5.f, screen.y+7.f});
                chest.setFillColor(sf::Color(210,145,55)); chest.setOutlineThickness(2.f);
                chest.setOutlineColor(sf::Color(255,220,100)); window_.draw(chest);
            }
        }
    }
    // Your standard, and the reach it steadies.
    if (banner_ && exploredMap_.at(banner_->at.x, banner_->at.y) != Visibility::Hidden) {
        const int reach = banner_->great ? 3 : 2;
        const auto corner = worldToScreen(banner_->at.x - reach, banner_->at.y - reach);
        const float side = static_cast<float>(playLayout::tileSize * (2 * reach + 1));
        sf::RectangleShape area({side, side}); area.setPosition(corner);
        area.setFillColor(sf::Color(200, 60, 50, 22)); area.setOutlineThickness(1.f); area.setOutlineColor(sf::Color(200, 60, 50, 90));
        window_.draw(area);
        const auto at = worldToScreen(banner_->at.x, banner_->at.y);
        if (onMap(at)) ui_.icon(window_, "spear-feather", {{at.x + 2.f, at.y - 6.f}, {24.f, 30.f}}, sf::Color(220, 70, 55));
    }
    for (const auto& drop : loreDrops_) {
        if (exploredMap_.at(drop.at.x, drop.at.y) != Visibility::Visible) continue;
        const auto screen = worldToScreen(drop.at.x, drop.at.y);
        if (!onMap(screen)) continue;
        sf::CircleShape glow(12.f); glow.setOrigin({12.f, 12.f}); glow.setPosition({screen.x + 14.f, screen.y + 15.f});
        glow.setFillColor(sf::Color(240, 136, 52, 70)); window_.draw(glow);
        ui_.icon(window_, "scroll-unfurled", {{screen.x + 4.f, screen.y + 4.f}, {20.f, 20.f}}, ui::kUnique);
        if (drop.at.x == player_.position().x && drop.at.y == player_.position().y)
            mapHints_.push_back({std::string(drop.id == "warlord_standard" ? "The Warlord's Standard" : "Lore") + " at your feet. G: read it", ui::kUnique});
    }
    for (const auto& item : groundItems_) {
        const auto p = item->position();
        if (exploredMap_.at(p.x, p.y) != Visibility::Visible) continue;
        const auto screen = worldToScreen(p.x, p.y);
        if (!onMap(screen)) continue;
        const sf::Color color = item->rarity() == ItemRarity::Unique ? ui::kUnique : item->rarity() == ItemRarity::Rare ? ui::kRare :
                                item->rarity() == ItemRarity::Magic ? ui::kMagic : sf::Color(225,218,200);
        sf::CircleShape glow(11.f); glow.setOrigin({11.f, 11.f});
        glow.setPosition({screen.x + 14.f, screen.y + 15.f});
        glow.setFillColor(sf::Color(color.r, color.g, color.b, 55)); window_.draw(glow);
        ui_.icon(window_, itemIcon(*item->definition()), {{screen.x + 5.f, screen.y + 5.f}, {18.f, 18.f}}, color);
    }
    for (const auto& item : groundItems_) {
        if (item->position().x == player_.position().x && item->position().y == player_.position().y) {
            mapHints_.push_back({item->name() + " at your feet. G: pick up", ui::kGold});
            break;
        }
    }
    if (!chestClaimed_ && besideChest())
        mapHints_.push_back({"A chest. G or walk into it to open it", ui::kGold});
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
    return item.rarity()==ItemRarity::Unique?ui::kUnique:item.rarity()==ItemRarity::Rare?ui::kRare:item.rarity()==ItemRarity::Magic?ui::kMagic:ui::kText;
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

// While you drag an item it rides on the cursor, so you can see what you're moving.
void Application::renderDraggedItem() {
    if (!inventoryOpen_ || !inventoryDragSource_ || !mousePixel_) return;
    const auto source = *inventoryDragSource_;
    const Item* item = source < kEquipmentSlotCount ? player_.inventory().equipped(static_cast<EquipmentSlot>(source))
        : source - kEquipmentSlotCount < player_.inventory().items().size() ? player_.inventory().items()[source - kEquipmentSlotCount].get() : nullptr;
    if (!item || !item->definition()) return;
    const sf::Vector2f at(*mousePixel_);
    const sf::FloatRect box{{at.x - 30, at.y - 30}, {60, 60}};
    ui_.inset(window_, box, rarityColor(*item));
    ui_.icon(window_, itemIcon(*item->definition()), {{box.position.x + 8, box.position.y + 8}, {44, 44}}, rarityColor(*item));
}

void Application::renderInventory() {
    const auto mouse=mousePixel_?std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)):std::nullopt;
    // Two framed halves: on a wide screen they dock to its edges.
    ui_.glass(window_,{{0,0},{540,720}},true);
    ui_.glass(window_,{{540,0},{740,720}},true);
    ui_.heading(window_,"Inventory",{28,18},28);
    ui_.button(window_,kCloseButton,"Close",mouse && kCloseButton.contains(*mouse));

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
    ui_.text(window_,inventory.armourKind()==ArmourKind::Cloth?std::string("Cloth armour"):std::string(armourName(inventory.armourKind())),
        {30,y},18,ui::kGold,ui::Font::Title);
    y+=30;
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
    if(!logMessages_.empty()) ui_.paragraph(window_,logMessages_.back(),kBagX,y,560,15,sf::Color(232,196,130),ui::Font::Body,712);

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
    const auto* currentDef=current?current->definition():nullptr;
    const auto baseLine=[&](const std::string& name,int value,int was) {
        if(!value && !was) return;
        std::string text=std::to_string(value)+" "+name;
        if(value!=was && !removing) text+="   ("+signedNumber(value-was)+")";
        lines.push_back({text,value>was&&!removing?ui::kGood:value<was&&!removing?ui::kBad:ui::kText,16,ui::Font::Bold});
    };
    if(definition.damage || (currentDef && currentDef->damage && definition.slot==EquipmentSlot::Weapon))
        baseLine(definition.weaponKind==WeaponKind::Staff?"spell damage":"damage",definition.damage,currentDef?currentDef->damage:0);
    if(definition.defence) {
        const auto kind=definition.weaponKind==WeaponKind::Shield?ArmourKind::Heavy:definition.armourKind;
        baseLine(kind==ArmourKind::Heavy?"armour":kind==ArmourKind::Light?"evasion":"ward",definition.defence,
                 currentDef && currentDef->armourKind==definition.armourKind?currentDef->defence:0);
    }
    if(definition.implicit) lines.push_back({affixText(*findAffix(definition.implicit),definition.implicitValue),ui::kText,15});
    const auto& st=player_.stats();
    std::string needs;
    const auto need=[&](int value,int have,const char* stat) {
        if(!value) return;
        needs+=(needs.empty()?"Requires ":", ")+std::to_string(value)+" "+stat;
        if(have<value) needs+="!";
    };
    need(definition.reqStrength,st.strength,"Str"); need(definition.reqDexterity,st.dexterity,"Dex"); need(definition.reqIntelligence,st.intelligence,"Int");
    if(!needs.empty()) {
        const bool unmet=needs.find('!')!=std::string::npos;
        needs.erase(std::remove(needs.begin(),needs.end(),'!'),needs.end());
        lines.push_back({needs,unmet?ui::kBad:ui::kMuted,14});
    }
    if(definition.damage || definition.defence || definition.implicit || !needs.empty()) lines.push_back({""});
    statLine("Strength",bonus.strength,after.strength-before.strength);
    statLine("Dexterity",bonus.dexterity,after.dexterity-before.dexterity);
    statLine("Intelligence",bonus.intelligence,after.intelligence-before.intelligence);
    statLine("Max life",bonus.maxHp,after.maxHp-before.maxHp);
    statLine("Max mana",bonus.maxMana,after.maxMana-before.maxMana);
    for(const auto& roll:selected->affixes()) {
        const auto& affix=*findAffix(roll.id);
        lines.push_back({affixText(affix,roll.value),affix.cursed?sf::Color(220,120,200):attributeAffix(affix.stat)?ui::kMagic:ui::kInfo,15});
        if(affix.cursed) lines.push_back({"   "+affixPenaltyText(affix,roll.value),ui::kBad,15});
    }
    if(definition.lore) { lines.push_back({""}); lines.push_back({definition.lore,sf::Color(200,150,100),14}); }
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
