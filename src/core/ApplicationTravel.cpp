#include "core/Application.hpp"
#include "entities/RunProgression.hpp"

#include <algorithm>
#include <limits>
#include <queue>
#include <random>
#include "core/GameIcons.hpp"
#include "core/PlayLayout.hpp"
#include "core/ScreenLayout.hpp"
#include "world/FloorTheme.hpp"

namespace engine {
namespace {
bool sameTile(Position a,Position b) { return a.x==b.x && a.y==b.y; }
bool training(const Item& item) { return std::string_view(item.definition()->id).find("training_")==0; }
int salePrice(const Item& item) { return training(item)?0:5+10*static_cast<int>(item.rarity())+2*item.rollTier(); }
constexpr int kShopPrice=30;
using namespace screen;
bool contains(const sf::FloatRect& rect,sf::Vector2i p) { return rect.contains(sf::Vector2f(p)); }
}

bool Application::dangerNearby() const {
    for (const auto& effect:player_.statusEffects().active())
        if (effect.type==StatusEffectType::Poison || effect.type==StatusEffectType::Burn ||
            effect.type==StatusEffectType::Stun || effect.type==StatusEffectType::Chill || effect.type==StatusEffectType::Marked || isCurse(effect.type)) return true;
    for (const auto& m:monsters_) if (!m->allied && m->stats().hp>0 &&
        (m->intent() || exploredMap_.at(m->position().x,m->position().y)==Visibility::Visible)) return true;
    return false;
}

void Application::handleTownMouse(const sf::Event& event) {
    if(dungeonMenu_) { handleDungeonMouse(event); return; }
    if(const auto* moved=event.getIf<sf::Event::MouseMoved>()) {
        mousePixel_=moved->position;
        const auto& bag=player_.inventory().items();
        const auto stock=rewardItemDefinitions();
        const auto count=selling_?bag.size():stock.size();
        const auto first=(shopSelection_/kTownRowsPerPage)*kTownRowsPerPage;
        for(std::size_t row=0;row<kTownRowsPerPage && first+row<count;++row)
            if(contains(townRow(static_cast<int>(row)),moved->position)) { shopSelection_=first+row; break; }
    }
    if(const auto* wheel=event.getIf<sf::Event::MouseWheelScrolled>()) {
        if(wheel->position.x>kTownPreview.position.x) return;
        const auto stock=rewardItemDefinitions();
        const auto count=selling_?player_.inventory().items().size():stock.size();
        if(!count) return;
        shopSelection_=wheel->delta<0?std::min(shopSelection_+9,count-1):shopSelection_>9?shopSelection_-9:0;
    }
    const auto* click=event.getIf<sf::Event::MouseButtonPressed>();
    if(!click || click->button!=sf::Mouse::Button::Left) return;
    const auto p=click->position;
    if(contains(kTownDungeons,p)) { handleTownKey(sf::Keyboard::Key::M); return; }
    if(contains(kTownBuy,p)) { selling_=false; shopSelection_=0; return; }
    if(contains(kTownSell,p)) { selling_=true; shopSelection_=0; return; }
    if(contains(kTownInn,p)) { handleTownKey(sf::Keyboard::Key::R); return; }
    if(contains(kTownEquipment,p)) { openInventory(); return; }
    if(contains(kTownReturn,p)) { handleTownKey(sf::Keyboard::Key::D); return; }
    if(contains(kTownTrade,p)) { handleTownKey(sf::Keyboard::Key::Enter); return; }
    const auto stock=rewardItemDefinitions();
    const auto count=selling_?player_.inventory().items().size():stock.size();
    const std::size_t perPage=kTownRowsPerPage;
    const auto first=(shopSelection_/perPage)*perPage;
    if(contains(kTownPrevious,p)) { shopSelection_=first>=perPage?first-perPage:0; return; }
    if(contains(kTownNext,p)) { shopSelection_=std::min(first+perPage,count?count-1:0); return; }
    for(std::size_t row=0;row<perPage;++row) if(contains(townRow(static_cast<int>(row)),p) && first+row<count) {
        shopSelection_=first+row; return; // selection previews; the trade button commits
    }
}

void Application::recordQuietTurn() {
    const bool danger=combatThisTurn_ || dangerNearby();
    if (danger) quietTurns_=0;
    else {
        quietTurns_=std::min(10,quietTurns_+1);
        if (quietTurns_>=10 && player_.stats().hp>0) {
            const int recovery=std::max(1,(player_.stats().maxHp+19)/20);
            player_.stats().hp=std::min(player_.stats().maxHp,player_.stats().hp+recovery);
        }
    }
    combatThisTurn_=false;
    if (danger && autoExploring_) stopAutoExplore("danger or combat detected.");
    if (danger && restTurns_>0) { restTurns_=0; log("Rest interrupted by danger."); }
}

void Application::startRest() {
    if (dangerNearby()) { log("Cannot rest: enemies, an attack warning or harmful effects are present."); return; }
    autoExploring_=false;
    cancelTargeting(); restTurns_=100; restClock_.restart();
    log("Resting until HP, mana and cooldowns recover. HP recovery starts at 10 quiet turns. Any key/click stops.");
}

void Application::reviveInTown() {
    // Resolve only from the death screen, after the combat iteration has ended.
    if(mode_!=GameMode::GameOver || wonGame_ || !adventureMode_ || extraLives_<=0) return;
    auto next=captureState();
    --next.extraLives;
    next.inTown=true; next.quietTurns=10;
    next.playerStats.hp=player_.stats().maxHp;
    next.playerStats.mana=player_.stats().maxMana;
    next.playerStatusEffects.clear();
    for(auto& talent:next.playerTalents) talent.cooldown=0;
    next.monsters.erase(std::remove_if(next.monsters.begin(),next.monsters.end(),[](const auto& m){return m.allied || m.hp<=0;}),next.monsters.end());
    std::queue<Position> candidates; std::vector<Position> seen;
    candidates.push(floorEntrance_);
    bool landed=false;
    while(!candidates.empty()) {
        const auto p=candidates.front(); candidates.pop();
        if(!map_.isWalkable(p.x,p.y) || std::any_of(seen.begin(),seen.end(),[&](Position q){return sameTile(p,q);})) continue;
        seen.push_back(p);
        if(std::none_of(next.monsters.begin(),next.monsters.end(),[&](const auto& m){return sameTile(m.position,p);})) {
            next.playerPosition=p; landed=true; break;
        }
        candidates.push({p.x+1,p.y}); candidates.push({p.x-1,p.y});
        candidates.push({p.x,p.y+1}); candidates.push({p.x,p.y-1});
    }
    if(!landed) { log("No free return tile; revival has not spent a life."); return; }
    if(!restoreState(next)) return;
    mode_=GameMode::Town; selling_=false; shopSelection_=0;
    log("Revived in town. Extra lives left: ",extraLives_,". Return leads to the floor entrance.");
}

void Application::returnToTown() {
    if (mode_!=GameMode::Playing) return;
    if (dangerNearby() || quietTurns_<10 || combatThisTurn_) {
        log("Waystone needs 10 quiet turns. Progress: ",quietTurns_,"/10. R: wait safely."); return;
    }
    cancelTargeting(); inventoryOpen_=false; vaultMenu_=0; shrineMenu_=false; exitMenu_=false; restTurns_=0;
    dissolveMinions();
    selling_=false; shopSelection_=0; dungeonMenu_=false; mode_=GameMode::Town;
    log("Waystone returns you to town. Your dungeon progress is preserved.");
}

bool Application::interactStairs() {
    if (sameTile(player_.position(),floorEntrance_)) {
        if (currentFloor_==1) returnToTown();
        else travelFloor(currentFloor_-1);
        return true;
    }
    if (sameTile(player_.position(),floorExit_)) { cancelTargeting(); exitMenu_=true; return true; }
    return false;
}

void Application::travelFloor(int destination,bool fromTown) {
    if(fromTown && mode_!=GameMode::Town) return;
    if(destination<1 || destination>kRunFinalFloor) return;
    if(destination==currentFloor_) {
        if(fromTown) { dungeonMenu_=false; handleTownKey(sf::Keyboard::Key::D); }
        return;
    }
    if (!fromTown && (dangerNearby() || combatThisTurn_)) { log("Stairs are unsafe while enemies, warnings or harmful effects are present."); return; }
    const bool down=destination>currentFloor_;
    auto found=floorCache_.find(destination);
    if (!fromTown && !down && found==floorCache_.end()) { log("That depth has no saved floor. Choose it from town to explore it."); return; }
    if (!fromTown && down && !sameTile(player_.position(),floorExit_)) return;
    dissolveMinions();
    auto current=captureState(false);
    floorCache_[currentFloor_]=current;
    cancelTargeting(); exitMenu_=false; restTurns_=0;
    if (found==floorCache_.end()) {
        currentFloor_=destination; mode_=GameMode::Playing; dungeonMenu_=false;
        regenerateLevel(std::random_device{}());
        log("Entered ",dungeonName(dungeonIndex(currentFloor_))," depth ",(currentFloor_-1)%10+1,". G: stairs; H: Waystone after 10 quiet turns.");
        return;
    }
    const auto& floor=found->second;
    // Import the floor only. Character, money, inventory, item IDs and loot RNG
    // remain the current campaign's values, never those in the archived snapshot.
    auto next=current; next.inTown=false;
    next.map=floor.map; next.exploredMap=floor.exploredMap; next.monsters=floor.monsters;
    next.currentFloor=destination; next.floorEntrance=floor.floorEntrance; next.floorExit=floor.floorExit;
    next.chestPosition=floor.chestPosition; next.chestExists=floor.chestExists; next.chestClaimed=floor.chestClaimed;
    next.ordinaryDrops=floor.ordinaryDrops;
    next.vaultExists=floor.vaultExists; next.vaultOpened=floor.vaultOpened; next.vaultClaimed=floor.vaultClaimed;
    next.vaultCenter=floor.vaultCenter; next.vaultEntrance=floor.vaultEntrance;
    next.landmark=floor.landmark; next.landmarkAltar=floor.landmarkAltar; next.landmarkUsed=floor.landmarkUsed;
    next.items.erase(std::remove_if(next.items.begin(),next.items.end(),[](const auto& item){return item.location<=-2;}),next.items.end());
    for (const auto& item:floor.items) if (item.location<=-2) next.items.push_back(item);
    next.playerPosition=(fromTown || down)?floor.floorEntrance:floor.floorExit;
    // A monster may have wandered onto the return stairs before we left. Find
    // the nearest free connected tile without deleting or moving that monster.
    std::queue<Position> candidates; std::vector<Position> seen;
    candidates.push(next.playerPosition);
    bool landed=false;
    while (!candidates.empty()) {
        const auto p=candidates.front(); candidates.pop();
        if (!next.map.isWalkable(p.x,p.y) || std::any_of(seen.begin(),seen.end(),[&](Position q){return sameTile(p,q);})) continue;
        seen.push_back(p);
        if (std::none_of(next.monsters.begin(),next.monsters.end(),[&](const auto& m){return sameTile(m.position,p);})) {
            next.playerPosition=p; landed=true; break;
        }
        candidates.push({p.x+1,p.y}); candidates.push({p.x-1,p.y});
        candidates.push({p.x,p.y+1}); candidates.push({p.x,p.y-1});
    }
    if (!landed) { log("No free arrival tile on that floor."); return; }
    if (restoreState(next,false)) log("Returned to preserved floor ",destination,".");
}

void Application::handleTownKey(sf::Keyboard::Key key) {
    if (key==sf::Keyboard::Key::F5) { saveGame(); return; }
    if (key==sf::Keyboard::Key::F9) { loadGame(); return; }
    if (inventoryOpen_) {
        if (key==sf::Keyboard::Key::Escape) inventoryOpen_=false;
        else handleInventoryKey(key);
        return;
    }
    if(dungeonMenu_) { handleDungeonKey(key); return; }
    if(key==sf::Keyboard::Key::M) {
        dungeonMenu_=true; dungeonSelection_=dungeonIndex(currentFloor_); dungeonDepth_=(currentFloor_-1)%10+1; return;
    }
    if (key==sf::Keyboard::Key::Escape) { window_.close(); return; }
    if (key==sf::Keyboard::Key::B) { openInventory(); return; }
    if (key==sf::Keyboard::Key::D) {
        mode_=GameMode::Playing; resumeLevelUpSequence(); updateFieldOfView();
        log("Returned to the exact place you left on floor ",currentFloor_,"."); return;
    }
    if (key==sf::Keyboard::Key::R) {
        player_.stats().hp=player_.stats().maxHp; player_.stats().mana=player_.stats().maxMana;
        player_.talents().resetCooldowns(); log("Recovered HP, mana and ability cooldowns at the inn."); return;
    }
    if (key==sf::Keyboard::Key::Tab) { selling_=!selling_; shopSelection_=0; }
    const auto stock=rewardItemDefinitions();
    const std::size_t count=selling_?player_.inventory().items().size():stock.size();
    if (!count) { shopSelection_=0; return; }
    shopSelection_=std::min(shopSelection_,count-1);
    if (key==sf::Keyboard::Key::Up) shopSelection_=(shopSelection_+count-1)%count;
    if (key==sf::Keyboard::Key::Down) shopSelection_=(shopSelection_+1)%count;
    if (key!=sf::Keyboard::Key::Enter) return;
    if (selling_) {
        const auto& item=*player_.inventory().items()[shopSelection_];
        const int price=salePrice(item);
        if (!price) { log("Training gear has no sale value."); return; }
        if (gold_>100000000-price) { log("Gold limit reached."); return; }
        log("Sold ",item.name()," for ",price," gold.");
        auto sold=player_.inventory().take(shopSelection_); gold_+=price;
        shopSelection_=0;
    } else {
        if (player_.inventory().full()) { log("Bag full (50). Sell an item first."); return; }
        if (gold_<kShopPrice) { log("Not enough gold."); return; }
        if (nextItemId_==std::numeric_limits<std::uint64_t>::max()) return;
        auto item=std::make_unique<Item>(*stock[shopSelection_],nextItemId_++);
        log("Bought ",item->name()," for ",kShopPrice," gold.");
        gold_-=kShopPrice; player_.inventory().add(std::move(item));
    }
}

void Application::renderTown() {
    if(dungeonMenu_) { renderDungeonSelection(); return; }
    const auto mouse=mousePixel_?std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)):std::nullopt;
    const auto hovered=[&](const sf::FloatRect& r){ return !inventoryOpen_ && mouse && r.contains(*mouse); };
    ui_.panel(window_,{{0,0},{1280,720}},true,sf::Color(140,135,130));
    ui_.heading(window_,"Town",{40,16},34);
    ui_.text(window_,"Gold",{190,30},16,ui::kMuted); ui_.text(window_,std::to_string(gold_),{232,26},22,ui::kRare,ui::Font::Bold);
    const auto& stats=player_.stats();
    ui_.bar(window_,{{320,30},{210,20}},stats.maxHp?static_cast<float>(stats.hp)/stats.maxHp:0.f,sf::Color(176,38,34),
        "Life "+std::to_string(stats.hp)+" / "+std::to_string(stats.maxHp));
    ui_.button(window_,kTownInn,"Inn: rest (R)",hovered(kTownInn));
    ui_.button(window_,kTownEquipment,"Equipment (B)",hovered(kTownEquipment));
    ui_.button(window_,kTownDungeons,"Choose dungeon (M)",hovered(kTownDungeons));
    ui_.button(window_,kTownReturn,"Return to floor (D)",hovered(kTownReturn));

    // --- Merchant list ---------------------------------------------------------
    ui_.button(window_,kTownBuy,"Buy",hovered(kTownBuy) || !selling_);
    ui_.button(window_,kTownSell,"Sell",hovered(kTownSell) || selling_);
    const auto& active=selling_?kTownSell:kTownBuy;
    sf::RectangleShape underline({active.size.x-8,3}); underline.setPosition({active.position.x+4,active.position.y+active.size.y+1});
    underline.setFillColor(ui::kGold); window_.draw(underline);
    const auto& bag=player_.inventory().items();
    const auto stock=rewardItemDefinitions();
    const auto count=selling_?bag.size():stock.size();
    ui_.text(window_,selling_?"Your bag, "+std::to_string(bag.size())+" items":"The merchant's stock, "+std::to_string(kShopPrice)+" gold each",
        {362,102},15,ui::kMuted);
    if (count) shopSelection_=std::min(shopSelection_,count-1);
    const auto first=(shopSelection_/kTownRowsPerPage)*kTownRowsPerPage;
    if(!count) ui_.text(window_,selling_?"Your bag is empty.":"Nothing for sale.",{48,150},16,ui::kMuted);
    for (std::size_t i=first;i<count && i<first+kTownRowsPerPage;++i) {
        const auto row=townRow(static_cast<int>(i-first));
        const ItemDefinition& definition=selling_?*bag[i]->definition():*stock[i];
        const sf::Color nameColor=selling_ && bag[i]->rarity()==ItemRarity::Rare?ui::kRare:
            selling_ && bag[i]->rarity()==ItemRarity::Magic?ui::kMagic:ui::kText;
        ui_.inset(window_,row,i==shopSelection_?ui::kGold:hovered(row)?ui::kBronze:sf::Color::Transparent);
        ui_.icon(window_,itemIcon(definition),{{row.position.x+8,row.position.y+5},{28,28}},nameColor);
        ui_.text(window_,selling_?bag[i]->name():std::string(definition.name),{row.position.x+46,row.position.y+8},16,nameColor);
        const int price=selling_?salePrice(*bag[i]):kShopPrice;
        const std::string priceText=price?std::to_string(price)+" gold":"no value";
        ui_.text(window_,priceText,{row.position.x+row.size.x-14-ui_.textWidth(priceText,15,ui::Font::Bold),row.position.y+9},15,
            price?ui::kRare:ui::kMuted,ui::Font::Bold);
    }
    const auto pages=std::max<std::size_t>(1,(count+kTownRowsPerPage-1)/kTownRowsPerPage);
    const auto page=count?shopSelection_/kTownRowsPerPage:0;
    ui_.button(window_,kTownPrevious,"Previous",hovered(kTownPrevious),page>0);
    ui_.button(window_,kTownNext,"Next",hovered(kTownNext),page+1<pages);
    ui_.textCentered(window_,"Page "+std::to_string(page+1)+" of "+std::to_string(pages),
        {{kTownPrevious.position.x+kTownPrevious.size.x,kTownPrevious.position.y},{kTownNext.position.x-kTownPrevious.position.x-kTownPrevious.size.x,34}},15,ui::kMuted);

    // --- Preview of the selected item --------------------------------------------
    ui_.panel(window_,kTownPreview,false,sf::Color(130,125,125));
    const float left=kTownPreview.position.x+20;
    if (count) {
        const ItemDefinition& definition=selling_?*bag[shopSelection_]->definition():*stock[shopSelection_];
        const std::string name=selling_?bag[shopSelection_]->name():std::string(definition.name);
        const sf::Color nameColor=selling_ && bag[shopSelection_]->rarity()==ItemRarity::Rare?ui::kRare:
            selling_ && bag[shopSelection_]->rarity()==ItemRarity::Magic?ui::kMagic:ui::kText;
        const sf::FloatRect art{{left,kTownPreview.position.y+20},{88,88}};
        ui_.inset(window_,art,ui::kBronze);
        ui_.icon(window_,itemIcon(definition),{{art.position.x+12,art.position.y+12},{64,64}},nameColor);
        ui_.text(window_,name,{left+104,art.position.y+8},22,nameColor,ui::Font::Title);
        ui_.text(window_,equipmentTypeName(definition),{left+104,art.position.y+40},15,ui::kMuted);
        const auto bonus=selling_?bag[shopSelection_]->bonuses():definition.bonuses;
        const auto slot=player_.inventory().preferredSlot(definition);
        const auto* equipped=player_.inventory().equipped(slot);
        const auto old=equipped?equipped->bonuses():ItemBonuses{};
        float y=art.position.y+108;
        const auto statLine=[&](const char* label,int value,int previous) {
            if(!value && !previous) return;
            ui_.text(window_,(value>=0?"+":"")+std::to_string(value)+" "+label,{left,y},16,ui::kText);
            if(!selling_ && value!=previous) {
                const int delta=value-previous;
                ui_.text(window_,std::string("(")+(delta>0?"+":"")+std::to_string(delta)+" vs. worn)",{left+170,y},15,delta>0?ui::kGood:ui::kBad);
            }
            y+=24;
        };
        statLine("Strength",bonus.strength,old.strength); statLine("Dexterity",bonus.dexterity,old.dexterity);
        statLine("Intelligence",bonus.intelligence,old.intelligence); statLine("Max life",bonus.maxHp,old.maxHp);
        statLine("Max mana",bonus.maxMana,old.maxMana);
        y+=8;
        ui_.text(window_,std::string("Worn in ")+slotName(slot)+": "+(equipped?equipped->name():std::string("nothing")),{left,y},15,ui::kInfo);
        y+=30;
        ui_.paragraph(window_,"The merchant only stocks basic gear; stronger affixed items come from the dungeon. Buying costs more than selling earns.",
            left,y,kTownPreview.size.x-40,14,ui::kMuted);
        const bool canTrade=selling_?salePrice(*bag[shopSelection_])>0:(gold_>=kShopPrice && !player_.inventory().full());
        ui_.button(window_,kTownTrade,selling_?"Sell for "+std::to_string(salePrice(*bag[shopSelection_]))+" gold (Enter)":
            "Buy for "+std::to_string(kShopPrice)+" gold (Enter)",hovered(kTownTrade),canTrade,16);
        if(!selling_ && player_.inventory().full()) ui_.text(window_,"Your bag is full.",{kTownTrade.position.x+316,kTownTrade.position.y+12},15,ui::kBad);
        else if(!selling_ && gold_<kShopPrice) ui_.text(window_,"Not enough gold.",{kTownTrade.position.x+316,kTownTrade.position.y+12},15,ui::kBad);
    } else {
        ui_.textCentered(window_,selling_?"Nothing to sell.":"Nothing in stock.",kTownPreview,18,ui::kMuted);
    }
    ui_.text(window_,"Floors you leave pause. Returning resumes where you stood; nothing respawns.",{40,596},15,ui::kMuted);
    if (!logMessages_.empty()) ui_.text(window_,logMessages_.back(),{40,624},16,sf::Color(232,196,130));
    ui_.text(window_,"D resume   R inn   B equipment   M dungeons   Tab buy or sell   F5/F9 save or load",{40,684},14,ui::kMuted);
    if (inventoryOpen_) renderInventory();
}

