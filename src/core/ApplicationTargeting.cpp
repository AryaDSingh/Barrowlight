#include "core/Application.hpp"

#include <algorithm>
#include <cmath>
#include "core/PlayLayout.hpp"
#include "entities/MonsterInspection.hpp"
#include "entities/TalentEffects.hpp"
#include "entities/HiddenCombat.hpp"
#include "world/LineOfFire.hpp"

namespace engine {
namespace {
constexpr int kMapWidth = playLayout::mapWidth, kMapTop = playLayout::mapTop,
    kMapHeight = playLayout::mapHeight, kTile = playLayout::tileSize;
constexpr float kPanelX = 912.f, kPanelRight = 1264.f;
constexpr std::size_t kPageSize = 9;
constexpr const char* kDungeonActions[]{"Bag B","Trees T","Codex J","Use G","Wait","Rest R","Explore Z","Town H","Cleanse C","Save F5","Load F9"};
sf::FloatRect dungeonActionRect(std::size_t index) { return {{10+80.f*index,64},{77,22}}; }
const sf::FloatRect kCancelAction{{795,64},{95,22}};
std::vector<StatusEffectInstance> hudEffects(const Player& player) {
    std::vector<StatusEffectInstance> effects;
    for (const auto& e:player.statusEffects().active()) if(e.type!=StatusEffectType::UnseenReady) effects.push_back(e);
    return effects;
}
std::string statusTooltip(const StatusEffectInstance& e) {
    const auto n=std::to_string(e.magnitude);
    switch(e.type) {
    case StatusEffectType::Poison: case StatusEffectType::Burn: return "Lose "+n+" HP each status tick. Bypasses Guard and reveals concealment.";
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
    // mapPixelToCoords handles resizing using SFML's unchanged logical view.
    const auto p = window_.mapPixelToCoords(pixel);
    if (p.x < 0 || p.x >= kMapWidth || p.y < kMapTop || p.y >= kMapTop + kMapHeight)
        return std::nullopt;
    const Position tile{cameraX_ + static_cast<int>(p.x) / kTile,
        cameraY_ + (static_cast<int>(p.y) - kMapTop) / kTile};
    if (!map_.inBounds(tile.x, tile.y)) return std::nullopt;
    return tile;
}

std::optional<std::size_t> Application::talentAtPixel(sf::Vector2i pixel) const {
    const auto p = window_.mapPixelToCoords(pixel);
    if (p.x<playLayout::hotbarX || p.y<playLayout::hotbarY || p.y>=playLayout::hotbarY+playLayout::hotbarHeight) return std::nullopt;
    const float local=p.x-playLayout::hotbarX;
    const auto slot=static_cast<std::size_t>(local/playLayout::hotbarStride);
    if (slot>=kPageSize || local-slot*playLayout::hotbarStride>=playLayout::hotbarWidth) return std::nullopt;
    return player_.talents().hotbarIndex(talentPage_*kPageSize+slot);
}

std::optional<StatusEffectInstance> Application::hoveredStatus() const {
    if (!mousePixel_) return std::nullopt;
    const auto p=window_.mapPixelToCoords(*mousePixel_);
    if (p.x<10 || p.y<playLayout::statusY || p.y>=playLayout::statusY+playLayout::statusHeight) return std::nullopt;
    const int slot=static_cast<int>((p.x-10)/playLayout::statusStride);
    if(slot>=9 || p.x-10-slot*playLayout::statusStride>=74) return std::nullopt;
    const auto effects=hudEffects(player_);
    const int page=std::min(statusPage_,std::max(0,(static_cast<int>(effects.size())-1)/9));
    const int index=page*9+slot;
    if(index>=static_cast<int>(effects.size())) return std::nullopt;
    return effects[index];
}

void Application::renderBattleHud() {
    sf::RectangleShape backdrop({896.f,128.f}); backdrop.setPosition({0,592});
    backdrop.setFillColor(sf::Color(13,17,24)); window_.draw(backdrop);
    const auto effects=hudEffects(player_);
    const int pages=std::max(1,(static_cast<int>(effects.size())+8)/9);
    statusPage_=std::clamp(statusPage_,0,pages-1);
    for(int slot=0;slot<9 && statusPage_*9+slot<static_cast<int>(effects.size());++slot) {
        const auto& e=effects[statusPage_*9+slot]; const float x=10+slot*78.f;
        sf::RectangleShape badge({74,24}); badge.setPosition({x,594});
        const bool harmful=isCleansable(e.type) || e.type==StatusEffectType::Stun || e.type==StatusEffectType::Wither || e.type==StatusEffectType::Shock;
        badge.setFillColor(harmful?sf::Color(91,40,45):sf::Color(31,66,70)); window_.draw(badge);
        drawText(std::string(statusName(e.type)).substr(0,9),x+3,595,10,sf::Color::White);
        drawText(e.type==StatusEffectType::BattleRhythm?"Ready":std::to_string(e.turnsRemaining)+" t",x+3,605,10,sf::Color(235,211,149));
    }
    if(effects.empty()) drawText("No active effects",10,598,12,sf::Color(150,165,180));
    if(pages>1) {
        drawText("<   >",726,594,16,sf::Color(180,220,240));
        drawText(std::to_string(statusPage_+1)+"/"+std::to_string(pages),794,598,12,sf::Color(180,220,240));
    }
    combatLogScroll_=std::clamp(combatLogScroll_,0,std::max(0,static_cast<int>(logMessages_.size())-2));
    const int first=std::max(0,static_cast<int>(logMessages_.size())-2-combatLogScroll_);
    for(int row=0;row<2 && first+row<static_cast<int>(logMessages_.size());++row) {
        const auto& text=logMessages_[first+row];
        drawText(text.size()>108?text.substr(0,105)+"...":text,10,622.f+17.f*row,12,sf::Color(205,215,227));
    }
    if(combatLogScroll_) drawText("Older",844,623,11,sf::Color(235,211,149));
    for(std::size_t slot=0;slot<9;++slot) {
        const auto index=player_.talents().hotbarIndex(talentPage_*9+slot); const float x=10+78.f*slot;
        const bool selected=index && aimingTalent_ && *index==*aimingTalent_;
        const bool hover=index && hoveredTalent_ && *index==*hoveredTalent_;
        sf::RectangleShape box({74,52}); box.setPosition({x,660});
        box.setFillColor(selected?sf::Color(83,68,30):hover?sf::Color(44,65,83):sf::Color(26,34,47));
        box.setOutlineThickness(-1); box.setOutlineColor(sf::Color(83,103,126)); window_.draw(box);
        drawText(std::to_string(slot+1),x+4,662,11,sf::Color(200,215,230));
        if(!index) { drawText("Empty",x+18,684,11,sf::Color(110,125,145)); continue; }
        const auto talent=combatTalent(player_,player_.talents().effectiveTalent(*index));
        const int cd=player_.talents().cooldownRemaining(*index);
        const bool ready=talentUnavailableReason(player_,*index).empty();
        const auto color=ready?sf::Color(117,226,219):sf::Color(125,140,160);
        // Small shape symbols drawn natively: mobility, area, buff/heal or strike.
        const std::string symbol=talent.shape==EffectShape::Movement?">>":
            talent.effectKind==TalentEffectKind::Heal?"+":talent.effectKind==TalentEffectKind::SelfBuff?"[]":
            (talent.tags&AreaTag)?"(*)":talent.projectile?"->":"/";
        drawText(symbol,x+24,662,16,color);
        drawText(std::to_string(player_.talents().rank(*index)),x+62,663,10,color);
        drawText(talent.name.size()>10?talent.name.substr(0,9)+".":talent.name,x+4,682,10,color);
        drawText(cd?"CD "+std::to_string(cd):talent.hpCost?std::to_string(talent.hpCost)+" HP":std::to_string(talent.manaCost)+" MP",x+4,696,11,
            cd?sf::Color(245,176,105):color);
    }
    for(int dir=0;dir<2;++dir) {
        sf::RectangleShape button({30,24}); button.setPosition({720.f+34*dir,660});
        button.setFillColor(sf::Color(43,56,72)); window_.draw(button);
        drawText(dir?">":"<",728.f+34*dir,660,17,sf::Color(210,230,245));
    }
    drawText("Page "+std::to_string(talentPage_+1)+"/2",795,664,12,sf::Color(180,210,225));
    drawText("PgUp/PgDn",722,695,11,sf::Color(160,180,200));
    if(player_.talents().passiveValue(PassiveKind::Deathless)) {
        const bool spent=std::find(player_.deathlessSpentFloors.begin(),player_.deathlessSpentFloors.end(),currentFloor_)!=player_.deathlessSpentFloors.end();
        drawText("Deathless",795,687,11,sf::Color(180,220,240));
        drawText(spent?"Spent":"Ready",795,700,11,spent?sf::Color(160,160,175):sf::Color(130,235,170));
    }
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
        const auto p=window_.mapPixelToCoords(wheel->position);
        if (p.x<kMapWidth && p.y>=playLayout::statusY && p.y<playLayout::statusY+playLayout::statusHeight)
            statusPage_=std::max(0,statusPage_+(wheel->delta<0?1:wheel->delta>0?-1:0));
        if (p.x<kMapWidth && p.y>=622.f && p.y<656.f)
            combatLogScroll_=std::clamp(combatLogScroll_+(wheel->delta<0?1:wheel->delta>0?-1:0),0,std::max(0,static_cast<int>(logMessages_.size())-2));
        if (p.x>=kPanelX && p.x<kPanelRight && p.y>=468.f) {
            inspectionScroll_=std::max(0,inspectionScroll_+(wheel->delta<0?3:wheel->delta>0?-3:0));
        }
    }
    if (const auto* click = event.getIf<sf::Event::MouseButtonPressed>()) {
        mousePixel_ = click->position;
        if (click->button == sf::Mouse::Button::Right) { cancelTargeting(); return; }
        if (click->button != sf::Mouse::Button::Left) return;
        const auto p=window_.mapPixelToCoords(click->position);
        if(aimingTalent_ || inspecting_) {
            if(kCancelAction.contains(p)) { cancelTargeting(); return; }
        } else for(std::size_t i=0;i<11;++i) if(dungeonActionRect(i).contains(p)) {
            switch(i) {
            case 0: openInventory(); break;
            case 1: openTalentTrees(); break;
            case 2: cancelTargeting(); codexOpen_=true; break;
            case 3: pickupItem(); break;
            case 4: player_.statusEffects().apply({StatusEffectType::Opening,2,0}); finishInventoryTurn(); break;
            case 5: startRest(); break;
            case 6: startAutoExplore(); break;
            case 7: returnToTown(); break;
            case 8:
                for(std::size_t index=0;index<player_.talents().knownTalents().size();++index)
                    if(player_.talents().knownTalents()[index].id=="basic.cleanse") { requestTalent(index); break; }
                break;
            case 9: saveGame(); break;
            case 10: loadGame(); break;
            }
            return;
        }
        if (p.y>=playLayout::hotbarY && p.y<playLayout::hotbarY+26 && p.x>=720.f && p.x<788.f) {
            changeTalentPage(p.x<754.f?-1:1); return;
        }
        if (p.y>=playLayout::statusY && p.y<playLayout::statusY+24 && p.x>=720.f && p.x<788.f) {
            statusPage_=std::max(0,statusPage_+(p.x<754.f?-1:1)); return;
        }
        if (const auto index = talentAtPixel(click->position)) { requestTalent(*index); return; }
        const auto tile = screenToWorld(click->position);
        if (!tile) return; // Clicking HUD never casts through it.
        targetCursor_ = *tile;
        if (aimingTalent_) { tryUseTalent(*aimingTalent_, targetCursor_); return; }
        if (inspecting_) return; // Explicit I/Tab inspection remains a free mode.
        if (exploredMap_.at(tile->x,tile->y)!=Visibility::Visible) {
            log("Click visible ground to move one step."); return;
        }
        if (auto* target=dynamic_cast<Monster*>(actorAt(*tile,&player_)); target && !target->allied && !target->tactics.concealed) {
            for (std::size_t i=0;i<player_.talents().knownTalents().size();++i)
                if (player_.talents().knownTalents()[i].id=="basic.attack") {
                    tryUseTalent(i,*tile); return;
                }
            return;
        }
        if (!map_.isWalkable(tile->x,tile->y)) { log("Choose visible walkable ground."); return; }
        const auto from=player_.position();
        const int dx=tile->x-from.x,dy=tile->y-from.y;
        if (!dx && !dy) return; // Clicking yourself does not wait or spend a turn.
        // Dominant-axis cardinal movement, no pathfinding or repeated movement.
        const Position step=std::abs(dx)>=std::abs(dy)?Position{dx>0?1:-1,0}:Position{0,dy>0?1:-1};
        if (const auto* enemy=dynamic_cast<const Monster*>(actorAt({from.x+step.x,from.y+step.y},&player_)); enemy && !enemy->allied && !enemy->tactics.concealed) {
            log("An enemy blocks that step. Click it to attack or select an ability."); return;
        }
        tryMovePlayer(step.x,step.y);
    }
}

void Application::drawWrapped(const std::string& text, float x, float& y,
    std::size_t columns, sf::Color color, float bottom) {
    std::istringstream words(text);
    std::string word, line;
    auto flush = [&] {
        if (y + 16.f <= bottom) drawText(line, x, y, 13, color);
        y += 17.f;
        line.clear();
    };
    while (words >> word) {
        if (!line.empty() && line.size() + word.size() + 1 > columns) flush();
        if (!line.empty()) line += ' ';
        line += word;
    }
    if (!line.empty()) flush();
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

void Application::renderTargetingPanel() {
    sf::RectangleShape panel({384.f, 720.f});
    panel.setPosition({896.f, 0.f}); panel.setFillColor(sf::Color(18, 22, 31));
    window_.draw(panel);
    const auto& talents = player_.talents().knownTalents();
    const std::size_t pages = 2;
    talentPage_ = std::min(talentPage_, pages - 1);
    drawText("DETAILS",kPanelX,10.f,17,sf::Color(110,220,220));
    drawText("Hover a talent or status below the map",kPanelX,30.f,12,sf::Color(175,185,205));
    const auto selected = aimingTalent_ ? aimingTalent_ : hoveredTalent_;
    float y = 55.f;
    const sf::Color normal(210, 220, 235), accent(110, 220, 220);
    const auto status=hoveredStatus();
    std::optional<std::string> hoveredLog;
    if (mousePixel_) {
        const auto mouse=window_.mapPixelToCoords(*mousePixel_);
        if(mouse.x>=10 && mouse.x<kMapWidth && mouse.y>=622 && mouse.y<656 && !logMessages_.empty()) {
            const int first=std::max(0,static_cast<int>(logMessages_.size())-2-combatLogScroll_);
            const int index=first+static_cast<int>((mouse.y-622)/17);
            if(index<static_cast<int>(logMessages_.size())) hoveredLog=logMessages_[index];
        }
    }
    if (status) {
        drawWrapped(statusName(status->type),kPanelX,y,43,accent,463.f);
        drawWrapped(status->type==StatusEffectType::BattleRhythm?"Ready until used":"Remaining: "+std::to_string(status->turnsRemaining)+" status ticks",kPanelX,y,43,normal,463.f);
        drawWrapped(statusTooltip(*status),kPanelX,y,43,normal,463.f);
        if (isCleansable(status->type)) drawWrapped("C: Cleanse removes this effect.",kPanelX,y,43,accent,463.f);
    } else if (hoveredLog) {
        drawWrapped("COMBAT LOG",kPanelX,y,43,accent,463.f);
        drawWrapped(*hoveredLog,kPanelX,y,43,normal,463.f);
    } else if (selected && *selected < talents.size()) {
        const auto talent = combatTalent(player_,player_.talents().effectiveTalent(*selected));
        drawWrapped(talent.name+" ["+std::to_string(player_.talents().rank(*selected))+"/3]", kPanelX, y, 43, accent, 463.f);
        drawWrapped(talent.description, kPanelX, y, 43, normal, 463.f);
        drawWrapped("Cost: " + std::to_string(talent.manaCost) + " mana, " +
            std::to_string(talent.hpCost) + " HP | CD: " + std::to_string(talent.cooldownTurns) +
            " turns", kPanelX, y, 43, normal, 463.f);
        const std::string rule = talent.shape == EffectShape::Movement ?
            "Movement: up to " + std::to_string(talent.moveDistance) + " tiles; stops at blockers." :
            talent.projectile ? "Aim at visible ground; projectile stops at the first enemy." :
            talent.targeting == TargetingMode::AdjacentEnemy ? "Aim at adjacent ground (no diagonals)." :
            talent.targeting == TargetingMode::Self ? "Centered on yourself." : "Range: any visible ground tile (direct spell).";
        drawWrapped(rule, kPanelX, y, 43, normal, 463.f);
        if (talent.tags & AreaTag) drawWrapped("Radius: " + std::to_string(talent.areaRadius), kPanelX, y, 43, normal, 463.f);
        const auto unavailable = talentUnavailableReason(player_, *selected);
        if (!unavailable.empty()) drawWrapped(unavailable, kPanelX, y, 43, sf::Color(255, 130, 110), 463.f);
        if (talent.movementBurn && (talent.shape==EffectShape::Movement || talent.retreatDistance))
            drawWrapped("Kindle: landing burns adjacent visible enemies for "+std::to_string(talent.movementBurn)+" per turn, 2 turns; reveals you.",kPanelX,y,43,accent,463.f);
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
                drawWrapped("On hit: " + std::to_string(low) + "-" + std::to_string(high) +
                    " damage (includes crit). Can miss. Targets: " +
                    std::to_string(preview.affected.size()), kPanelX, y, 43, accent, 463.f);
            }
            if (!preview.message.empty()) drawWrapped(preview.message, kPanelX, y, 43,
                preview.valid ? accent : sf::Color(255, 130, 110), 463.f);
        }
    } else {
        drawWrapped("Hover a talent for details. Select a talent to aim; self buffs and healing cast immediately.",
            kPanelX, y, 43, normal, 463.f);
    }

