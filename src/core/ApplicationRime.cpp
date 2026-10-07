#include "core/Application.hpp"
#include "entities/Lore.hpp"
#include "entities/MonsterFactory.hpp"

#include <algorithm>

namespace engine {

// Rimeholt: the first path beyond the Lich. Its cold freezes standing water
// and creeps the ice across the floor; the Winter King raises his court.

// Each of your turns on a Rimeholt floor: water freezes, and the ice spreads a
// little, until a fifth of the floor is ice.
void Application::tickCold() {
    if (!rimeFloor(currentFloor_) || trial_ || surfaces_.empty()) return;
    std::vector<Position> ice;
    int open = 0;
    for (int y = 0; y < map_.height(); ++y)
        for (int x = 0; x < map_.width(); ++x) {
            if (!map_.isWalkable(x, y)) continue;
            ++open;
            const auto s = surfaceAt({x, y});
            if (s == SurfaceType::Water) setSurface({x, y}, SurfaceType::Ice, 0);
            if (s == SurfaceType::Water || s == SurfaceType::Ice) ice.push_back({x, y});
        }
    if (ice.empty() || static_cast<int>(ice.size()) * 5 >= open) return;
    const auto me = player_.position();
    for (int k = 0; k < 2; ++k) {
        const auto from = ice[static_cast<std::size_t>((floorTurns_ * 7 + k * 13) % static_cast<int>(ice.size()))];
        for (const Position n : {Position{from.x + 1, from.y}, Position{from.x, from.y + 1}, Position{from.x - 1, from.y}, Position{from.x, from.y - 1}})
            if (map_.isWalkable(n.x, n.y) && surfaceAt(n) == SurfaceType::None && !(n.x == me.x && n.y == me.y)) { setSurface(n, SurfaceType::Ice, 0); break; }
    }
}

// The Winter King, at half his life: three of his court climb out of the ice.
void Application::callCourt(Monster& king) {
    courtCalled_ = true;
    const auto at = king.position();
    int raised = 0;
    for (int r = 1; r <= 3 && raised < 3; ++r)
        for (int dy = -r; dy <= r && raised < 3; ++dy)
            for (int dx = -r; dx <= r && raised < 3; ++dx) {
                const Position p{at.x + dx, at.y + dy};
                if (std::max(std::abs(dx), std::abs(dy)) != r || !map_.isWalkable(p.x, p.y) || isOccupied(p, nullptr)) continue;
                auto made = createMonster(MonsterType::RimeWight, p, MonsterTier::Elite);
                scaleDungeonMonster(*made, floorDepth(currentFloor_));
                made->setRewardsEligible(false);
                made->tactics.alert = 8; made->tactics.lastKnown = player_.position(); made->voicedAlert = true;
                setSurface(p, SurfaceType::Ice, 12);
                scheduler_.add(*made);
                monsters_.push_back(std::move(made));
                ++raised;
            }
    log("The Winter King raises his hand, and his court climbs out of the ice!");
}

// After the Lich: the victory stands, but the run goes on. A road north opens.
void Application::goOn() {
    if (!wonGame_) return;
    wonGame_ = false; pendingFinalVictory_ = false;
    mode_ = GameMode::Playing;
    if (!player_.knowsLore("winter_road")) {
        player_.lore.push_back("winter_road");
        if (const auto* entry = loreEntry("winter_road")) for (const char* line : entry->text) log(line);
        log("It leads to Rimeholt. Choose it from the dungeon menu in town.");
    }
}

// Gravecold: one of the frozen dead rises to fight for you (turns 0: until it falls).
Monster* Application::raiseFrozenDead(MonsterType kind, Position at, int turns) {
    auto made = createMonster(kind, at);
    const int mind = player_.stats().intelligence;
    configureMinion(*made, 2, mind);
    if (kind == MonsterType::FrozenThrall) { made->stats().maxHp = 40 + 2 * mind; made->stats().strength += 4; made->stats().speed = 70; }
    else { made->stats().maxHp = 24 + mind; made->stats().strength += 2; }
    made->stats().hp = made->stats().maxHp;
    made->remainingLife = turns ? turns + 1 : 0;
    made->lastObservedHp = made->stats().hp;
    auto* risen = made.get();
    scheduler_.add(*risen);
    monsters_.push_back(std::move(made));
    return risen;
}

} // namespace engine
