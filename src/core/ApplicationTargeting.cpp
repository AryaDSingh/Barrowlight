#include "core/Application.hpp"
#include "entities/Ascendancy.hpp"

#include <algorithm>
#include <cmath>
#include "core/GameIcons.hpp"
#include "core/PlayLayout.hpp"
#include "entities/MonsterInspection.hpp"
#include "entities/PlayerLeveling.hpp"
#include "entities/RunProgression.hpp"
#include "world/FloorTheme.hpp"
#include "entities/TalentEffects.hpp"
#include "entities/HiddenCombat.hpp"
#include "world/LineOfFire.hpp"

namespace engine {
namespace {
constexpr int kMapLeft = playLayout::mapLeft, kMapWidth = playLayout::mapWidth, kMapTop = playLayout::mapTop,
    kMapHeight = playLayout::mapHeight, kTile = playLayout::tileSize;
constexpr std::size_t kPageSize = 9;
struct DungeonAction { const char* name; const char* key; const char* icon; };
constexpr DungeonAction kDungeonActions[]{
    {"Inventory","B","knapsack"},{"Talent trees","T","tree-branch"},
    {"Use / pick up","G","locked-chest"},{"Wait a turn","Space","hourglass"},{"Rest","R","campfire"},
    {"Auto-explore","Z","compass"},{"Return to town","H","village"},{"Cleanse","C","aura"},
    {"Save game","F5","scroll-unfurled"},{"Load game","F9","spell-book"},{"Inspect enemies","I","third-eye"},
    {"Torch","L","burning-embers"}};
constexpr std::size_t kDungeonActionCount = std::size(kDungeonActions);
sf::FloatRect dungeonActionRect(std::size_t index) {
    using namespace playLayout;
    return {{actionX+actionStrideX*(index%actionColumns), actionY+actionStrideY*(index/actionColumns)},{actionWidth,actionHeight}};
}
// The mode banner (aiming, inspecting, resting, exploring) runs along the
// bottom of the map, with its Cancel/Stop button at the right end.
const sf::FloatRect kModeBanner{{kMapLeft+154.f,kMapTop+kMapHeight-42.f},{700,34}};
const sf::FloatRect kCancelAction{{kModeBanner.position.x+kModeBanner.size.x-96,kModeBanner.position.y+5},{88,24}};
sf::FloatRect pageButton(int dir) {
    using namespace playLayout;
    return {{pageButtonX+(pageButtonWidth+4)*dir,hotbarY},{pageButtonWidth,pageButtonHeight}};
}
sf::FloatRect hotbarRect(std::size_t slot) {
    using namespace playLayout;
    return {{hotbarX+hotbarStride*slot,hotbarY},{hotbarWidth,hotbarHeight}};
}
sf::FloatRect statusRect(int slot) {
    using namespace playLayout;
    return {{statusX+statusStride*(slot%statusColumns),statusY+statusStride*(slot/statusColumns)},{statusSize,statusSize}};
}
constexpr int kStatusPerPage = playLayout::statusColumns*playLayout::statusRows;
const sf::FloatRect kLogArea{{playLayout::logX,playLayout::logY},
    {playLayout::logWidth,playLayout::logLineHeight*playLayout::logLines}};
const char* className(PlayerClass c) {
    switch(c) { case PlayerClass::Warrior: return "Warrior"; case PlayerClass::Thief: return "Thief";
        case PlayerClass::Mage: return "Mage"; case PlayerClass::Spellblade: return "Spellblade"; }
    return "";
}
// Clickable parts of the character column.
const sf::FloatRect kPortraitArea{{14,14},{64,64}}, kNameArea{{84,14},{160,66}}, kXpArea{{14,136},{228,32}};
const sf::FloatRect kLevelBadge{{160,40},{84,22}};
const sf::FloatRect kMinimapArea{{18,playLayout::minimapY+4},{220,142}};
bool harmfulStatus(StatusEffectType t) {
    return isCleansable(t) || t==StatusEffectType::Stun || t==StatusEffectType::Wither || t==StatusEffectType::Shock;
}
std::vector<StatusEffectInstance> hudEffects(const Player& player) {
    std::vector<StatusEffectInstance> effects;
    for (const auto& e:player.statusEffects().active()) if(e.type!=StatusEffectType::UnseenReady) effects.push_back(e);
    return effects;
}
std::string statusTooltip(const StatusEffectInstance& e) {
    const auto n=std::to_string(e.magnitude);
    switch(e.type) {
    case StatusEffectType::Poison: case StatusEffectType::Burn: return "Lose "+n+" HP each status tick. Bypasses Guard and reveals concealment.";
    case StatusEffectType::Smothered: return "Pitch black: your torch or lantern can't burn and Conjure Light fails. Your light returns when this ends. Fire spells can still light braziers and oil.";
    case StatusEffectType::Doom: return "Lose "+n+" HP when the countdown expires. Guard and dodge do not prevent this. Cleanse before the last tick, or heal to prepare for it.";
    case StatusEffectType::ManaDrain: return "Lose "+n+" mana each status tick, down to zero. Combat mana recovery remains 1 per turn.";
    case StatusEffectType::Stun: return "Lose actions until this expires, then gain brief stun immunity. Cleanse does not remove Stun.";
    case StatusEffectType::StunRecovery: return "Temporarily immune to another Stun. Cannot be cleansed.";
    case StatusEffectType::Empowered: return "Adds "+n+" damage to direct attacks.";
    case StatusEffectType::Guard: return "Blocks up to "+n+" damage from each direct hit. Does not block damage over time or Doom.";
    case StatusEffectType::Evasion: return "Adds "+n+" percentage points of dodge. Total dodge is capped at 60%.";
    case StatusEffectType::Chill: return "Reduces damage dealt by "+n+"%. Chilled enemies also skip movement on alternating turns.";
    case StatusEffectType::Shock: return "Enables Lightning follow-ups. Certain talents consume Shock for an additional effect.";
    case StatusEffectType::Concealed: return "Enemies roll detection using distance, Dexterity and concealment rank. Most attacks and taking damage reveal you.";
    case StatusEffectType::Opening: return "A brief opportunity from waiting or movement talents. Enables bonuses from compatible bow and armour talents.";
    case StatusEffectType::Marked: return "The next direct hit deals 25% more damage, then consumes the mark.";
    case StatusEffectType::BattleRhythm: return "Your next melee attack gains "+n+" damage. The bonus is ready until used.";
    case StatusEffectType::BloodPact: return "Spells spend HP instead of mana while active. You cannot spend your last HP.";
    case StatusEffectType::Wither: return "Direct hits on this target heal their attacker by up to "+n+" HP, capped by actual HP removed.";
    case StatusEffectType::HuntersMark: return "Halves eligible stealth detection chance (minimum 5%). Also applied with a separate damage mark.";
    case StatusEffectType::FlameBlade: case StatusEffectType::FrostBlade: case StatusEffectType::StormBlade: case StatusEffectType::ArcaneBlade:
        return "Melee imbue: "+n+" landed hits remain. Applies its elemental effect on melee hits; ends when hits or duration run out.";
    default: return "An active talent effect. Its remaining duration is shown on the status badge.";
    }
}
bool same(Position a, Position b) { return a.x == b.x && a.y == b.y; }
}

std::optional<Position> Application::screenToWorld(sf::Vector2i pixel) const {
    // Mouse positions are already in the 1280x720 layout (handleEvent converts them).
    const auto p = sf::Vector2f(pixel);
    if (!onMap(p)) return std::nullopt;
    const auto shift = cameraShift();
    const Position tile{cameraX_ + static_cast<int>(std::floor((p.x - shift.x - kMapLeft) / kTile)),
        cameraY_ + static_cast<int>(std::floor((p.y - shift.y - kMapTop) / kTile))};
    if (!map_.inBounds(tile.x, tile.y)) return std::nullopt;
    return tile;
}

bool Application::onMap(sf::Vector2f p) const {
    return p.x >= kMapLeft && p.x < kMapLeft + kMapWidth && p.y >= kMapTop && p.y < kMapTop + kMapHeight;
}

std::optional<std::size_t> Application::talentAtPixel(sf::Vector2i pixel) const {
    const auto p = sf::Vector2f(pixel);
    for (std::size_t slot = 0; slot < kPageSize; ++slot)
        if (hotbarRect(slot).contains(p)) return player_.talents().hotbarIndex(talentPage_*kPageSize+slot);
    return std::nullopt;
}

std::optional<StatusEffectInstance> Application::hoveredStatus() const {
    if (!mousePixel_) return std::nullopt;
    const auto p=sf::Vector2f(*mousePixel_);
    const auto effects=hudEffects(player_);
    const int page=std::min(statusPage_,std::max(0,(static_cast<int>(effects.size())-1)/kStatusPerPage));
    for (int slot=0;slot<kStatusPerPage;++slot) {
        const int index=page*kStatusPerPage+slot;
        if (index<static_cast<int>(effects.size()) && statusRect(slot).contains(p)) return effects[index];
    }
    return std::nullopt;
}

std::optional<std::size_t> Application::hoveredLogLine() const {
    if (!mousePixel_ || logMessages_.empty()) return std::nullopt;
    const auto p=sf::Vector2f(*mousePixel_);
    if (!kLogArea.contains(p)) return std::nullopt;
    const int shown=std::min<int>(playLayout::logLines,static_cast<int>(logMessages_.size()));
    const int first=std::max(0,static_cast<int>(logMessages_.size())-shown-combatLogScroll_);
    const int row=static_cast<int>((p.y-playLayout::logY)/playLayout::logLineHeight);
    if (row>=shown) return std::nullopt;
    return static_cast<std::size_t>(first+row);
}

// The map tile under a point on the minimap (same fit as renderMinimap).
std::optional<Position> Application::minimapTile(sf::Vector2f p) const {
    if (!kMinimapArea.contains(p) || map_.width()<=0 || map_.height()<=0) return std::nullopt;
    const float scale=std::floor(std::min(kMinimapArea.size.x/map_.width(),kMinimapArea.size.y/map_.height()));
    if (scale<1.f) return std::nullopt;
    const sf::Vector2f origin{kMinimapArea.position.x+(kMinimapArea.size.x-scale*map_.width())/2,
                              kMinimapArea.position.y+(kMinimapArea.size.y-scale*map_.height())/2};
    const Position tile{static_cast<int>((p.x-origin.x)/scale),static_cast<int>((p.y-origin.y)/scale)};
    if (!map_.inBounds(tile.x,tile.y)) return std::nullopt;
    return tile;
}

void Application::renderMinimap(sf::FloatRect area) {
    if (map_.width()<=0 || map_.height()<=0) return;
    const float scale=std::floor(std::min(area.size.x/map_.width(),area.size.y/map_.height()));
    if (scale<1.f) return;
    const sf::Vector2f origin{area.position.x+(area.size.x-scale*map_.width())/2,
                              area.position.y+(area.size.y-scale*map_.height())/2};
    sf::VertexArray cells(sf::PrimitiveType::Triangles);
    const auto cell=[&](int x,int y,sf::Color c,float grow=0.f) {
        const float l=origin.x+x*scale-grow, t=origin.y+y*scale-grow, s=scale+2*grow;
        const sf::Vector2f a{l,t},b{l+s,t},c2{l+s,t+s},d{l,t+s};
        for (const auto v:{a,b,c2,a,c2,d}) cells.append(sf::Vertex{v,c});
    };
    for (int y=0;y<map_.height();++y) for (int x=0;x<map_.width();++x) {
        const auto vis=exploredMap_.at(x,y);
        if (vis==Visibility::Hidden) continue;
        const auto type=map_.tileAt(x,y).type;
        sf::Color c=type==TileType::Wall?sf::Color(92,78,60):type==TileType::Door?sf::Color(110,190,255):sf::Color(52,48,46);
        if (vis!=Visibility::Visible) c=sf::Color(c.r*2/3,c.g*2/3,c.b*2/3);
        cell(x,y,c);
    }
    for (const auto& m:monsters_) {
        const auto p=m->position();
        if (m->stats().hp>0 && !m->tactics.concealed && exploredMap_.at(p.x,p.y)==Visibility::Visible)
            cell(p.x,p.y,m->allied?sf::Color(110,230,230):sf::Color(230,70,60),0.5f);
        else if (sensedMonster(*m)) cell(p.x,p.y,sf::Color(170,60,55),0.5f);
    }
    if (landmark_!=LandmarkKind::None && exploredMap_.at(landmarkAltar_.x,landmarkAltar_.y)!=Visibility::Hidden)
        cell(landmarkAltar_.x,landmarkAltar_.y,landmarkUsed_?sf::Color(150,140,120):sf::Color(255,190,90),1.f);
    cell(player_.position().x,player_.position().y,sf::Color(255,214,90),1.f);
    window_.draw(cells);
    // The rectangle the main view currently shows.
    sf::RectangleShape view({scale*kMapWidth/kTile,scale*kMapHeight/kTile});
    view.setPosition({origin.x+cameraX_*scale,origin.y+cameraY_*scale});
    view.setFillColor(sf::Color::Transparent); view.setOutlineThickness(1.f); view.setOutlineColor(sf::Color(232,196,112,120));
    window_.draw(view);
}

void Application::renderBattleHud() {
    using namespace playLayout;
    const auto mouse=mousePixel_?std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)):std::nullopt;
    const auto hovered=[&](const sf::FloatRect& r){ return mouse && r.contains(*mouse); };
    const auto& stats=player_.stats();

    // --- Character column ---------------------------------------------------
    ui_.panel(window_,{{0,0},{static_cast<float>(sidebarWidth),720}});
    const sf::FloatRect portrait{{14,14},{64,64}};
    ui_.inset(window_,portrait,hovered(portrait)?ui::kGold:sf::Color(140,108,62));
    sprites_.draw(window_,playerSpriteFrame(),{portrait.position.x+4,portrait.position.y+4},56.f);
    ui_.text(window_,className(playerClass_),{90,14},20,ui::kGold,ui::Font::Title);
    ui_.text(window_,"Level "+std::to_string(player_.level()),{90,42},16,ui::kText,ui::Font::Bold);
    if (pointsToSpend()) {
        // Earned points wait here, pulsing, until the player chooses to spend them.
        const float pulse=0.5f+0.5f*std::sin(animationClock_.getElapsedTime().asSeconds()*4.f);
        sf::RectangleShape back(kLevelBadge.size); back.setPosition(kLevelBadge.position);
        back.setFillColor(sf::Color(70,52,20,static_cast<std::uint8_t>(170+60*pulse)));
        back.setOutlineThickness(1.f);
        back.setOutlineColor(hovered(kLevelBadge)?ui::kRare:sf::Color(232,196,112,static_cast<std::uint8_t>(140+110*pulse)));
        window_.draw(back);
        ui_.textCentered(window_,"+ Level up",kLevelBadge,13,ui::kRare,ui::Font::Bold);
    }
    ui_.text(window_,adventureMode_?"Adventure, "+std::to_string(extraLives_)+" spare li"+(extraLives_==1?"fe":"ves"):"Roguelike, one life",
        {90,62},13,ui::kMuted);

    ui_.bar(window_,{{14,90},{228,20}},stats.maxHp?static_cast<float>(stats.hp)/stats.maxHp:0.f,sf::Color(176,38,34),
        "Life  "+std::to_string(stats.hp)+" / "+std::to_string(stats.maxHp));
    ui_.bar(window_,{{14,114},{228,20}},stats.maxMana?static_cast<float>(stats.mana)/stats.maxMana:0.f,sf::Color(52,92,190),
        "Mana  "+std::to_string(stats.mana)+" / "+std::to_string(stats.maxMana));
    const bool maxLevel=player_.level()>=kRunMaxLevel;
    ui_.bar(window_,{{14,138},{228,12}},maxLevel?1.f:static_cast<float>(player_.xp())/std::max(1,xpForNextLevel(player_.level())),
        sf::Color(196,156,72));
    ui_.text(window_,maxLevel?"Experience: max level":"Experience "+std::to_string(player_.xp())+" / "+std::to_string(xpForNextLevel(player_.level())),
        {16,152},13,ui::kMuted);

    const auto theme=floorTheme(currentFloor_);
    if (trial_) {
        ui_.text(window_,trialName(trial_),{14,176},16,ui::kUnique,ui::Font::Bold);
        ui_.text(window_,std::string(trialGuardian(trial_))+(boss_?" awaits":" is fallen"),{14,196},14,ui::kText);
    } else {
        ui_.text(window_,std::string(dungeonName(dungeonIndex(currentFloor_)))+"  "+std::to_string((currentFloor_-1)%10+1)+"/10",{14,176},16,ui::kGold,ui::Font::Bold);
        ui_.text(window_,std::string(theme.name)+", depth "+std::to_string(currentFloor_),{14,196},14,ui::kText);
    }
    const auto stat=[&](const char* name,int value,float x,float y) {
        ui_.text(window_,name,{x,y},14,ui::kMuted);
        ui_.text(window_,std::to_string(value),{x+30,y},14,ui::kText,ui::Font::Bold);
    };
    stat("Str",stats.strength,14,220); stat("Dex",stats.dexterity,96,220); stat("Int",stats.intelligence,178,220);
    // armourName already says "armour" for light and heavy; cloth needs it added.
    const auto kind=player_.inventory().armourKind();
    const std::string armour=kind==ArmourKind::Cloth?"Cloth armour":armourName(kind);
    ui_.text(window_,armour+"   Gold "+std::to_string(gold_),{14,240},13,ui::kMuted);

    // Status effects: icon tiles, harmful ones rimmed red.
    const auto effects=hudEffects(player_);
    const int pages=std::max(1,(static_cast<int>(effects.size())+kStatusPerPage-1)/kStatusPerPage);
    statusPage_=std::clamp(statusPage_,0,pages-1);
    if (effects.empty()) ui_.text(window_,"No active effects",{statusX,statusY+10},14,ui::kMuted);
    for (int slot=0;slot<kStatusPerPage && statusPage_*kStatusPerPage+slot<static_cast<int>(effects.size());++slot) {
        const auto& e=effects[statusPage_*kStatusPerPage+slot];
        const auto r=statusRect(slot);
        const bool harmful=harmfulStatus(e.type);
        ui_.inset(window_,r,harmful?sf::Color(176,44,40):sf::Color(70,150,120));
        ui_.icon(window_,statusIcon(e.type),{{r.position.x+5,r.position.y+4},{r.size.x-10,r.size.y-12}},
            harmful?sf::Color(240,140,120):sf::Color(170,230,200));
        ui_.text(window_,e.type==StatusEffectType::BattleRhythm?"rdy":std::to_string(e.turnsRemaining),
            {r.position.x+r.size.x-16,r.position.y+r.size.y-17},12,ui::kGold,ui::Font::Bold);
    }
    if (pages>1) ui_.text(window_,"Effects "+std::to_string(statusPage_+1)+"/"+std::to_string(pages)+" (scroll)",
        {statusX,statusY+statusStride*statusRows-2},12,ui::kMuted);

    ui_.text(window_,"Map",{14,minimapY-22},14,ui::kGold,ui::Font::Title);
    const sf::FloatRect minimap{{14,minimapY},{228,150}};
    ui_.inset(window_,minimap);
    renderMinimap(kMinimapArea);

    const int quiet=dangerNearby()?0:quietTurns_;
    ui_.text(window_,"Town recall: "+std::to_string(quiet)+"/10 quiet turns",{14,minimapY+156},13,quiet>=10?ui::kGood:ui::kMuted);
    if(player_.talents().passiveValue(PassiveKind::Deathless)) {
        const bool spent=std::find(player_.deathlessSpentFloors.begin(),player_.deathlessSpentFloors.end(),currentFloor_)!=player_.deathlessSpentFloors.end();
        ui_.text(window_,spent?"Deathless: spent this floor":"Deathless: ready",{14,minimapY+174},13,spent?ui::kMuted:ui::kGood);
    }

    const bool busy=aimingTalent_ || inspecting_ || autoExploring_ || restTurns_>0;
    for (std::size_t i=0;i<kDungeonActionCount;++i) {
        const auto r=dungeonActionRect(i);
        ui_.inset(window_,r,hovered(r)&&!busy?ui::kBronze:sf::Color::Transparent);
        ui_.icon(window_,kDungeonActions[i].icon,{{r.position.x+6,r.position.y+4},{r.size.x-12,r.size.y-20}},
            busy?ui::kMuted:hovered(r)?ui::kGold:std::string_view(kDungeonActions[i].name)=="Torch"?
            (player_.lightLit?sf::Color(255,190,110):sf::Color(110,105,100)):ui::kText);
        const auto key=std::string(kDungeonActions[i].key);
        ui_.text(window_,key,{r.position.x+(r.size.x-ui_.textWidth(key,11,ui::Font::Bold))/2,r.position.y+r.size.y-16},11,ui::kMuted,ui::Font::Bold);
    }

    // --- Bottom strip: log and hotbar ---------------------------------------
    ui_.panel(window_,{{static_cast<float>(sidebarWidth),bottomTop},{1280.f-sidebarWidth,720.f-bottomTop}});
    ui_.inset(window_,{{logX-4,logY-6},{logWidth+8,logLineHeight*logLines+10}});
    const int shown=std::min<int>(logLines,static_cast<int>(logMessages_.size()));
    combatLogScroll_=std::clamp(combatLogScroll_,0,std::max(0,static_cast<int>(logMessages_.size())-shown));
    const int first=std::max(0,static_cast<int>(logMessages_.size())-shown-combatLogScroll_);
    for (int row=0;row<shown;++row) {
        std::string line=logMessages_[first+row];
        while (line.size()>4 && ui_.textWidth(line,14)>logWidth-8) line=line.substr(0,line.size()-5)+"...";
        // Older lines fade, ToME-style; the newest is brightest.
        const int age=shown-1-row+combatLogScroll_;
        const auto alpha=static_cast<std::uint8_t>(std::max(110,255-age*22));
        ui_.text(window_,line,{logX+2,logY+logLineHeight*row-2},14,sf::Color(222,212,192,alpha));
    }
    if (combatLogScroll_) ui_.text(window_,"older",{logX+logWidth-34,logY-4},11,ui::kGold);

    ui_.divider(window_,hotbarX-12,bottomTop+12,712);
    for(std::size_t slot=0;slot<kPageSize;++slot) {
        const auto index=player_.talents().hotbarIndex(talentPage_*kPageSize+slot);
        const auto r=hotbarRect(slot);
        const bool selected=index && aimingTalent_ && *index==*aimingTalent_;
        const bool hover=index && hoveredTalent_ && *index==*hoveredTalent_;
        ui_.inset(window_,r,selected?ui::kGold:hover?ui::kBronze:sf::Color::Transparent);
        ui_.text(window_,std::to_string(slot+1),{r.position.x+4,r.position.y+2},12,ui::kMuted,ui::Font::Bold);
        if(!index) continue;
        const auto talent=combatTalent(player_,player_.talents().effectiveTalent(*index));
        const int cd=player_.talents().cooldownRemaining(*index);
        const bool ready=talentUnavailableReason(player_,*index).empty();
        ui_.icon(window_,talentIcon(talent),{{r.position.x+7,r.position.y+6},{r.size.x-14,r.size.y-14}},
            ready?sf::Color(236,226,204):sf::Color(110,104,96));
        if (cd) {
            sf::RectangleShape shade({r.size.x-4,r.size.y-4}); shade.setPosition({r.position.x+2,r.position.y+2});
            shade.setFillColor(sf::Color(0,0,0,150)); window_.draw(shade);
            ui_.textCentered(window_,std::to_string(cd),r,22,ui::kGold,ui::Font::Title);
        }
        const std::string cost=talent.hpCost?std::to_string(talent.hpCost):talent.manaCost?std::to_string(talent.manaCost):"";
        if (!cost.empty())
            ui_.text(window_,cost,{r.position.x+r.size.x-4-ui_.textWidth(cost,12,ui::Font::Bold),r.position.y+r.size.y-17},12,
                talent.hpCost?sf::Color(240,110,100):sf::Color(130,170,255),ui::Font::Bold);
    }
    for(int dir=0;dir<2;++dir) ui_.button(window_,pageButton(dir),dir?">":"<",hovered(pageButton(dir)),true,16);
    ui_.text(window_,"Page "+std::to_string(talentPage_+1)+"/2",{pageButtonX+4,hotbarY+30},13,ui::kMuted);
    ui_.text(window_,"PgUp/PgDn",{pageButtonX+4,hotbarY+46},12,ui::kMuted);
    ui_.text(window_,"Click or press a number to use. Right-click a slot to change it.",{hotbarX,hotbarY+60},13,ui::kMuted);
}

