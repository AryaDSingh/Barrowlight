#include "core/Application.hpp"
#include "entities/HiddenTrees.hpp"

namespace engine {
namespace {
constexpr const char* kCodexPath="codex.txt";
const sf::FloatRect kClose{{1100,25},{145,38}}, kSave{{840,25},{245,38}},
    kPrevious{{40,440},{145,38}}, kNext{{195,440},{145,38}}, kConsult{{40,590},{310,40}};
sf::FloatRect codexPageRect(int page) { return {{35,172+60.f*page},{315,56}}; }
}
void Application::handleCodexMouse(const sf::Event& event) {
    if(const auto* wheel=event.getIf<sf::Event::MouseWheelScrolled>(); wheel && wheel->delta!=0) {
        handleCodexKey(wheel->delta>0?sf::Keyboard::Key::Up:sf::Keyboard::Key::Down);
    }
    const auto* click=event.getIf<sf::Event::MouseButtonPressed>();
    if(!click || click->button!=sf::Mouse::Button::Left) return;
    const auto point=sf::Vector2f(click->position);
    if(kClose.contains(point)) { handleCodexKey(sf::Keyboard::Key::Escape); return; }
    if(kSave.contains(point)) { handleCodexKey(sf::Keyboard::Key::S); return; }
    if(kPrevious.contains(point)) { handleCodexKey(sf::Keyboard::Key::Up); return; }
    if(kNext.contains(point)) { handleCodexKey(sf::Keyboard::Key::Down); return; }
    if(kConsult.contains(point)) { handleCodexKey(sf::Keyboard::Key::L); return; }
    for(int i=0;i<4;++i) if(codexPageRect(i).contains(point)) { codexSelection_=i; return; }
}
void Application::discoverLore(const char* id) {
    if (!codex_.discover(id)) return;
    const auto* lore=findLore(id);
    log("Lore found: ",lore->title,". J: read your Codex.");
    if (!codex_.save(kCodexPath)) log(codex_.error());
}
void Application::discoverMonsterLore(const Monster& monster) {
    if (!monster.rewardsEligible()) return;
    if (monster.type()==MonsterType::GoblinWarlord || (monster.type()==MonsterType::Shaman && !player_.bloodRelic && rollChance(.20f))) {
        if (!player_.bloodRelic) log("Relic acquired: Blood Testament. Kept in your run's Codex, no bag space required.");
        player_.bloodRelic=true; discoverLore("shaman.blood");
    }
    if (monster.type()==MonsterType::Lich) {
        if (!player_.animationRelic) log("Rare relic acquired: Ossuary Seal. Specialize Arcane to discover Animation.");
        player_.animationRelic=true;
    }
    refreshHiddenDiscoveries();
    switch (monster.type()) {
    case MonsterType::GoblinWarlord: discoverLore("warlord.ember"); break;
    case MonsterType::Lich: discoverLore("lich.grave"); break;
    case MonsterType::Skeleton: discoverLore("skeleton.grave"); break;
    case MonsterType::Archer: discoverLore("archer.shadow"); break;
    default: break;
    }
}
void Application::handleCodexKey(sf::Keyboard::Key key) {
    if (key==sf::Keyboard::Key::J || key==sf::Keyboard::Key::Escape) { codexOpen_=false; return; }
    if (key==sf::Keyboard::Key::Up) codexSelection_=(codexSelection_+3)%4;
    if (key==sf::Keyboard::Key::Down) codexSelection_=(codexSelection_+1)%4;
    if (key==sf::Keyboard::Key::S) codex_.save(kCodexPath);
    if (key==sf::Keyboard::Key::L && mode_==GameMode::Town) {
        constexpr const char* rumours[]{"town.ember","town.grave","town.shadow","town.blood"};
        discoverLore(rumours[codexSelection_]);
    }
}
void Application::renderCodex() {
    const sf::Color text(220,228,240), accent(110,225,210), dim(155,165,185), selected(255,220,110);
    drawText("CODEX - collected lore",40,25,28,accent);
    drawText("Click a page or use wheel/Up/Down. J/Esc: return. S: save knowledge.",40,70,17,text);
    drawText("Knowledge survives death. Reading takes no turns and grants no character bonuses.",40,105,16,dim);
    const auto button=[&](const sf::FloatRect& rect,const std::string& label,bool enabled=true) {
        sf::RectangleShape box(rect.size); box.setPosition(rect.position);
        const bool hover=mousePixel_ && rect.contains(sf::Vector2f(*mousePixel_));
        box.setFillColor(enabled?(hover?sf::Color(44,68,80):sf::Color(27,41,55)):sf::Color(27,30,38));
        box.setOutlineThickness(1); box.setOutlineColor(enabled?accent:dim); window_.draw(box);
        drawText(label,rect.position.x+8,rect.position.y+9,15,enabled?text:dim);
    };
    button(kClose,"Close [J/Esc]"); button(kSave,"Save knowledge [S]");
    button(kPrevious,"Previous page"); button(kNext,"Next page");
    for (int i=0;i<4;++i) {
        const int count=codex_.count(i);
        const auto rect=codexPageRect(i);
        sf::RectangleShape box(rect.size); box.setPosition(rect.position);
        box.setFillColor(i==codexSelection_?sf::Color(40,56,70):sf::Color(22,30,42));
        box.setOutlineThickness(1); box.setOutlineColor(i==codexSelection_?selected:dim); window_.draw(box);
        drawText(std::string(i==codexSelection_?"> ":"  ")+(codex_.revealed(kHiddenIds[i])?kHiddenNames[i]:"???"),40,180+60.f*i,20,i==codexSelection_?selected:text);
        drawText(std::string(codex_.revealed(kHiddenIds[i])?"Revealed":count?"Hinted":"Unknown")+" | "+std::to_string(count)+" fragments",65,207+60.f*i,14,dim);
    }
    float y=150;
    if (codex_.revealed(kHiddenIds[codexSelection_])) {
        drawWrapped(kHiddenConditions[codexSelection_],370,y,88,accent,560);
        drawWrapped(hiddenTreeAvailable(player_,kHiddenIds[codexSelection_])?"Requirements met this run. T: purchase with a tree point.":"This character must earn the requirements again.",370,y,88,selected,560);
        y+=12;
    }
    if (!codex_.count(codexSelection_))
        drawWrapped("An unwritten page. Explore the dungeon and consult books and rumours in town to find fragments.",370,y,88,dim,560);
    for (const auto& lore:kLoreFragments) {
        if (lore.family!=codexSelection_ || !codex_.knows(lore.id)) continue;
        drawWrapped(std::string(lore.title)+" - "+lore.source,370,y,88,selected,560);
        drawWrapped(lore.text,370,y,88,text,560); y+=18;
    }
    drawText(std::string("Run relics: ")+(player_.bloodRelic?"Blood Testament  ":"")+(player_.animationRelic?"Ossuary Seal":""),40,555,15,dim);
    button(kConsult,"Consult town book/rumour [L]",mode_==GameMode::Town);
    y=592;
    drawWrapped(mode_==GameMode::Town ? "Consulting this page is free. Each source grants its fragment once." :
        "Return to town to consult its books and rumours.",375,y,100,accent,635);
    if (!codex_.error().empty()) { y=645; drawWrapped(codex_.error(),40,y,135,selected,710); }
    else drawText("Lore is saved separately from your current adventure.",40,655,15,dim);
}
} // namespace engine
