#include "core/Application.hpp"

#include <algorithm>
#include <array>
#include <queue>

namespace engine {
namespace {
constexpr std::array<Position,4> directions{{{0,-1},{1,0},{0,1},{-1,0}}};
bool sameExploreTile(Position a, Position b) { return a.x==b.x && a.y==b.y; }

// Search known walkable terrain only. An unseen neighbour is a frontier,
// never a reason to inspect the hidden map or plan through an unseen room.
std::optional<Position> exploreStep(const Map& map, const ExploredMap& vision, Position start) {
    struct Node { Position tile, firstStep; };
    std::queue<Node> frontier;
    std::vector<bool> seen(static_cast<std::size_t>(map.width())*map.height());
    seen[static_cast<std::size_t>(start.y)*map.width()+start.x]=true;
    frontier.push({start,start});
    while (!frontier.empty()) {
        const auto node=frontier.front(); frontier.pop();
        for (const auto d:directions) {
            const Position next{node.tile.x+d.x,node.tile.y+d.y};
            if (next.x<0 || next.y<0 || next.x>=map.width() || next.y>=map.height()) continue;
            const auto index=static_cast<std::size_t>(next.y)*map.width()+next.x;
            if (seen[index]) continue;
            const Position first=sameExploreTile(node.tile,start)?next:node.firstStep;
            if (vision.at(next.x,next.y)==Visibility::Hidden) return first;
            seen[index]=true;
            if (map.isWalkable(next.x,next.y)) frontier.push({next,first});
        }
    }
    return std::nullopt;
}
}

std::vector<std::string> Application::visibleExploreInterests() const {
    std::vector<std::string> result;
    const auto add=[&](const char* kind,Position p) {
        if (exploredMap_.at(p.x,p.y)==Visibility::Visible)
            result.push_back(std::string(kind)+":"+std::to_string(p.x)+":"+std::to_string(p.y));
    };
    for (const auto& item:groundItems_)
        if (exploredMap_.at(item->position().x,item->position().y)==Visibility::Visible)
            result.push_back("item:"+std::to_string(item->instanceId()));
    if (chestExists_ && !chestClaimed_) add("chest",chestPosition_);
    if (vaultExists_ && !vaultClaimed_) {
        add("vault",vaultEntrance_);
        if (vaultOpened_) add("cache",vaultCenter_);
    }
    add("stairs",floorEntrance_);
    if (map_.isWalkable(floorExit_.x,floorExit_.y)) add("stairs",floorExit_);
    for (int y=0;y<map_.height();++y) for (int x=0;x<map_.width();++x)
        if (exploredMap_.at(x,y)==Visibility::Visible && map_.tileAt(x,y).type==TileType::Door)
            add("door",{x,y});
    return result;
}

void Application::stopAutoExplore(const char* reason) {
    if (!autoExploring_) return;
    autoExploring_=false;
    log("Auto-explore stopped: ",reason);
}

void Application::startAutoExplore() {
    if (mode_!=GameMode::Playing || inventoryOpen_ || vaultMenu_ || exitMenu_ || codexOpen_) return;
    if (dangerNearby() || combatThisTurn_) {
        log("Cannot auto-explore: enemies, attack warnings or harmful effects are present.");
        return;
    }
    if (!exploreStep(map_,exploredMap_,player_.position())) {
        log("No reachable unexplored area. Check stairs or sealed vaults manually.");
        return;
    }
    cancelTargeting(); restTurns_=0;
    // Explicit restart acknowledges currently visible discoveries. No looting,
    // opening, floor transition or ability use is performed by automation.
    exploreSeenInterests_=visibleExploreInterests();
    exploreStepsLeft_=map_.width()*map_.height()*4;
    autoExploring_=true; exploreClock_.restart();
    log("Auto-exploring. Stops for danger or new discoveries; any key/click cancels.");
}

void Application::stepAutoExplore() {
    if (!window_.isOpen() || mode_!=GameMode::Playing || codexOpen_ || inventoryOpen_ || vaultMenu_ || exitMenu_) {
        stopAutoExplore("another screen opened."); return;
    }
    if (dangerNearby() || combatThisTurn_) { stopAutoExplore("danger detected."); return; }
    const auto newInterest=[&]() {
        const auto visible=visibleExploreInterests();
        return std::any_of(visible.begin(),visible.end(),[&](const auto& key) {
            return std::find(exploreSeenInterests_.begin(),exploreSeenInterests_.end(),key)==exploreSeenInterests_.end();
        });
    };
    if (newInterest()) { stopAutoExplore("loot, a chest, stairs or a vault discovered. Z continues."); return; }
    if (exploreClock_.getElapsedTime().asMilliseconds()<100) return;
    exploreClock_.restart();
    if (exploreStepsLeft_--<=0) { stopAutoExplore("step limit reached. Continue manually or press Z again."); return; }
    const auto step=exploreStep(map_,exploredMap_,player_.position());
    if (!step) { stopAutoExplore("no reachable unexplored area. Check stairs or sealed vaults manually."); return; }
    // tryMovePlayer normally bump-attacks. Never permit that during exploration;
    // friendly skeletons can still swap places through the normal movement path.
    if (auto* blocker=actorAt(*step,&player_)) {
        const auto* ally=dynamic_cast<const Monster*>(blocker);
        if (!ally || !ally->allied) { stopAutoExplore("the route is occupied."); return; }
    }
    const auto before=player_.position();
    const int hp=player_.stats().hp;
    if (!tryMovePlayer(step->x-before.x,step->y-before.y)) { stopAutoExplore("the route is blocked."); return; }
    if (!autoExploring_) return; // combat may already have interrupted this turn
    if (player_.stats().hp<hp || dangerNearby() || mode_!=GameMode::Playing) {
        stopAutoExplore("damage, danger or a character choice needs your attention."); return;
    }
    const auto p=player_.position();
    const bool onItem=std::any_of(groundItems_.begin(),groundItems_.end(),[&](const auto& item) {
        return sameExploreTile(item->position(),p);
    });
    if (newInterest() || onItem || (chestExists_ && !chestClaimed_ && sameExploreTile(p,chestPosition_)) ||
        sameExploreTile(p,floorEntrance_) || sameExploreTile(p,floorExit_) || map_.tileAt(p.x,p.y).type==TileType::Door ||
        (vaultExists_ && !vaultClaimed_ && (sameExploreTile(p,vaultEntrance_) || sameExploreTile(p,vaultCenter_))))
        stopAutoExplore("loot, a chest, stairs or a vault needs your attention. Z continues.");
}
} // namespace engine