std::vector<Actor*> Application::targetingEnemies() const {
    std::vector<Actor*> result;
    for (const auto& monster : monsters_)
        if (monster->stats().hp > 0 && !monster->allied) result.push_back(monster.get());
    return result;
}

TalentTarget Application::targetPreview(std::size_t index, Position cursor, bool includeConcealed) {
    if (index >= player_.talents().knownTalents().size()) return {};
    const auto talent=combatTalent(player_,player_.talents().effectiveTalent(index));
    if (talent.boneSwap) {
        TalentTarget result; result.destination=player_.position();
        if (!map_.isWalkable(cursor.x,cursor.y) || exploredMap_.at(cursor.x,cursor.y)!=Visibility::Visible || !hasLineOfFire(map_,player_.position(),cursor)) { result.message="Choose visible ground or an ally with a clear line of fire."; return result; }
        result.valid=true; result.area.push_back(cursor);
        for (auto& m:monsters_) if (m->allied && m->stats().hp>0 && m->position().x==cursor.x && m->position().y==cursor.y) { result.affected.push_back(m.get()); result.destination=cursor; }
        if (result.affected.empty()) result.message="No ally here: cast still spends its turn and cost.";
        return result;
    }
    auto enemies=targetingEnemies();
    if(!includeConcealed) enemies.erase(std::remove_if(enemies.begin(),enemies.end(),[](Actor* a){
        const auto* m=dynamic_cast<const Monster*>(a); return m && m->tactics.concealed;
    }),enemies.end());
    auto result=resolveTalentTarget(map_, exploredMap_, player_, enemies,talent,cursor);
    if (talent.shape==EffectShape::Movement || talent.retreatDistance) {
        for (const auto& m:monsters_) if (m->allied && m->stats().hp>0) {
            const auto p=m->position();
            const auto& path=talent.shape==EffectShape::Movement?result.path:result.movementPath;
            if (std::any_of(path.begin(),path.end(),[&](Position q){return q.x==p.x && q.y==p.y;})) {
                result.valid=false; result.message="An ally blocks this movement. Walk into it to swap, or use Bone Swap.";
            }
        }
    }
    return result;
}