void Application::handleDungeonKey(sf::Keyboard::Key key) {
    if(key==sf::Keyboard::Key::Escape || key==sf::Keyboard::Key::M) { dungeonMenu_=false; return; }
    if(key==sf::Keyboard::Key::Up || key==sf::Keyboard::Key::Down) dungeonSelection_=1-dungeonSelection_;
    if(key==sf::Keyboard::Key::Left) dungeonDepth_=std::max(1,dungeonDepth_-1);
    if(key==sf::Keyboard::Key::Right) dungeonDepth_=std::min(10,dungeonDepth_+1);
    if(key==sf::Keyboard::Key::Enter) travelFloor(dungeonSelection_*10+dungeonDepth_,true);
}

void Application::handleDungeonMouse(const sf::Event& event) {
    const auto* click=event.getIf<sf::Event::MouseButtonPressed>();
    if(!click || click->button!=sf::Mouse::Button::Left) return;
    const auto point=sf::Vector2f(click->position);
    if(kDungeonBack.contains(point)) { handleDungeonKey(sf::Keyboard::Key::Escape); return; }
    if(kDungeonEnter.contains(point)) { handleDungeonKey(sf::Keyboard::Key::Enter); return; }
    for(int i=0;i<2;++i) if(dungeonCard(i).contains(point)) { dungeonSelection_=i; return; }
    for(int depth=1;depth<=10;++depth) if(depthCard(depth).contains(point)) { dungeonDepth_=depth; return; }
}