    y = 468.f;
    drawWrapped("ENEMY INSPECTION - wheel to scroll", kPanelX, y, 43, accent, 712.f);
    std::optional<Position> inspectTile;
    if (aimingTalent_ || inspecting_) inspectTile = targetCursor_;
    else if (mousePixel_) {
        inspectTile=screenToWorld(*mousePixel_);
        // Keep the last hovered enemy while moving into its inspection panel.
        if (!inspectTile) inspectTile=inspectionAnchor_;
    }
    if (!inspectTile || !inspectionAnchor_ || !same(*inspectTile,*inspectionAnchor_)) inspectionScroll_=0;
    inspectionAnchor_=inspectTile;
    std::vector<std::string> details;
    if (inspectTile) {
        for (const auto& monster:monsters_) {
            if (!same(monster->position(),*inspectTile)) continue;
            const auto lines=inspectMonster(*monster,exploredMap_);
            if (lines.empty()) continue; // dead or hidden actors never disclose information
            if (monster->allied) {
                details.push_back("ALLIED SKELETON - "+std::to_string(monster->stats().hp)+"/"+std::to_string(monster->stats().maxHp)+" HP. Walk into it to swap.");
                details.push_back(monster->remainingLife?"Expires in "+std::to_string(monster->remainingLife)+" actions.":"Permanent until death or travel.");
            } else {
                if (player_.statusEffects().has(StatusEffectType::Concealed)) {
                    const float chance=enemyStealthDetectionChance(*monster);
                    details.push_back(chance>0.f ? "Stealth detection: "+std::to_string(static_cast<int>(std::lround(chance*100.f)))+
                        "% on next active turn. Detection breaks concealment.":"No stealth check from here (outside detection range or sight).");
                }
                details.insert(details.end(),lines.begin(),lines.end());
            }
            break;
        }
    }
    if (details.empty()) {
        inspectionScroll_=0;
        details.push_back("Hover a visible enemy for details. I / Tab: free inspection mode. Left-click: adjacent basic attack; ground: one step.");
    }
    std::vector<std::string> wrapped;
    for (const auto& detail:details) {
        std::istringstream words(detail); std::string word,line;
        while(words>>word) {
            if (!line.empty() && line.size()+word.size()+1>43) { wrapped.push_back(line); line.clear(); }
            if (!line.empty()) line+=' ';
            line+=word;
        }
        if (!line.empty()) wrapped.push_back(line);
    }
    constexpr int visibleLines=12;
    inspectionScroll_=std::clamp(inspectionScroll_,0,std::max(0,static_cast<int>(wrapped.size())-visibleLines));
    for(int i=0;i<visibleLines && inspectionScroll_+i<static_cast<int>(wrapped.size());++i)
        drawText(wrapped[inspectionScroll_+i],kPanelX,489.f+17.f*i,13,normal);
    if (wrapped.size()>visibleLines)
        drawText("Lines "+std::to_string(inspectionScroll_+1)+"-"+
            std::to_string(std::min(inspectionScroll_+visibleLines,static_cast<int>(wrapped.size())))+
            "/"+std::to_string(wrapped.size())+" | scroll here for more",kPanelX,699.f,12,accent);

