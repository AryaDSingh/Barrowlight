#include "core/Application.hpp"
#include "entities/MonsterFactory.hpp"

#include <algorithm>
#include <random>

namespace engine {

// Nemeses: leave a floor while a foe that hurt you badly still hunts you, and
// it remembers. It takes a name, follows you down, and comes for you on the
// next new floor below; flee it again and it grows bolder. Kill it and it's over.

namespace {
const char* const kNemesisNames[]{"Grub", "Snikkit", "Vosk", "Murga", "Old Rattle", "Kessa", "Brannoch", "Ulf", "Hollis", "Mirel", "Toadmouth", "Skarn"};
const char* const kNemesisTitles[]{"the Torchbreaker", "the Unbowed", "Who Follows", "the Patient", "Bloodtooth", "the Grudge-Keeper",
                                   "Ashjaw", "the Long Shadow", "Nightgrin", "the Unforgiving"};
}

// Leaving a floor: the worst of what still hunts you remembers you.
void Application::rememberFoe() {
    if (trial_ || labRun_) return;
    // Your nemesis, if it's here, follows you; fleeing it again makes it bolder.
    for (auto& m : monsters_)
        if (m->roam == Roam::Nemesis && m->stats().hp > 0) {
            if (m->tactics.alert > 0 && player_.nemesis.rank < 3) {
                ++player_.nemesis.rank;
                log("Behind you, ", m->name(), " roars. It is not done with you.");
            }
            player_.nemesis.depth = floorDepth(currentFloor_);
            m->stats().hp = 0; scheduler_.remove(*m);
            removeDeadMonsters();
            return;
        }
    if (player_.nemesis.type >= 0) return; // one grudge at a time
    Monster* worst = nullptr;
    const int badly = std::max(5, player_.stats().maxHp / 6);
    for (auto& m : monsters_) {
        if (m->allied || m->stats().hp <= 0 || m->tactics.alert <= 0 || m.get() == boss_ || m->eventChampion ||
            isUniqueMonster(m->type()) || m->harmToPlayer < badly) continue;
        if (!worst || m->harmToPlayer > worst->harmToPlayer) worst = m.get();
    }
    if (!worst) return;
    std::mt19937 rng(static_cast<unsigned>(static_cast<int>(worst->type()) * 7919 + currentFloor_ * 104729 + floorTurns_));
    const std::string name = std::string(kNemesisNames[rng() % std::size(kNemesisNames)]) + " " + kNemesisTitles[rng() % std::size(kNemesisTitles)];
    player_.nemesis = {static_cast<int>(worst->type()), name, floorDepth(currentFloor_), 1};
    log("Behind you, the ", plainName(*worst), " howls after you. It will remember you, and you will hear of ", name, ".");
    worst->stats().hp = 0; scheduler_.remove(*worst); // it follows you
    removeDeadMonsters();
}

// A new floor deeper than where you fled it: your nemesis is here, and hunting.
void Application::spawnNemesis() {
    const auto& n = player_.nemesis;
    if (n.type < 0 || trial_ || labRun_ || boss_ || floorDepth(currentFloor_) <= n.depth) return;
    if (std::any_of(monsters_.begin(), monsters_.end(), [](const auto& m) { return m->roam == Roam::Nemesis && m->stats().hp > 0; })) return;
    const auto me = player_.position();
    std::vector<Position> spots;
    for (int y = 0; y < map_.height(); ++y)
        for (int x = 0; x < map_.width(); ++x) {
            const Position p{x, y};
            if (map_.isWalkable(x, y) && !isOccupied(p, nullptr) && std::max(std::abs(x - me.x), std::abs(y - me.y)) >= 12 &&
                exploredMap_.at(x, y) != Visibility::Visible && !(vaultExists_ && std::max(std::abs(x - vaultCenter_.x), std::abs(y - vaultCenter_.y)) <= 4))
                spots.push_back(p);
        }
    if (spots.empty()) return;
    const auto at = spots[static_cast<std::size_t>(currentFloor_ * 2654435761u % spots.size())];
    auto made = createMonster(static_cast<MonsterType>(n.type), at, MonsterTier::Nightmare);
    scaleDungeonMonster(*made, floorDepth(currentFloor_));
    made->stats().maxHp = made->stats().maxHp * (150 + 25 * (n.rank - 1)) / 100;
    made->stats().hp = made->stats().maxHp;
    made->stats().strength += 2 * n.rank;
    made->lastObservedHp = made->stats().hp;
    made->roam = Roam::Nemesis;
    made->setName(n.name);
    made->tactics.alert = 8; made->tactics.lastKnown = me; made->tactics.concealed = false;
    scheduler_.add(*made);
    monsters_.push_back(std::move(made));
}

} // namespace engine
