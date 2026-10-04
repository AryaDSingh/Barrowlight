#include "core/Application.hpp"
#include <queue>
#include <limits>
#include <algorithm>
#include <cstdlib>
#include "core/GameIcons.hpp"
#include "core/PlayLayout.hpp"
#include "core/ScreenLayout.hpp"
#include "world/FloorTheme.hpp"

namespace engine {
namespace {
using namespace screen;
}

void Application::handleVaultMouse(const sf::Event& event) {
    const auto* click=event.getIf<sf::Event::MouseButtonPressed>();
    if(!click || click->button!=sf::Mouse::Button::Left || !vaultMenu_) return;
    const auto point=sf::Vector2f(click->position);
    if(vaultCancel(vaultMenu_).contains(point)) { handleVaultKey(sf::Keyboard::Key::Escape); return; }
    if(vaultCommit(vaultMenu_).contains(point)) { handleVaultKey(sf::Keyboard::Key::Enter); return; }
    if(vaultMenu_==2) for(std::size_t i=0;i<vaultRewards_.size();++i)
        if(vaultRewardCard(static_cast<int>(i)).contains(point)) { vaultSelection_=i; return; }
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
        cancelTargeting(); vaultMenu_=1; handleVaultKey(sf::Keyboard::Key::Enter); return true;
    }
    if (vaultOpened_ && near(vaultCenter_)) {
        if (!vaultCleared()) { log("The cache won't budge."); return true; }
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
        log("The vault door grinds open.");
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
    const auto marker=[&](Position tile,const char* icon,sf::Color color) {
        if (exploredMap_.at(tile.x,tile.y)!=Visibility::Visible) return;
        const auto pos=worldToScreen(tile.x,tile.y);
        if (!onMap(pos)) return;
        ui_.icon(window_,icon,{{pos.x+3,pos.y+3},{22,22}},color);
    };
    if (!vaultOpened_) marker(vaultEntrance_,"locked-chest",sf::Color(235,150,255));
    else if (!vaultClaimed_) marker(vaultCenter_,"locked-chest",vaultCleared()?ui::kRare:sf::Color(150,140,150));
    const bool nearEntrance=std::abs(p.x-vaultEntrance_.x)+std::abs(p.y-vaultEntrance_.y)<=1;
    const bool nearCache=std::abs(p.x-vaultCenter_.x)+std::abs(p.y-vaultCenter_.y)<=1;
    if (!vaultMenu_) {
        if ((!vaultOpened_ && nearEntrance) || (vaultOpened_ && !vaultClaimed_ && nearCache)) {
            mapHints_.push_back({!vaultOpened_ ? "A sealed door. G or click to open it" :
                vaultCleared()?"The cache. G to open it":"The cache, locked fast.", sf::Color(235,190,255)});
        }
        return;
    }
    const auto mouse=mousePixel_?std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)):std::nullopt;
    const auto hovered=[&](const sf::FloatRect& r){ return mouse && r.contains(*mouse); };
    using namespace screen;
    if (vaultMenu_==1) {
        beginMenu(140);
        ui_.panel(window_,kVaultWarning,true,sf::Color(150,140,150));
        const float x=kVaultWarning.position.x, w=kVaultWarning.size.x;
        ui_.icon(window_,"locked-chest",{{x+w/2-34,kVaultWarning.position.y+22},{68,68}},sf::Color(235,190,255));
        ui_.textCentered(window_,"Optional vault",{{x,kVaultWarning.position.y+96},{w,44}},34,ui::kGold,ui::Font::Title);
        float y=kVaultWarning.position.y+152;
        const auto line=[&](const std::string& text,sf::Color color,unsigned size=17) {
            ui_.textCentered(window_,text,{{x,y},{w,24}},size,color); y+=30;
        };
        line("Inside: an Elite Goblin and an Archer, in a small room.",ui::kText);
        line("The Elite Goblin has 1.5x life and 1.4x damage. The Archer shoots from range.",sf::Color(240,180,130),16);
        line("Reward: choose one of three rare items once both guards are dead.",ui::kRare);
        y+=8;
        line("Opening takes a turn and the guards can act at once.",ui::kMuted,16);
        line("You can retreat, and skipping the vault never blocks the stairs.",ui::kMuted,16);
        ui_.button(window_,vaultCommit(1),"Open the vault (Enter)",hovered(vaultCommit(1)),true,16);
        ui_.button(window_,vaultCancel(1),"Leave it sealed (Esc)",hovered(vaultCancel(1)),true,16);
        return;
    }
    ui_.panel(window_,{{0,0},{1280,720}},true,sf::Color(150,140,150));
    ui_.heading(window_,"Choose one reward",{60,30},32);
    ui_.text(window_,"Click a reward to inspect it, then claim it. The other two are lost.",{420,46},15,ui::kMuted);
    for (std::size_t i=0;i<vaultRewards_.size();++i) {
        const auto& item=*vaultRewards_[i]; const auto b=item.bonuses();
        const auto r=vaultRewardCard(static_cast<int>(i));
        const bool chosen=i==vaultSelection_;
        ui_.inset(window_,r,chosen?ui::kRare:hovered(r)?ui::kBronze:sf::Color::Transparent);
        const sf::FloatRect art{{r.position.x+r.size.x/2-56,r.position.y+22},{112,112}};
        ui_.inset(window_,art,sf::Color(255,214,96,chosen?200:70));
        ui_.icon(window_,itemIcon(*item.definition()),{{art.position.x+16,art.position.y+16},{80,80}},ui::kRare);
        float y=r.position.y+146;
        ui_.paragraph(window_,item.name(),r.position.x+20,y,r.size.x-40,20,ui::kRare,ui::Font::Title);
        ui_.text(window_,equipmentTypeName(*item.definition()),{r.position.x+20,y},15,ui::kMuted); y+=30;
        const auto* equipped=player_.inventory().equipped(player_.inventory().preferredSlot(*item.definition()));
        const auto old=equipped?equipped->bonuses():ItemBonuses{};
        const auto stat=[&](const char* name,int value,int previous) {
            if(!value && !previous) return;
            ui_.text(window_,(value>=0?"+":"")+std::to_string(value)+" "+name,{r.position.x+20,y},16,ui::kText);
            const int delta=value-previous;
            if(delta) ui_.text(window_,std::string("(")+(delta>0?"+":"")+std::to_string(delta)+")",{r.position.x+200,y},15,delta>0?ui::kGood:ui::kBad);
            y+=23;
        };
        stat("Strength",b.strength,old.strength); stat("Dexterity",b.dexterity,old.dexterity);
        stat("Intelligence",b.intelligence,old.intelligence); stat("Max life",b.maxHp,old.maxHp); stat("Max mana",b.maxMana,old.maxMana);
        for(const auto& roll:item.affixes()) {
            ui_.text(window_,std::string(findAffix(roll.id)->name)+" +"+std::to_string(roll.value),{r.position.x+20,y},15,ui::kMagic); y+=22;
        }
        y+=6;
        ui_.paragraph(window_,"Compared with "+(equipped?equipped->name():std::string("an empty slot")),r.position.x+20,y,r.size.x-40,14,ui::kMuted,
            ui::Font::Body,r.position.y+r.size.y-8);
    }
    const bool canClaim=!player_.inventory().full() && !vaultRewards_.empty();
    ui_.button(window_,vaultCommit(2),"Claim it (Enter, 1 turn)",hovered(vaultCommit(2)),canClaim,16);
    ui_.button(window_,vaultCancel(2),"Decide later (Esc)",hovered(vaultCancel(2)),true,16);
    if(player_.inventory().full()) ui_.text(window_,"Your bag is full. Decide later, make space, then come back.",{720,502},16,ui::kBad);
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
    if (monster.vaultGuard && vaultCleared()) log("The vault falls silent.");
    if (!monster.rewardsEligible()) return;
    if (monster.eventChampion && !trialGuardianChampion(monster.eventChampion)) grantUnique(monster.position());
    const bool unique=isUniqueMonster(monster.type());
    const bool special=unique || monster.tier()!=MonsterTier::Base;
    const int quality = boss || unique ? 2 : static_cast<int>(monster.tier());
    const int count = boss ? 2 : 1;
    if (!boss && !special && (ordinaryDrops_ >= 2 || loot_.roll(100) >= 20u)) return;
    // Elites usually, not always, carry something.
    if (!boss && !unique && monster.tier()==MonsterTier::Elite && loot_.roll(100) >= 60u) return;
    for (int i = 0; i < count && nextItemId_ < std::numeric_limits<std::uint64_t>::max(); ++i) {
        auto item = loot_.generate(floorDepth(currentFloor_), quality, nextItemId_++, monster.position(),
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