void Application::renderDungeonSelection() {
    const auto mouse=mousePixel_?std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)):std::nullopt;
    const auto hovered=[&](const sf::FloatRect& r){ return mouse && r.contains(*mouse); };
    ui_.panel(window_,{{0,0},{1280,720}},true,sf::Color(140,135,130));
    ui_.heading(window_,"Choose your descent",{40,16},32);
    ui_.text(window_,"Pick a dungeon and a depth. Arrows also work; Enter descends, Esc returns to town.",{40,70},15,ui::kMuted);
    const char* descriptions[]{"Barracks, a ruined sanctum and the crypts. The Goblin Warlord waits at depth 5, the Lich at depth 10.",
                               "Undead legions beyond the broken seal. The final Lich waits at depth 10."};
    const char* icons[]{"relic-blade","skull-crossed-bones"};
    for(int i=0;i<2;++i) {
        const auto r=dungeonCard(i);
        const bool active=i==dungeonSelection_;
        ui_.inset(window_,r,active?ui::kGold:hovered(r)?ui::kBronze:sf::Color::Transparent);
        const sf::FloatRect art{{r.position.x+16,r.position.y+16},{118,118}};
        ui_.inset(window_,art);
        ui_.icon(window_,icons[i],{{art.position.x+16,art.position.y+16},{86,86}},active?ui::kGold:ui::kMuted);
        ui_.text(window_,dungeonName(i),{r.position.x+152,r.position.y+16},26,active?ui::kGold:ui::kText,ui::Font::Title);
        ui_.text(window_,"Levels "+std::to_string(i*10+1)+" to "+std::to_string(dungeonMaximum(i)),{r.position.x+152,r.position.y+52},16,ui::kText,ui::Font::Bold);
        float y=r.position.y+80;
        ui_.paragraph(window_,descriptions[i],r.position.x+152,y,r.size.x-170,15,ui::kMuted);
    }
    ui_.text(window_,"Depth",{40,272},20,ui::kGold,ui::Font::Title);
    for(int depth=1;depth<=10;++depth) {
        const auto r=depthCard(depth);
        const int floor=dungeonSelection_*10+depth;
        const bool visited=floor==currentFloor_ || floorCache_.count(floor);
        ui_.inset(window_,r,depth==dungeonDepth_?ui::kGold:hovered(r)?ui::kBronze:sf::Color::Transparent);
        ui_.textCentered(window_,std::to_string(depth),{{r.position.x,r.position.y+6},{r.size.x,44}},32,
            depth==dungeonDepth_?ui::kGold:ui::kText,ui::Font::Title);
        if(depth==5 || depth==10)
            ui_.icon(window_,"skull-crossed-bones",{{r.position.x+r.size.x-24,r.position.y+6},{18,18}},sf::Color(200,70,60));
        if(visited) ui_.textCentered(window_,floor==currentFloor_?"you are here":"visited",{{r.position.x,r.position.y+52},{r.size.x,20}},13,ui::kGood);
    }
    const int destination=dungeonSelection_*10+dungeonDepth_;
    const bool visited=destination==currentFloor_ || floorCache_.count(destination);
    float y=410;
    ui_.text(window_,std::string(dungeonName(dungeonSelection_))+", depth "+std::to_string(dungeonDepth_),{40,y},22,ui::kGold,ui::Font::Title);
    y+=36;
    ui_.text(window_,"Your level "+std::to_string(player_.level())+", depth level "+std::to_string(destination),{40,y},17,
        player_.level()<destination?ui::kBad:ui::kText,ui::Font::Bold);
    y+=28;
    ui_.paragraph(window_,visited?"Visited: enemies and loot stay exactly as you left them. Nothing respawns.":
        "A new floor, generated once. Entering doesn't heal you or take a turn.",40,y,1180,16,ui::kText);
    y+=6;
    ui_.paragraph(window_,"Entry is never restricted, so deep floors can be deadly. Difficulty depends only on depth. "
        "Returning to your current floor puts you back where you stood; other visited floors start at their entrance.",40,y,1180,15,ui::kMuted);
    ui_.button(window_,kDungeonEnter,"Descend (Enter)",hovered(kDungeonEnter),true,17);
    ui_.button(window_,kDungeonBack,"Back to town (Esc)",hovered(kDungeonBack),true,17);
    if(!logMessages_.empty()) ui_.text(window_,logMessages_.back(),{600,616},15,sf::Color(232,196,130));
}

