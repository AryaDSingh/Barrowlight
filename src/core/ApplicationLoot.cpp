#include "core/Application.hpp"
#include <queue>
#include <limits>
#include <algorithm>
#include <cstdlib>
#include "core/PlayLayout.hpp"
#include "world/FloorTheme.hpp"

namespace engine {
namespace {
sf::FloatRect vaultRewardRect(std::size_t row) { return {{50,135+155.f*row},{1180,125}}; }
sf::FloatRect vaultCommitRect(int menu) { return {{60,menu==1?425.f:590.f},{295,44}}; }
sf::FloatRect vaultCancelRect(int menu) { return {{380,menu==1?425.f:590.f},{280,44}}; }
}

void Application::handleVaultMouse(const sf::Event& event) {
    const auto* click=event.getIf<sf::Event::MouseButtonPressed>();
    if(!click || click->button!=sf::Mouse::Button::Left || !vaultMenu_) return;
    const auto point=sf::Vector2f(click->position);
    if(vaultCancelRect(vaultMenu_).contains(point)) { handleVaultKey(sf::Keyboard::Key::Escape); return; }
    if(vaultCommitRect(vaultMenu_).contains(point)) { handleVaultKey(sf::Keyboard::Key::Enter); return; }
    if(vaultMenu_==2) for(std::size_t i=0;i<vaultRewards_.size();++i)
        if(vaultRewardRect(i).contains(point)) { vaultSelection_=i; return; }
}

bool Application::vaultCleared() const {
    return vaultExists_ && vaultOpened_ && std::none_of(monsters_.begin(),monsters_.end(),
        [](const auto& m){return m->vaultGuard && m->stats().hp>0;});
}

bool Application::interactVault() {
    if (!vaultExists_ || vaultClaimed_) return false;
    const auto p=player_.position();
    auto near=[&](Position q){return std::abs(p.x-q.x)+std::abs(p.y-q.y)<=1;};
    if (!vaultOpened_ && near(vaultEntrance_)) {
        cancelTargeting(); vaultMenu_=1; return true;
    }
    if (vaultOpened_ && near(vaultCenter_)) {
        if (!vaultCleared()) { log("Vault cache sealed: defeat both vault guards first."); return true; }
        cancelTargeting(); vaultSelection_=0; vaultMenu_=2; return true;
    }
    return false;
}

void Application::handleVaultKey(sf::Keyboard::Key key) {
    if (key==sf::Keyboard::Key::Escape) { vaultMenu_=0; return; }
    if (key==sf::Keyboard::Key::F5) { saveGame(); return; }
    if (key==sf::Keyboard::Key::F9) { loadGame(); return; }
    if (vaultMenu_==2) {
        const auto count=vaultRewards_.size();
        if (count && key==sf::Keyboard::Key::Up) vaultSelection_=(vaultSelection_+count-1)%count;
        if (count && key==sf::Keyboard::Key::Down) vaultSelection_=(vaultSelection_+1)%count;
    }
    if (key!=sf::Keyboard::Key::Enter) return;
    if (vaultMenu_==1 && !vaultOpened_) {
        vaultOpened_=true; vaultMenu_=0;
        map_.setTile(vaultEntrance_.x,vaultEntrance_.y,Tile{TileType::Floor,true,true});
        log("Vault opened. Defeat the Elite Goblin and Archer; retreat is allowed.");
        updateFieldOfView(); finishInventoryTurn();
    } else if (vaultMenu_==2 && vaultCleared() && !vaultClaimed_ && vaultSelection_<vaultRewards_.size()) {
        if (player_.inventory().full()) { log("Bag full (50). Make space, then claim your reward."); return; }
        log("Vault reward: ",vaultRewards_[vaultSelection_]->name()," (in your bag).");
        player_.inventory().add(std::move(vaultRewards_[vaultSelection_]));
        vaultRewards_.clear(); vaultClaimed_=true; vaultMenu_=0;
        finishInventoryTurn();
    }
}

void Application::renderVault() {
    if (!vaultExists_) return;
    const auto p=player_.position();
    const auto marker=[&](Position tile,const char* glyph,sf::Color color) {
        if (exploredMap_.at(tile.x,tile.y)!=Visibility::Visible) return;
        const auto pos=worldToScreen(tile.x,tile.y);
        if (pos.x<0 || pos.x>=playLayout::mapWidth || pos.y<playLayout::mapTop ||
            pos.y>=playLayout::mapTop+playLayout::mapHeight) return;
        drawText(glyph,pos.x+5,pos.y+1,22,color);
    };
    if (!vaultOpened_) marker(vaultEntrance_,"V",sf::Color(235,140,255));
    else if (!vaultClaimed_) marker(vaultCenter_,vaultCleared()?"!":"X",sf::Color(255,215,100));
    const bool nearEntrance=std::abs(p.x-vaultEntrance_.x)+std::abs(p.y-vaultEntrance_.y)<=1;
    const bool nearCache=std::abs(p.x-vaultCenter_.x)+std::abs(p.y-vaultCenter_.y)<=1;
    if (!vaultMenu_) {
        if ((!vaultOpened_ && nearEntrance) || (vaultOpened_ && !vaultClaimed_ && nearCache)) {
            sf::RectangleShape banner({880.f,28.f}); banner.setPosition({8.f,90.f});
            banner.setFillColor(sf::Color(25,15,35,245)); window_.draw(banner);
            drawText(!vaultOpened_ ? "Optional vault: Elite Goblin + Archer. G: read warning (free)." :
                vaultCleared()?"Vault cleared! G: choose one of three rare rewards.":"Vault cache: defeat both guards to claim a reward.",
                18.f,94.f,14,sf::Color(245,215,150));
        }
        return;
    }
    sf::RectangleShape backdrop({1280.f,720.f}); backdrop.setFillColor(sf::Color(12,15,25,250)); window_.draw(backdrop);
    drawText(vaultMenu_==1?"OPTIONAL VAULT":"CHOOSE ONE VAULT REWARD",60,45,26,sf::Color(235,190,255));
    const auto button=[&](const sf::FloatRect& rect,const std::string& label,bool enabled=true) {
        sf::RectangleShape shape(rect.size); shape.setPosition(rect.position);
        const bool hover=mousePixel_ && rect.contains(sf::Vector2f(*mousePixel_));
        shape.setFillColor(enabled?(hover?sf::Color(45,70,82):sf::Color(28,43,57)):sf::Color(29,30,38));
        shape.setOutlineThickness(1); shape.setOutlineColor(sf::Color(130,190,210)); window_.draw(shape);
        drawText(label,rect.position.x+10,rect.position.y+11,16,enabled?sf::Color::White:sf::Color(155,160,175));
    };
    button(vaultCommitRect(vaultMenu_),vaultMenu_==1?"Enter: Open vault (1 turn)":"Enter: Claim (1 turn)",
        vaultMenu_==1 || (!player_.inventory().full() && !vaultRewards_.empty()));
    button(vaultCancelRect(vaultMenu_),vaultMenu_==1?"Esc: Leave sealed":"Esc: Decide later");
    if (vaultMenu_==1) {
        drawText("Danger: one Elite Goblin and one Archer in a small room.",60,120,19,sf::Color::White);
        drawText("Elite Goblin: 1.5x HP, 1.4x damage. Archer attacks from range.",60,165,17,sf::Color(245,190,130));
        drawText("Reward: choose one of three rare items after defeating both guards.",60,230,18,sf::Color(255,220,120));
        drawText("Opening costs one turn. Enemies can respond immediately.",60,280,18,sf::Color::White);
        drawText("You may retreat. Skipping the vault never blocks the next floor.",60,330,18,sf::Color::White);
        drawText("Click a button or use its keyboard shortcut. F5/F9: save/load",60,495,16,sf::Color(140,235,220));
        return;
    }
    drawText("Click a reward or use Up/Down to select. The Claim button takes it. F5/F9: save/load",60,90,16,sf::Color::White);
    for (std::size_t i=0;i<vaultRewards_.size();++i) {
        const auto& item=*vaultRewards_[i]; const auto b=item.bonuses();
        const float y=145.f+155.f*i;
        const auto rect=vaultRewardRect(i);
        sf::RectangleShape card(rect.size); card.setPosition(rect.position);
        card.setFillColor(i==vaultSelection_?sf::Color(40,53,70):sf::Color(23,30,43));
        card.setOutlineThickness(1);
        card.setOutlineColor(i==vaultSelection_?sf::Color(255,220,100):sf::Color(75,95,115)); window_.draw(card);
        drawText(std::string(i==vaultSelection_?"> ":"  ")+item.name(),60,y,20,
            i==vaultSelection_?sf::Color(255,220,100):sf::Color::White);
        drawText(equipmentTypeName(*item.definition())+" | STR +"+std::to_string(b.strength)+
            "  DEX +"+std::to_string(b.dexterity)+"  INT +"+std::to_string(b.intelligence)+
            "  HP +"+std::to_string(b.maxHp)+"  Mana +"+std::to_string(b.maxMana),80,y+36,16,sf::Color(180,230,220));
        const auto* equipped=player_.inventory().equipped(player_.inventory().preferredSlot(*item.definition()));
        const auto old=equipped?equipped->bonuses():ItemBonuses{};
        auto delta=[](int n){return (n>=0?std::string("+"):std::string{})+std::to_string(n);};
        drawText("Compared to "+(equipped?equipped->name():std::string("empty slot"))+":",80,y+65,14,sf::Color(190,190,205));
        drawText("STR "+delta(b.strength-old.strength)+"  DEX "+delta(b.dexterity-old.dexterity)+
            "  INT "+delta(b.intelligence-old.intelligence)+"  HP "+delta(b.maxHp-old.maxHp)+
            "  Mana "+delta(b.maxMana-old.maxMana),80,y+89,15,sf::Color(190,190,205));
    }
    drawText("Claimed gear goes into your bag. The other two rewards are discarded.",60,650,16,sf::Color(245,215,150));
    if(player_.inventory().full()) drawText("Bag full (50). Decide later, make space, then return to claim.",60,682,16,sf::Color(255,165,130));
}

void Application::spawnFloorChest() {
    ordinaryDrops_ = 0;
    chestExists_ = false; chestClaimed_ = false;
    std::queue<Position> frontier;
    std::vector<bool> seen(static_cast<std::size_t>(map_.width()) * map_.height());
    auto enqueue = [&](Position p) {
        if (!map_.isWalkable(p.x, p.y)) return;
        const auto index = static_cast<std::size_t>(p.y) * map_.width() + p.x;
        if (seen[index]) return;
        seen[index] = true; frontier.push(p);
    };
    enqueue(player_.position());
    while (!frontier.empty()) {
        const auto p = frontier.front(); frontier.pop();
        if (map_.tileAt(p.x, p.y).type == TileType::Floor && !isOccupied(p, nullptr)) {
            chestPosition_ = p; chestExists_ = true;
        }
        enqueue({p.x+1,p.y}); enqueue({p.x-1,p.y}); enqueue({p.x,p.y+1}); enqueue({p.x,p.y-1});
    }
}

void Application::rewardMonster(Monster& monster, bool boss) {
    if (monster.vaultGuard && vaultCleared()) log("Vault cleared! Return to its cache and press G to choose a rare item.");
    if (!monster.rewardsEligible()) return;
    const bool unique=isUniqueMonster(monster.type());
    const bool special=unique || monster.tier()!=MonsterTier::Base;
    const int quality = boss || unique ? 2 : static_cast<int>(monster.tier());
    const int count = boss ? 2 : 1;
    if (!boss && !special && (ordinaryDrops_ >= 2 || loot_.roll(100) >= static_cast<unsigned>(35 + quality * 15))) return;
    for (int i = 0; i < count && nextItemId_ < std::numeric_limits<std::uint64_t>::max(); ++i) {
        auto item = loot_.generate(currentFloor_, quality, nextItemId_++, monster.position(),
                                   boss || unique || monster.tier()==MonsterTier::Nightmare ? ItemRarity::Rare :
                                   special ? ItemRarity::Magic : ItemRarity::Normal);
        if (boss) {
            log("Boss reward: ", item->name(), " (in your bag).");
            player_.inventory().add(std::move(item)); // guaranteed rewards retain overflow rather than being lost
            if (player_.inventory().items().size()>Inventory::capacity) log("Boss reward kept in bag overflow. Sell or drop gear to make space.");
        } else {
            if (!special) ++ordinaryDrops_;
            log(monster.name(), " drops ", item->name(), ".");
            groundItems_.push_back(std::move(item));
        }
    }
}
}