    if(autoExploring_ || restTurns_>0 || aimingTalent_ || inspecting_) {
    drawText(autoExploring_ ? "AUTO-EXPLORE: any key/click stops. Pauses for danger and discoveries." :
        restTurns_>0 ? "RESTING: any key/click stops. Recovering HP, mana and cooldowns." :
        aimingTalent_ ? "AIM: mouse / arrows  Tab: target  Enter / click: cast  Esc / right-click: cancel" :
        inspecting_ ? "INSPECT: mouse / arrows  Tab: enemy  I / Esc / right-click: close" :
        "WASD move  Space wait  Z explore  R rest  H town  1-9 cast  C cleanse  B bag  T trees  J codex  G use  F5/F9 save/load",
        10.f, 68.f, 12, sf::Color(180, 210, 220));
        sf::RectangleShape button(kCancelAction.size); button.setPosition(kCancelAction.position);
        button.setFillColor(sf::Color(45,65,80)); window_.draw(button);
        drawText(autoExploring_ || restTurns_>0?"Stop":"Cancel",805,67,12,sf::Color::White);
    } else for(std::size_t i=0;i<11;++i) {
        const auto rect=dungeonActionRect(i);
        sf::RectangleShape button(rect.size); button.setPosition(rect.position);
        button.setFillColor(mousePixel_ && rect.contains(sf::Vector2f(*mousePixel_))?sf::Color(45,70,85):sf::Color(26,40,54));
        window_.draw(button); drawText(kDungeonActions[i],rect.position.x+4,68,11,sf::Color(200,225,235));
    }
}
} // namespace engine