void Application::handleTravelKey(sf::Keyboard::Key key) {
    if(!exitMenu_) return;
    if(key==sf::Keyboard::Key::Escape) exitMenu_=false;
    else if(key==sf::Keyboard::Key::Num1) { exitMenu_=false; travelFloor(currentFloor_+1); }
    else if(key==sf::Keyboard::Key::Num2) { exitMenu_=false; returnToTown(); }
    else if(key==sf::Keyboard::Key::F5) saveGame();
    else if(key==sf::Keyboard::Key::F9) loadGame();
}

void Application::handleTravelMouse(const sf::Event& event) {
    const auto* click=event.getIf<sf::Event::MouseButtonPressed>();
    if(!click || click->button!=sf::Mouse::Button::Left) return;
    if(contains(kTravelDown,click->position)) handleTravelKey(sf::Keyboard::Key::Num1);
    else if(contains(kTravelTown,click->position)) handleTravelKey(sf::Keyboard::Key::Num2);
    else if(contains(kTravelStay,click->position)) handleTravelKey(sf::Keyboard::Key::Escape);
}

void Application::renderTravel() {
    const auto p=floorEntrance_;
    const auto at=worldToScreen(p.x,p.y);
    if (exploredMap_.at(p.x,p.y)==Visibility::Visible && onMap(at))
        ui_.icon(window_,"jump-across",{{at.x+3,at.y+3},{22,22}},ui::kInfo);
    if (sameTile(player_.position(),floorEntrance_))
        mapHints_.push_back({currentFloor_==1?"Entrance. G: return to town (needs 10 quiet turns)":"Stairs up. G: ascend to the previous floor",ui::kInfo});
    if (!exitMenu_) return;
    const auto mouse=mousePixel_?std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)):std::nullopt;
    const auto hovered=[&](const sf::FloatRect& r){ return mouse && r.contains(*mouse); };
    sf::RectangleShape dim({1280,720}); dim.setFillColor(sf::Color(0,0,0,120)); window_.draw(dim);
    ui_.panel(window_,kTravelDialog,true,sf::Color(150,145,140));
    ui_.textCentered(window_,"Stairs down",{{kTravelDialog.position.x,kTravelDialog.position.y+22},{kTravelDialog.size.x,44}},32,ui::kGold,ui::Font::Title);
    ui_.textCentered(window_,"Floors persist, so you can come back for loot. Travel doesn't heal you.",
        {{kTravelDialog.position.x,kTravelDialog.position.y+64},{kTravelDialog.size.x,22}},15,ui::kMuted);
    ui_.button(window_,kTravelDown,"1: Descend to depth "+std::to_string(currentFloor_+1),hovered(kTravelDown),true,17);
    ui_.button(window_,kTravelTown,"2: Return to town (needs 10 quiet turns)",hovered(kTravelTown),true,17);
    ui_.button(window_,kTravelStay,"Esc: Stay here",hovered(kTravelStay),true,17);
}
} // namespace engine
