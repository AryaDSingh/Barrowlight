#include "core/Application.hpp"
#include <queue>
#include <limits>

namespace engine {
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
    if (!monster.rewardsEligible()) return;
    const int quality = boss ? 2 : static_cast<int>(monster.tier());
    const int count = boss ? 2 : 1;
    if (!boss && (ordinaryDrops_ >= 2 || loot_.roll(100) >= static_cast<unsigned>(35 + quality * 15))) return;
    if (boss) giveRune(kRunes[loot_.roll(4)].id);
    for (int i = 0; i < count && nextItemId_ < std::numeric_limits<std::uint64_t>::max(); ++i) {
        auto item = loot_.generate(currentFloor_, quality, nextItemId_++, monster.position(),
                                   boss ? ItemRarity::Rare : ItemRarity::Normal);
        if (boss) {
            log("Boss reward: ", item->name(), " (in your bag).");
            player_.inventory().add(std::move(item)); // final boss rewards remain owned after victory
        } else {
            ++ordinaryDrops_;
            log(monster.name(), " drops ", item->name(), ".");
            groundItems_.push_back(std::move(item));
        }
    }
}
}
