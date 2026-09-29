#include "core/Application.hpp"
#include "entities/RunProgression.hpp"

#include <algorithm>
#include <limits>
#include <queue>
#include <random>
#include "core/PlayLayout.hpp"
#include "world/FloorTheme.hpp"

namespace engine {
namespace {
bool sameTile(Position a,Position b) { return a.x==b.x && a.y==b.y; }
bool training(const Item& item) { return std::string_view(item.definition()->id).find("training_")==0; }
int salePrice(const Item& item) { return training(item)?0:5+10*static_cast<int>(item.rarity())+2*item.rollTier(); }
constexpr int kShopPrice=30;
const sf::FloatRect kTravelDown{{195,285},{245,44}}, kTravelTown{{455,285},{250,44}}, kTravelStay{{720,285},{240,44}};
const sf::FloatRect kTownBuy{{40,135},{170,38}}, kTownSell{{220,135},{170,38}},
    kTownTrade{{680,525},{265,48}}, kTownPrevious{{40,525},{155,40}}, kTownNext{{215,525},{155,40}},
    kTownInn{{770,65},{150,38}}, kTownInventory{{930,65},{135,38}}, kTownReturn{{1080,65},{155,38}};
const sf::FloatRect kTownCodex{{410,135},{220,38}};
const sf::FloatRect kTownDungeons{{650,135},{260,38}}, kDungeonEnter{{40,545},{260,45}}, kDungeonBack{{320,545},{240,45}};
sf::FloatRect dungeonCard(int index) { return {{40+610.f*index,125},{550,90}}; }
sf::FloatRect dungeonDepthCard(int depth) { return {{40+120.f*((depth-1)%5),375+60.f*((depth-1)/5)},{110,45}}; }
const sf::FloatRect kTownRows[]{{{35,220},{600,30}},{{35,252},{600,30}},{{35,284},{600,30}},
    {{35,316},{600,30}},{{35,348},{600,30}},{{35,380},{600,30}},{{35,412},{600,30}},
    {{35,444},{600,30}},{{35,476},{600,30}}};
bool contains(const sf::FloatRect& rect,sf::Vector2i p) { return rect.contains(sf::Vector2f(p)); }
void shopButton(sf::RenderWindow& window,const sf::FloatRect& rect,const std::string& label,bool active=false,bool enabled=true) {
    sf::RectangleShape shape(rect.size); shape.setPosition(rect.position);
    shape.setFillColor(!enabled?sf::Color(25,29,37):active?sf::Color(39,76,78):sf::Color(30,43,56));
    shape.setOutlineThickness(1); shape.setOutlineColor(active?sf::Color(110,225,210):sf::Color(95,115,140)); window.draw(shape);
}
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
        const auto first=(shopSelection_/9)*9;
        for(std::size_t row=0;row<9 && first+row<count;++row)
            if(contains(kTownRows[row],moved->position)) { shopSelection_=first+row; break; }
    }
    if(const auto* wheel=event.getIf<sf::Event::MouseWheelScrolled>()) {
        if(wheel->position.x>650) return;
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
    if(contains(kTownCodex,p)) { cancelTargeting(); codexOpen_=true; return; }
    if(contains(kTownInn,p)) { handleTownKey(sf::Keyboard::Key::R); return; }
    if(contains(kTownInventory,p)) { openInventory(); return; }
    if(contains(kTownReturn,p)) { handleTownKey(sf::Keyboard::Key::D); return; }
    if(contains(kTownTrade,p)) { handleTownKey(sf::Keyboard::Key::Enter); return; }
    const auto stock=rewardItemDefinitions();
    const auto count=selling_?player_.inventory().items().size():stock.size();
    const auto first=(shopSelection_/9)*9;
    if(contains(kTownPrevious,p)) { shopSelection_=first>=9?first-9:0; return; }
    if(contains(kTownNext,p)) { shopSelection_=std::min(first+9,count?count-1:0); return; }
    for(std::size_t row=0;row<9;++row) if(contains(kTownRows[row],p) && first+row<count) {
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
    cancelTargeting(); inventoryOpen_=false; vaultMenu_=0; exitMenu_=false; restTurns_=0;
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
    const auto button=[&](const sf::FloatRect& rect,const std::string& label,bool active=false,bool enabled=true) {
        shopButton(window_,rect,label,active,enabled);
        drawText(label,rect.position.x+8,rect.position.y+8,15,enabled?sf::Color(225,232,242):sf::Color(130,135,145));
    };
    drawText("TOWN - provisional hub",40,25,28,sf::Color(130,225,215));
    drawText("D: resume   R: inn   B: equipment   J: Codex   F5/F9: save/load",40,70,16,sf::Color::White);
    drawText("Gold: "+std::to_string(gold_)+"   HP: "+std::to_string(player_.stats().hp)+"/"+std::to_string(player_.stats().maxHp),40,105,19,sf::Color(255,220,130));
    drawText("Select an item to preview, then click the trade button. Existing keyboard controls still work.",40,190,15,sf::Color(185,195,210));
    button(kTownBuy,"Buy",!selling_); button(kTownSell,"Sell",selling_);
    button(kTownCodex,"Codex / books [J]");
    button(kTownDungeons,"Choose dungeon [M]");
    button(kTownInn,"Inn: recover"); button(kTownInventory,"Equipment"); button(kTownReturn,"Return to floor");
    const auto& bag=player_.inventory().items();
    const auto stock=rewardItemDefinitions();
    const auto count=selling_?bag.size():stock.size();
    if (count) shopSelection_=std::min(shopSelection_,count-1);
    const auto first=(shopSelection_/9)*9;
    for (std::size_t i=first;i<count && i<first+9;++i) {
        const auto name=selling_?bag[i]->name():std::string(stock[i]->name);
        const int price=selling_?salePrice(*bag[i]):kShopPrice;
        sf::RectangleShape row({600,30}); row.setPosition({35,220+32.f*(i-first)});
        row.setFillColor(i==shopSelection_?sf::Color(42,59,73):sf::Color(21,30,43));
        row.setOutlineThickness(mousePixel_ && kTownRows[i-first].contains(sf::Vector2f(*mousePixel_))?1.f:0.f);
        row.setOutlineColor(sf::Color(110,225,210)); window_.draw(row);
        drawText((i==shopSelection_?"> ":"  ")+name+" - "+std::to_string(price)+" gold",43,225+32.f*(i-first),16,
            i==shopSelection_?sf::Color(255,220,100):sf::Color::White);
    }
    const auto pages=std::max<std::size_t>(1,(count+8)/9);
    const auto page=count?shopSelection_/9:0;
    button(kTownPrevious,"Previous",false,page>0);
    button(kTownNext,"Next",false,page+1<pages);
    button(kTownTrade,selling_?"Sell selected item":"Buy selected item",false,count>0 && (selling_?salePrice(*bag[shopSelection_])>0:(gold_>=kShopPrice && !player_.inventory().full())));
    drawText("Page "+std::to_string(page+1)+" / "+std::to_string(pages),405,535,15,sf::Color(190,200,215));
    if (count) {
        const auto bonus=selling_?bag[shopSelection_]->bonuses():stock[shopSelection_]->bonuses;
        const auto slot=player_.inventory().preferredSlot(selling_?*bag[shopSelection_]->definition():*stock[shopSelection_]);
        const auto* equipped=player_.inventory().equipped(slot);
        drawText("Type: "+equipmentTypeName(selling_?*bag[shopSelection_]->definition():*stock[shopSelection_]),680,230,18,sf::Color(140,230,215));
        drawText("STR +"+std::to_string(bonus.strength)+"  DEX +"+std::to_string(bonus.dexterity)+"  INT +"+std::to_string(bonus.intelligence),680,270,17,sf::Color::White);
        drawText("Max HP +"+std::to_string(bonus.maxHp)+"  Max mana +"+std::to_string(bonus.maxMana),680,305,17,sf::Color::White);
        float y=350;
        drawWrapped("Equipped: "+(equipped?equipped->name():std::string("nothing")),680,y,65,sf::Color(185,195,210),460);
        drawWrapped("Stock is basic gear. Find stronger affixed items in the dungeon. Buying costs more than resale; returning never refreshes dungeon loot.",680,y,65,sf::Color(185,195,210),500);
    }
    drawText("Inactive floors pause. Returning resumes your saved location; nothing respawns.",40,585,16,sf::Color(140,230,215));
    if (!logMessages_.empty()) { float y=610; drawWrapped(logMessages_.back(),40,y,145,sf::Color(255,220,140),700); }
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
    for(int depth=1;depth<=10;++depth) if(dungeonDepthCard(depth).contains(point)) { dungeonDepth_=depth; return; }
}

void Application::renderDungeonSelection() {
    const sf::Color text(220,230,240), accent(110,225,210), selected(255,220,110);
    const auto box=[&](sf::FloatRect rect,bool active) {
        sf::RectangleShape shape(rect.size); shape.setPosition(rect.position);
        shape.setFillColor(active?sf::Color(42,64,77):sf::Color(24,35,49));
        shape.setOutlineThickness(1); shape.setOutlineColor(active?selected:accent); window_.draw(shape);
    };
    drawText("CHOOSE YOUR DESCENT",40,25,27,accent);
    drawText("Click a dungeon and depth, then Enter. Arrows: dungeon/depth. Esc: town. F5/F9: save/load.",40,75,16,text);
    for(int i=0;i<2;++i) {
        const auto rect=dungeonCard(i); box(rect,i==dungeonSelection_);
        drawText(std::string(dungeonName(i))+" | levels "+std::to_string(i*10+1)+"-"+std::to_string(dungeonMaximum(i)),rect.position.x+15,140,21,accent);
        drawText("Difficulty follows depth, independent of your level",rect.position.x+15,177,15,text);
    }
    const int destination=dungeonSelection_*10+dungeonDepth_;
    const int suggested=destination;
    const bool visited=destination==currentFloor_ || floorCache_.count(destination);
    drawText("Your level: "+std::to_string(player_.level())+" | Depth level: "+std::to_string(suggested),40,250,20,player_.level()<suggested?selected:text);
    drawText("Deeper floors add tougher encounters and better rewards.",40,287,16,text);
    drawText(dungeonSelection_?"Undead legions. Final Lich at depth 10.":"Barracks, sanctum and crypts. Warlord at depth 5; Lich at depth 10.",40,320,16,accent);
    for(int depth=1;depth<=10;++depth) {
        const auto rect=dungeonDepthCard(depth); box(rect,depth==dungeonDepth_);
        const int floor=dungeonSelection_*10+depth;
        drawText(std::string(floor==currentFloor_ || floorCache_.count(floor)?"* ":"  ")+"Depth "+std::to_string(depth),rect.position.x+5,rect.position.y+12,14,text);
    }
    drawText(visited?"* Visited: enemies and loot stay as you left them. No respawns.":"New floor: generated once. Entering does not heal you or advance a turn.",40,505,16,text);
    box(kDungeonEnter,false); box(kDungeonBack,false);
    drawText("Enter selected depth",50,558,17,text); drawText("Back to town [Esc]",330,558,17,text);
    float y=620;
    drawWrapped("Entry is unrestricted: higher depths can be deadly. Difficulty depends only on depth. Returning to the current floor resumes your exact position; other visited floors arrive at their entrance.",40,y,140,selected,690);
    if(!logMessages_.empty()) drawText(logMessages_.back().substr(0,135),40,694,12,text);
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
    if (exploredMap_.at(p.x,p.y)==Visibility::Visible && at.x>=0 && at.x<playLayout::mapWidth &&
        at.y>=playLayout::mapTop && at.y<playLayout::mapTop+playLayout::mapHeight)
        drawText("<",at.x+5,at.y+1,22,sf::Color(120,205,255));
    drawText("H: "+std::to_string(dangerNearby()?0:quietTurns_)+"/10",1150,12,12,sf::Color(160,220,240));
    if (sameTile(player_.position(),floorEntrance_))
        drawText(currentFloor_==1?"G: return to town (10 quiet turns)":"G: ascend to the previous floor",590,49,12,sf::Color(130,220,255));
    if (!exitMenu_) return;
    sf::RectangleShape box({850.f,240.f}); box.setPosition({160,210}); box.setFillColor(sf::Color(15,20,30,250)); window_.draw(box);
    drawText("DESCENT",195,235,24,sf::Color(255,215,120));
    const auto button=[&](const sf::FloatRect& rect,const std::string& label) {
        const bool hover=mousePixel_ && contains(rect,*mousePixel_);
        shopButton(window_,rect,label,hover);
        drawText(label,rect.position.x+10,rect.position.y+11,15,sf::Color::White);
    };
    button(kTravelDown,"1: Descend to floor "+std::to_string(currentFloor_+1));
    button(kTravelTown,"2: Return to town");
    button(kTravelStay,"Esc: Stay here");
    drawText("Town requires 10 quiet turns. Stairs require no current danger.",195,340,17,sf::Color(190,210,230));
    drawText("Floors persist; you can come back for loot. Travel does not heal you.",195,380,17,sf::Color(190,210,230));
}
} // namespace engine