void Application::cancelTargeting() {
    aimingTalent_.reset();
    inspecting_ = false;
    inspectionScroll_=0; inspectionAnchor_.reset();
    hoveredTalent_.reset();
}

void Application::requestTalent(std::size_t index) {
    const auto& known = player_.talents().knownTalents();
    if (index >= known.size()) return;
    const auto reason = talentUnavailableReason(player_, index);
    if (!reason.empty()) { log(reason); return; }
    cancelTargeting();
    const Talent talent = combatTalent(player_,player_.talents().effectiveTalent(index));
    if (talent.targeting == TargetingMode::Self && talent.shape == EffectShape::SingleTarget) {
        tryUseTalent(index, player_.position());
        return;
    }
    aimingTalent_ = index;
    targetCursor_ = player_.position();
    if (talent.shape != EffectShape::Movement && talent.shape != EffectShape::AreaAroundSelf)
        cycleTarget();
}

void Application::cycleTarget(int direction) {
    std::vector<Position> candidates;
    for (const auto& monster : monsters_) {
        const Position p = monster->position();
        if (monster->tactics.concealed || monster->stats().hp <= 0 || (monster->allied && (!aimingTalent_ || !player_.talents().effectiveTalent(*aimingTalent_).boneSwap)) || exploredMap_.at(p.x, p.y) != Visibility::Visible) continue;
        if (aimingTalent_) {
            const auto result = targetPreview(*aimingTalent_, p);
            // An intercepted enemy isn't independently targetable by this ray.
            if (!result.valid || std::find(result.affected.begin(), result.affected.end(),
                monster.get()) == result.affected.end()) continue;
        }
        candidates.push_back(p);
    }
    std::sort(candidates.begin(), candidates.end(), [&](Position a, Position b) {
        const auto from = player_.position();
        const int da = (a.x-from.x)*(a.x-from.x) + (a.y-from.y)*(a.y-from.y);
        const int db = (b.x-from.x)*(b.x-from.x) + (b.y-from.y)*(b.y-from.y);
        if (da != db) return da < db;
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    if (candidates.empty()) return;
    const auto found = std::find_if(candidates.begin(), candidates.end(),
        [&](Position p) { return same(p, targetCursor_); });
    int index = found == candidates.end() ? (direction > 0 ? -1 : 0) :
        static_cast<int>(found - candidates.begin());
    index = (index + direction + static_cast<int>(candidates.size())) % static_cast<int>(candidates.size());
    targetCursor_ = candidates[static_cast<std::size_t>(index)];
}

void Application::changeTalentPage(int direction) {
    cancelTargeting();
    const auto count = std::max<std::size_t>(18,player_.talents().hotbar().size());
    const int pages = static_cast<int>(std::max<std::size_t>(1, (count + kPageSize - 1) / kPageSize));
    talentPage_ = static_cast<std::size_t>((static_cast<int>(talentPage_) + direction + pages) % pages);
}

bool Application::handleTargetingKey(sf::Keyboard::Key key, bool shift) {
    if (key == sf::Keyboard::Key::PageDown) { changeTalentPage(1); return true; }
    if (key == sf::Keyboard::Key::PageUp) { changeTalentPage(-1); return true; }
    if (key == sf::Keyboard::Key::I) {
        const bool wasInspecting = inspecting_;
        cancelTargeting();
        inspecting_ = !wasInspecting;
        targetCursor_ = player_.position();
        if (inspecting_) cycleTarget();
        return true;
    }
    if (key == sf::Keyboard::Key::Tab) {
        if (!aimingTalent_ && !inspecting_) {
            inspecting_ = true;
            targetCursor_ = player_.position();
        }
        cycleTarget(shift ? -1 : 1);
        return true;
    }
    if (!aimingTalent_ && !inspecting_) return false;
    if (key == sf::Keyboard::Key::Enter) {
        if (aimingTalent_) tryUseTalent(*aimingTalent_, targetCursor_);
        return true;
    }
    Position delta;
    switch (key) {
        case sf::Keyboard::Key::W: case sf::Keyboard::Key::Up: delta.y = -1; break;
        case sf::Keyboard::Key::S: case sf::Keyboard::Key::Down: delta.y = 1; break;
        case sf::Keyboard::Key::A: case sf::Keyboard::Key::Left: delta.x = -1; break;
        case sf::Keyboard::Key::D: case sf::Keyboard::Key::Right: delta.x = 1; break;
        default: return false;
    }
    targetCursor_.x = std::clamp(targetCursor_.x + delta.x, 0, map_.width() - 1);
    targetCursor_.y = std::clamp(targetCursor_.y + delta.y, 0, map_.height() - 1);
    return true;
}

void Application::handleTargetingMouse(const sf::Event& event) {
    updateCamera(); // Events may follow movement before the next rendered frame.
    if (event.is<sf::Event::MouseLeft>()) {
        mousePixel_.reset(); hoveredTalent_.reset();
    }
    if (const auto* move = event.getIf<sf::Event::MouseMoved>()) {
        mousePixel_ = move->position;
        hoveredTalent_ = talentAtPixel(move->position);
        if (aimingTalent_ || inspecting_)
            if (const auto tile = screenToWorld(move->position)) targetCursor_ = *tile;
    }
    if (const auto* wheel=event.getIf<sf::Event::MouseWheelScrolled>()) {
        const auto p=sf::Vector2f(wheel->position);
        const int step=wheel->delta<0?1:wheel->delta>0?-1:0;
        if (sf::FloatRect({0,playLayout::statusY},{static_cast<float>(playLayout::sidebarWidth),
                playLayout::statusStride*playLayout::statusRows}).contains(p))
            statusPage_=std::max(0,statusPage_+step);
        // Scrolling up (away from you) reaches older messages.
        if (kLogArea.contains(p))
            combatLogScroll_=std::clamp(combatLogScroll_-step,0,std::max(0,static_cast<int>(logMessages_.size())-playLayout::logLines));
        // Over the map, the wheel scrolls the enemy inspection tooltip.
        if (onMap(p)) inspectionScroll_=std::max(0,inspectionScroll_+3*step);
    }
    if (const auto* click = event.getIf<sf::Event::MouseButtonPressed>()) {
        mousePixel_ = click->position;
        if (click->button == sf::Mouse::Button::Right) {
            // Right-clicking a hotbar slot opens the talents to rebind it.
            if (!aimingTalent_ && !inspecting_) {
                const auto p=sf::Vector2f(click->position);
                for (std::size_t slot=0;slot<kPageSize;++slot) if (hotbarRect(slot).contains(p)) { openTalentTrees(); return; }
            }
            cancelTargeting(); return;
        }
        if (click->button != sf::Mouse::Button::Left) return;
        const auto p=sf::Vector2f(click->position);
        if(aimingTalent_ || inspecting_) {
            if(kCancelAction.contains(p)) { cancelTargeting(); return; }
        } else for(std::size_t i=0;i<kDungeonActionCount;++i) if(dungeonActionRect(i).contains(p)) {
            const std::string action=kDungeonActions[i].name;
            if(action=="Inventory") openInventory();
            else if(action=="Talent trees") openTalentTrees();
            else if(action=="Use / pick up") pickupItem();
            else if(action=="Wait a turn") { player_.statusEffects().apply({StatusEffectType::Opening,2,0}); finishInventoryTurn(); }
            else if(action=="Rest") startRest();
            else if(action=="Auto-explore") startAutoExplore();
            else if(action=="Torch") toggleLight();
            else if(action=="Return to town") returnToTown();
            else if(action=="Cleanse") {
                for(std::size_t index=0;index<player_.talents().knownTalents().size();++index)
                    if(player_.talents().knownTalents()[index].id=="basic.cleanse") { requestTalent(index); break; }
            }
            else if(action=="Save game") saveGame();
            else if(action=="Load game") loadGame();
            else if(action=="Inspect enemies") handleTargetingKey(sf::Keyboard::Key::I,false);
            return;
        }
        if(!aimingTalent_ && !inspecting_) {
            if(kPortraitArea.contains(p)) { openInventory(); return; }
            if(pointsToSpend() && kLevelBadge.contains(p)) { openLevelUp(); return; }
            if(kNameArea.contains(p) || kXpArea.contains(p)) { openTalentTrees(); return; }
            if(const auto tile=minimapTile(p)) {
                if(exploredMap_.at(tile->x,tile->y)==Visibility::Hidden || !map_.isWalkable(tile->x,tile->y)) log("You haven't explored there yet.");
                else if(!startTravel(*tile)) log("Can't walk away while danger is near.");
                return;
            }
            // Clicking a curable effect cleanses it (the Cleanse talent decides if it can).
            if(const auto status=hoveredStatus()) {
                if(isCleansable(status->type)) {
                    for(std::size_t index=0;index<player_.talents().knownTalents().size();++index)
                        if(player_.talents().knownTalents()[index].id=="basic.cleanse") { requestTalent(index); break; }
                } else log(statusName(status->type)," can't be cleansed.");
                return;
            }
        }
        for (int dir=0;dir<2;++dir) if (pageButton(dir).contains(p)) { changeTalentPage(dir?1:-1); return; }
        if (const auto index = talentAtPixel(click->position)) { requestTalent(*index); return; }
        const auto tile = screenToWorld(click->position);
        if (!tile) return; // Clicking HUD never casts through it.
        targetCursor_ = *tile;
        if (aimingTalent_) { tryUseTalent(*aimingTalent_, targetCursor_); return; }
        if (inspecting_) return; // Explicit I/Tab inspection remains a free mode.
        const auto vision=exploredMap_.at(tile->x,tile->y);
        // The landmark altar: use it from beside it, or walk over first.
        if (landmark_!=LandmarkKind::None && vision!=Visibility::Hidden && same(*tile,landmarkAltar_)) {
            if (nearAltar()) { openShrine(); return; }
            std::optional<Position> best;
            const auto from=player_.position();
            for (const Position d:{Position{1,0},Position{-1,0},Position{0,1},Position{0,-1}}) {
                const Position n{landmarkAltar_.x+d.x,landmarkAltar_.y+d.y};
                if (!map_.isWalkable(n.x,n.y) || exploredMap_.at(n.x,n.y)==Visibility::Hidden) continue;
                if (!best || std::abs(n.x-from.x)+std::abs(n.y-from.y)<std::abs(best->x-from.x)+std::abs(best->y-from.y)) best=n;
            }
            if (!best) { log("You can't reach the altar from here."); return; }
            if (!startTravel(*best)) { log("Danger is too close to kneel at the altar."); return; }
            travelToAltar_=autoExploring_;
            return;
        }
        if (auto* target=dynamic_cast<Monster*>(actorAt(*tile,&player_));
            vision==Visibility::Visible && target && !target->allied && !target->tactics.concealed) {
            for (std::size_t i=0;i<player_.talents().knownTalents().size();++i)
                if (player_.talents().knownTalents()[i].id=="basic.attack") {
                    tryUseTalent(i,*tile); return;
                }
            return;
        }
        if (vision==Visibility::Hidden) { log("You haven't explored there yet."); return; }
        if (!map_.isWalkable(tile->x,tile->y)) { log("You can't walk there."); return; }
        const auto from=player_.position();
        const int dx=tile->x-from.x,dy=tile->y-from.y;
        // Clicking yourself uses what's underfoot: items, chests, stairs, vaults.
        if (!dx && !dy) { pickupItem(); return; }
        // Farther away: walk there over known ground. With danger nearby that
        // isn't allowed, so fall back to a single step toward the click.
        if (std::abs(dx)+std::abs(dy)>1 && startTravel(*tile)) return;
        const Position step=std::abs(dx)>=std::abs(dy)?Position{dx>0?1:-1,0}:Position{0,dy>0?1:-1};
        if (const auto* enemy=dynamic_cast<const Monster*>(actorAt({from.x+step.x,from.y+step.y},&player_)); enemy && !enemy->allied && !enemy->tactics.concealed) {
            log("An enemy blocks that step. Click it to attack or select an ability."); return;
        }
        tryMovePlayer(step.x,step.y);
    }
}

void Application::drawWrapped(const std::string& text, float x, float& y,
    std::size_t columns, sf::Color color, float bottom) {
    // `columns` dates from the monospace font; ~7px per column at 14px.
    for (const auto& line : ui_.wrap(text, columns*7.2f, 14)) {
        if (y + 16.f <= bottom) ui_.text(window_, line, {x, y}, 14, color);
        y += 17.f;
    }
}

void Application::renderTargetingOverlay() {
    auto inViewport = [&](Position p) {
        return p.x >= cameraX_ && p.x < cameraX_ + kMapWidth / kTile &&
            p.y >= cameraY_ && p.y < cameraY_ + kMapHeight / kTile &&
            exploredMap_.at(p.x, p.y) == Visibility::Visible;
    };
    auto mark = [&](Position p, sf::Color color, bool fill) {
        if (!inViewport(p)) return;
        sf::RectangleShape box({kTile - 2.f, kTile - 2.f});
        const auto pos = worldToScreen(p.x, p.y);
        box.setPosition({pos.x + 1.f, pos.y + 1.f});
        box.setFillColor(fill ? sf::Color(color.r, color.g, color.b, 65) : sf::Color::Transparent);
        box.setOutlineColor(color);
        box.setOutlineThickness(-2.f);
        window_.draw(box);
    };
    std::optional<Position> inspectionTile;
    if (aimingTalent_ || inspecting_) inspectionTile = targetCursor_;
    else if (mousePixel_) inspectionTile = screenToWorld(*mousePixel_);
    if (inspectionTile) mark(*inspectionTile, sf::Color(245, 220, 100), false);
    if (!aimingTalent_) return;
    const auto preview = targetPreview(*aimingTalent_, targetCursor_);
    const sf::Color color = preview.valid ? sf::Color(80, 220, 220) : sf::Color(245, 100, 90);
    for (Position p : preview.area) mark(p, color, true);
    for (Position p : preview.movementPath) mark(p, sf::Color(130, 160, 255), false);
    auto arrow = [&](const std::vector<Position>& path) {
        for (std::size_t i = 1; i < path.size(); ++i) {
            if (!inViewport(path[i - 1]) || !inViewport(path[i])) continue;
            auto a = worldToScreen(path[i - 1].x, path[i - 1].y);
            auto b = worldToScreen(path[i].x, path[i].y);
            a += sf::Vector2f(kTile / 2.f, kTile / 2.f);
            b += sf::Vector2f(kTile / 2.f, kTile / 2.f);
            const sf::Vertex segment[] = {{a, color}, {b, color}};
            window_.draw(segment, 2, sf::PrimitiveType::Lines);
            if (i + 1 == path.size()) {
                const auto difference = b - a;
                const float length = std::sqrt(difference.x * difference.x + difference.y * difference.y);
                if (length <= 0.f) continue;
                const sf::Vector2f direction = difference / length;
                const sf::Vector2f perpendicular{-direction.y, direction.x};
                sf::ConvexShape head(3);
                head.setPoint(0, b);
                head.setPoint(1, b - direction * 10.f + perpendicular * 5.f);
                head.setPoint(2, b - direction * 10.f - perpendicular * 5.f);
                head.setFillColor(color); window_.draw(head);
            }
        }
    };
    arrow(preview.path);
    arrow(preview.movementPath);
    arrow(preview.chainPath);
    // A valid ground area stays cyan; only enemies actually hit get gold outlines.
    for (const auto* enemy:preview.affected) {
        const auto* m=dynamic_cast<const Monster*>(enemy);
        if (enemy!=&player_ && (!m || !m->tactics.concealed)) mark(enemy->position(),sf::Color(255,190,65),false);
    }
    if (preview.blockedAt) mark(*preview.blockedAt, sf::Color(255, 90, 75), true);
    if (player_.talents().knownTalents()[*aimingTalent_].shape == EffectShape::Movement)
        mark(preview.destination, sf::Color(130, 255, 150), true);
}

void Application::renderHudTooltips() {
    using ui::Line;
    const auto mouse=mousePixel_?std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)):std::nullopt;
    const sf::FloatRect screen{{0,0},{1280,720}};
    const sf::FloatRect aboveHotbar{{0,0},{1280,playLayout::hotbarY-8}};

    // --- Mode banner along the bottom of the map ----------------------------
    if (autoExploring_ || restTurns_>0 || aimingTalent_ || inspecting_) {
        ui_.panel(window_,kModeBanner,false,sf::Color(150,140,130));
        const std::string text=autoExploring_ && travelGoal_ ? "Walking. Any key or click stops; danger and new sightings pause it." :
            autoExploring_ ? "Exploring. Any key or click stops; danger and discoveries pause it." :
            restTurns_>0 ? "Resting. Any key or click stops; recovers life, mana and cooldowns." :
            aimingTalent_ ? "Aim with mouse or arrows, Tab cycles targets, Enter or click casts" :
            "Inspecting: mouse or arrows, Tab for the next enemy, I or Esc closes";
        ui_.text(window_,text,{kModeBanner.position.x+14,kModeBanner.position.y+7},15,ui::kText);
        ui_.button(window_,kCancelAction,autoExploring_ || restTurns_>0?"Stop":"Cancel",
            mouse && kCancelAction.contains(*mouse),true,14);
    }

    // --- Talent: hovered or being aimed, shown above its hotbar slot --------
    const auto& talents = player_.talents().knownTalents();
    const auto selected = aimingTalent_ ? aimingTalent_ : hoveredTalent_;
    if (selected && *selected < talents.size()) {
        const auto talent = combatTalent(player_,player_.talents().effectiveTalent(*selected));
        std::vector<Line> lines{{talent.name,ui::kGold,19,ui::Font::Title},
            {"Rank "+std::to_string(player_.talents().rank(*selected))+" of 3",ui::kMuted,14},{""},
            {talent.description,ui::kText,15}};
        lines.push_back({""});
        if (talent.manaCost) lines.push_back({"Mana cost: "+std::to_string(talent.manaCost),sf::Color(130,170,255),15});
        if (talent.hpCost) lines.push_back({"Life cost: "+std::to_string(talent.hpCost),sf::Color(240,110,100),15});
        lines.push_back({"Cooldown: "+(talent.cooldownTurns?std::to_string(talent.cooldownTurns)+" turns":std::string("none")),ui::kText,15});
        lines.push_back({talent.shape == EffectShape::Movement ?
            "Movement: up to " + std::to_string(talent.moveDistance) + " tiles; stops at blockers." :
            talent.projectile ? "Aim at visible ground; the projectile stops at the first enemy." :
            talent.targeting == TargetingMode::AdjacentEnemy ? "Aim at adjacent ground (no diagonals)." :
            talent.targeting == TargetingMode::Self ? "Centred on yourself." : "Range: any visible ground tile.",ui::kInfo,15});
        if (talent.tags & AreaTag) lines.push_back({"Radius: " + std::to_string(talent.areaRadius),ui::kInfo,15});
        if (talent.movementBurn && (talent.shape==EffectShape::Movement || talent.retreatDistance))
            lines.push_back({"Kindle: landing burns adjacent visible enemies for "+std::to_string(talent.movementBurn)+" per turn for 2 turns, and reveals you.",ui::kInfo,15});
        const auto unavailable = talentUnavailableReason(player_, *selected);
        if (!unavailable.empty()) lines.push_back({unavailable,ui::kBad,15});
        if (aimingTalent_) {
            auto preview = targetPreview(*selected, targetCursor_);
            preview.affected.erase(std::remove_if(preview.affected.begin(),preview.affected.end(),[](const Actor* a){
                const auto* m=dynamic_cast<const Monster*>(a); return m && m->tactics.concealed;
            }),preview.affected.end());
            if (talent.effectKind == TalentEffectKind::Damage && !preview.affected.empty()) {
                int low = 0, high = 0;
                bool first = true;
                for (const auto* actor : preview.affected) {
                    auto hitTalent = talent;
                    if (actor == preview.chainedTarget) hitTalent.damagePercent /= 2;
                    const auto damage = estimateTalentDamage(hitTalent, player_, *actor);
                    low = first ? damage.normal : std::min(low, damage.normal);
                    high = std::max(high, damage.critical); first = false;
                }
                lines.push_back({""});
                lines.push_back({"On hit: " + std::to_string(low) + "-" + std::to_string(high) +
                    " damage (includes crits). Can miss. Targets: " + std::to_string(preview.affected.size()),ui::kGood,15,ui::Font::Bold});
            }
            if (!preview.message.empty()) lines.push_back({preview.message,preview.valid?ui::kInfo:ui::kBad,15});
        }
        // Anchor over the slot it's bound to on this page, or the hotbar's start.
        float slotX=playLayout::hotbarX;
        for (std::size_t slot=0;slot<kPageSize;++slot)
            if (const auto i=player_.talents().hotbarIndex(talentPage_*kPageSize+slot); i && *i==*selected) slotX=hotbarRect(slot).position.x;
        // While aiming, stay clear of the mode banner as well.
        const sf::FloatRect room=aimingTalent_?sf::FloatRect{{0,0},{1280,kModeBanner.position.y-6}}:aboveHotbar;
        ui_.tooltip(window_,lines,{slotX-18,room.size.y},360,room);
        return;
    }

    // --- Status effect ------------------------------------------------------
    if (const auto status=hoveredStatus(); status && mouse) {
        std::vector<Line> lines{{statusName(status->type),harmfulStatus(status->type)?ui::kBad:ui::kGood,18,ui::Font::Title},
            {status->type==StatusEffectType::BattleRhythm?"Ready until used":"Remaining: "+std::to_string(status->turnsRemaining)+" status ticks",ui::kMuted,14},
            {""},{statusTooltip(*status),ui::kText,15}};
        if (isCleansable(status->type)) lines.push_back({"Click or press C to cleanse it.",ui::kInfo,15});
        ui_.tooltip(window_,lines,*mouse,320,screen);
        return;
    }

    // --- Clickable parts of the character column ------------------------------
    if (mouse && !aimingTalent_ && !inspecting_) {
        const auto hint=[&](const std::string& title,const std::string& detail) {
            ui_.tooltip(window_,{{title,ui::kGold,16,ui::Font::Bold},{detail,ui::kMuted,14}},*mouse,240,screen);
        };
        if (kPortraitArea.contains(*mouse)) { hint("Inventory","Click to open your equipment and bag (B)."); return; }
        if (pointsToSpend() && kLevelBadge.contains(*mouse)) {
            std::string detail;
            const auto add=[&](int n,const char* what){ if(n>0) detail+=(detail.empty()?"":", ")+std::to_string(n)+" "+what; };
            add(player_.unspentAttributePoints(),"attribute"); add(player_.abilityPoints(),"ability"); add(player_.treePoints(),"tree");
            hint("Points to spend",detail+" point(s). Click or press P to spend them; they keep until you do.");
            return;
        }
        if (kNameArea.contains(*mouse) || kXpArea.contains(*mouse)) { hint("Talents","Click to open your talent trees (T)."); return; }
        if (const auto tile=minimapTile(*mouse)) {
            hint("Map",exploredMap_.at(tile->x,tile->y)==Visibility::Hidden?"Unexplored.":"Click to walk there.");
            return;
        }
    }

    // --- Action buttons -------------------------------------------------------
    if (mouse) for (std::size_t i=0;i<kDungeonActionCount;++i) if (dungeonActionRect(i).contains(*mouse)) {
        ui_.tooltip(window_,{{kDungeonActions[i].name,ui::kGold,16,ui::Font::Bold},{std::string("Key: ")+kDungeonActions[i].key,ui::kMuted,14}},
            {dungeonActionRect(i).position.x+dungeonActionRect(i).size.x-10,dungeonActionRect(i).position.y-60},220,screen);
        return;
    }

    // --- Log line: the full, untruncated message ---------------------------------
    if (const auto line=hoveredLogLine(); line && mouse) {
        ui_.tooltip(window_,{{logMessages_[*line],ui::kText,15}},{mouse->x,mouse->y-80},420,aboveHotbar);
        return;
    }

    // --- Enemy inspection, next to the enemy --------------------------------
    std::optional<Position> inspectTile;
    if (aimingTalent_ || inspecting_) inspectTile = targetCursor_;
    else if (mousePixel_) inspectTile=screenToWorld(*mousePixel_);
    if (!inspectTile || !inspectionAnchor_ || !same(*inspectTile,*inspectionAnchor_)) inspectionScroll_=0;
    inspectionAnchor_=inspectTile;
    if (!inspectTile) return;
    for (const auto& monster:monsters_) {
        if (!same(monster->position(),*inspectTile)) continue;
        if (sensedMonster(*monster)) {
            const auto at=worldToScreen(inspectTile->x,inspectTile->y);
            ui_.tooltip(window_,{{monster->name(),ui::kGold,18,ui::Font::Title},
                {"Hunting you from out of sight.",ui::kBad,15},
                {std::to_string(monster->stats().hp)+"/"+std::to_string(monster->stats().maxHp)+" life",ui::kMuted,14}},
                {at.x+kTile,at.y},300,{{static_cast<float>(kMapLeft),static_cast<float>(kMapTop)},
                {static_cast<float>(kMapWidth),static_cast<float>(kMapHeight)}});
            return;
        }
        auto details=inspectMonster(*monster,exploredMap_);
        if (details.empty()) continue; // dead or hidden actors never disclose information
        std::vector<Line> lines;
        if (monster->allied) {
            lines.push_back({"Allied skeleton",sf::Color(110,230,230),18,ui::Font::Title});
            lines.push_back({std::to_string(monster->stats().hp)+"/"+std::to_string(monster->stats().maxHp)+" life. Walk into it to swap places.",ui::kText,15});
            lines.push_back({monster->remainingLife?"Expires in "+std::to_string(monster->remainingLife)+" actions.":"Lasts until it dies or you travel.",ui::kMuted,14});
        } else {
            const sf::Color nameColor=isUniqueMonster(monster->type())?ui::kRare:
                monster->tier()==MonsterTier::Nightmare?sf::Color(255,255,255):monster->tier()==MonsterTier::Elite?sf::Color(255,200,60):ui::kGold;
            lines.push_back({details.front(),nameColor,18,ui::Font::Title});
            std::vector<std::string> rest(details.begin()+1,details.end());
            if (player_.statusEffects().has(StatusEffectType::Concealed)) {
                const float chance=enemyStealthDetectionChance(*monster);
                rest.insert(rest.begin(),chance>0.f ? "Stealth detection: "+std::to_string(static_cast<int>(std::lround(chance*100.f)))+
                    "% on its next active turn." : "No stealth check from here.");
            }
            constexpr int kVisible=12;
            inspectionScroll_=std::clamp(inspectionScroll_,0,std::max(0,static_cast<int>(rest.size())-kVisible));
            for (int i=inspectionScroll_;i<static_cast<int>(rest.size()) && i<inspectionScroll_+kVisible;++i) {
                const auto& text=rest[static_cast<std::size_t>(i)];
                const bool warning=text.rfind("Strike",0)==0 || text.rfind("Ritual",0)==0 || text.rfind("Committed",0)==0;
                lines.push_back({text,i==0?ui::kText:warning?ui::kBad:sf::Color(196,188,170),i==0?15u:14u,i==0?ui::Font::Bold:ui::Font::Body});
            }
            if (static_cast<int>(rest.size())>kVisible) lines.push_back({"Scroll for more ("+std::to_string(inspectionScroll_+1)+"-"+
                std::to_string(std::min(inspectionScroll_+kVisible,static_cast<int>(rest.size())))+" of "+std::to_string(rest.size())+")",ui::kMuted,13});
        }
        const auto at=worldToScreen(inspectTile->x,inspectTile->y);
        ui_.tooltip(window_,lines,{at.x+kTile,at.y},330,{{static_cast<float>(kMapLeft),static_cast<float>(kMapTop)},
            {static_cast<float>(kMapWidth),static_cast<float>(kMapHeight)}});
        return;
    }
    // No creature there: describe the ground or fixture instead.
    if (exploredMap_.at(inspectTile->x,inspectTile->y)==Visibility::Hidden) return;
    std::vector<Line> lines;
    if (const auto s=surfaceAt(*inspectTile); s!=SurfaceType::None) {
        lines.push_back({surfaceName(s),ui::kGold,16,ui::Font::Bold});
        lines.push_back({surfaceHint(s),ui::kText,14});
    }
    if (const int index=propIndexAt(inspectTile->x,inspectTile->y); index>=0) {
        const auto kind=props_[static_cast<std::size_t>(index)].kind;
        const char* hint=kind==PropKind::Brazier?"Lights the dark. Walk into it to spill burning coals; cold puts it out.":
            kind==PropKind::ColdBrazier?"Unlit. A fire spell lights it.":
            kind==PropKind::OilBarrel?"Fire or lightning blows it up; an arrow punctures it; walk into it to flood the floor with oil.":nullptr;
        if (hint) { lines.push_back({propName(kind),ui::kGold,16,ui::Font::Bold}); lines.push_back({hint,ui::kText,14}); }
    }
    for (const auto& front:wallTorches_) if (front.x==inspectTile->x && (front.y==inspectTile->y || front.y-1==inspectTile->y)) {
        const bool lit=torchLit(front.x,front.y-1);
        lines.push_back({lit?"Wall torch":"Unlit wall torch",ui::kGold,16,ui::Font::Bold});
        lines.push_back({lit?"Cold or an arrow puts it out.":"A fire spell lights it.",ui::kText,14});
        break;
    }
    if (lines.empty()) return;
    const auto at=worldToScreen(inspectTile->x,inspectTile->y);
    ui_.tooltip(window_,lines,{at.x+kTile,at.y},280,{{static_cast<float>(kMapLeft),static_cast<float>(kMapTop)},
        {static_cast<float>(kMapWidth),static_cast<float>(kMapHeight)}});
}

// Short contextual prompts ("item at your feet", vault, stairs) stack in
// the top-left of the map.
void Application::renderMapHints() {
    float y=kMapTop+8.f;
    for (const auto& [text,color]:mapHints_) {
        const float w=ui_.textWidth(text,15)+24;
        sf::RectangleShape back({w,26}); back.setPosition({kMapLeft+8.f,y});
        back.setFillColor(sf::Color(10,8,10,200)); back.setOutlineThickness(1); back.setOutlineColor(sf::Color(140,108,62,200));
        window_.draw(back);
        ui_.text(window_,text,{kMapLeft+20.f,y+4},15,color);
        y+=32.f;
    }
    mapHints_.clear();
}
} // namespace engine
