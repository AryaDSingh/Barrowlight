#include "core/Application.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <random>

#include "entities/MonsterFactory.hpp"
#include "world/FloorTheme.hpp"
#include "world/Pathfinder.hpp"

namespace engine {

// Roaming threats: a patrol that walks the whole floor, a wandering champion
// with its escort, and hunters that come for you if you linger.

namespace {
constexpr float kPatrolChance = 0.6f;
constexpr float kChampionChance = 0.35f;
constexpr int kChampionDepth = 3;
// The hunt: a warning, then hunters; again every kHuntEvery turns after.
constexpr int kFirstHunt = 900, kHuntWarning = 100, kHuntEvery = 400;
int chebyshev(Position a, Position b) { return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)); }
int manhattan(Position a, Position b) { return std::abs(a.x - b.x) + std::abs(a.y - b.y); }
} // namespace

std::string Application::wandererName(const Monster& monster) { return plainName(monster) + ", the Wanderer"; }

// The walkable tile nearest the middle of each of the nine module cells:
// the rooms a roamer walks between. Never inside the sealed vault.
std::vector<Position> Application::roamWaypoints() const {
    std::vector<Position> points;
    const int cw = map_.width() / 3, ch = map_.height() / 3;
    for (int cy = 0; cy < 3; ++cy)
        for (int cx = 0; cx < 3; ++cx) {
            const Position mid{cx * cw + cw / 2, cy * ch + ch / 2};
            bool found = false;
            for (int r = 0; r <= std::max(cw, ch) / 2 && !found; ++r)
                for (int dy = -r; dy <= r && !found; ++dy)
                    for (int dx = -r; dx <= r && !found; ++dx) {
                        const Position p{mid.x + dx, mid.y + dy};
                        if (std::max(std::abs(dx), std::abs(dy)) != r || !map_.isWalkable(p.x, p.y)) continue;
                        if (vaultExists_ && chebyshev(p, vaultCenter_) <= 4) continue;
                        points.push_back(p); found = true;
                    }
        }
    return points;
}

// A step toward goal that goes round the other actors in the way, if the
// detour isn't much longer.
std::optional<Position> Application::flankStep(const Monster& monster, Position goal) {
    Map blocked = map_;
    const auto block = [&](Position p) {
        if (p.x != goal.x || p.y != goal.y) blocked.setTile(p.x, p.y, Tile{TileType::Wall, false, true});
    };
    for (const auto& m : monsters_) if (m.get() != &monster && m->stats().hp > 0) block(m->position());
    block(player_.position());
    const auto around = findPath(blocked, monster.position(), goal);
    const auto direct = findPath(map_, monster.position(), goal);
    if (!around || around->size() < 2 || (direct && around->size() > direct->size() + 8)) return std::nullopt;
    return (*around)[1];
}

// Room to room. Every member of a pack picks the same next room, so they
// stay together.
AIDecision Application::roamStep(Monster& m) {
    auto& t = m.tactics;
    const auto points = roamWaypoints();
    if (points.empty()) return {};
    const int n = static_cast<int>(points.size());
    const auto next = [&](int i) { return n < 2 ? i : (i + 1 + (i * 5 + currentFloor_) % (n - 1)) % n; };
    t.patrol %= n;
    const Position here = m.position();
    if (manhattan(here, points[static_cast<std::size_t>(t.patrol)]) <= 2) t.patrol = next(t.patrol);
    const Position goal = points[static_cast<std::size_t>(t.patrol)];
    const auto path = findPath(map_, here, goal);
    if (!path) { t.patrol = next(t.patrol); return {}; }
    AIDecision d;
    if (path->size() > 1) {
        if (!isOccupied((*path)[1], &m)) { d.type = AIActionType::Move; d.movePosition = (*path)[1]; }
        else if (const auto around = flankStep(m, goal)) { d.type = AIActionType::Move; d.movePosition = *around; }
    }
    return d;
}

// On a new floor: maybe a patrol, and from depth 3 maybe a wandering champion.
void Application::planRoamers(unsigned seed) {
    std::mt19937 rng(seed ^ 0x5eed7a11u);
    std::uniform_real_distribution<float> roll(0.f, 1.f);
    const auto points = roamWaypoints();
    if (points.size() < 3) return;
    const Position start = player_.position();
    const auto nearestPoint = [&](Position p) {
        int best = 0;
        for (int i = 1; i < static_cast<int>(points.size()); ++i)
            if (manhattan(p, points[static_cast<std::size_t>(i)]) < manhattan(p, points[static_cast<std::size_t>(best)])) best = i;
        return best;
    };
    const auto ordinary = [&](const Monster& m) {
        return m.stats().hp > 0 && !m.allied && !m.vaultGuard && !m.eventChampion && &m != boss_ &&
               !isUniqueMonster(m.type()) && m.roam == Roam::None;
    };

    // A patrol: one pack leaves its post and walks the floor.
    if (roll(rng) < kPatrolChance) {
        std::vector<Monster*> leaders;
        for (auto& m : monsters_) if (ordinary(*m) && chebyshev(m->position(), start) > 15) leaders.push_back(m.get());
        if (!leaders.empty()) {
            const Position post = leaders[std::uniform_int_distribution<std::size_t>(0, leaders.size() - 1)(rng)]->position();
            const int route = nearestPoint(post);
            for (auto& m : monsters_)
                if (ordinary(*m) && chebyshev(m->position(), post) <= 2) { m->roam = Roam::Patrol; m->tactics.patrol = route; }
        }
    }

    // A wandering champion and its escort, starting as far from you as it can.
    if (floorDepth(currentFloor_) < kChampionDepth || roll(rng) >= kChampionChance) return;
    std::array<MonsterType, 3> band{MonsterType::Ogre, MonsterType::GoblinRaider, MonsterType::Shaman};
    if (cathedralFloor(currentFloor_)) band = {MonsterType::DeepLurker, MonsterType::DrownedOne, MonsterType::DrownedChorister};
    else if (foundryFloor(currentFloor_)) band = {MonsterType::SlagGolem, MonsterType::OrcSmith, MonsterType::BellowsImp};
    else if (floorTheme(currentFloor_).region == FloorRegion::Crypts) band = {MonsterType::CryptSentinel, MonsterType::Skeleton, MonsterType::Bonecaller};
    int route = 0;
    for (int i = 1; i < static_cast<int>(points.size()); ++i)
        if (manhattan(start, points[static_cast<std::size_t>(i)]) > manhattan(start, points[static_cast<std::size_t>(route)])) route = i;
    const Position lair = points[static_cast<std::size_t>(route)];
    if (chebyshev(lair, start) <= 15) return;
    std::vector<Position> spots;
    for (int r = 0; r <= 2 && spots.size() < band.size(); ++r)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                const Position p{lair.x + dx, lair.y + dy};
                if (spots.size() >= band.size() || std::max(std::abs(dx), std::abs(dy)) != r) continue;
                if (map_.isWalkable(p.x, p.y) && !isOccupied(p, nullptr)) spots.push_back(p);
            }
    if (spots.size() < band.size()) return;
    for (std::size_t i = 0; i < band.size(); ++i) {
        auto m = createMonster(band[i], spots[i], i ? MonsterTier::Elite : MonsterTier::Nightmare);
        scaleDungeonMonster(*m, floorDepth(currentFloor_));
        m->tactics.patrol = route;
        if (i == 0) {
            m->roam = Roam::Champion;
            m->stats().maxHp = m->stats().maxHp * 3 / 2; m->stats().hp = m->stats().maxHp;
            m->setName(wandererName(*m));
        } else {
            m->roam = Roam::Patrol;
        }
        monsters_.push_back(std::move(m));
    }
}

// Each turn on a floor brings the hunt a little closer.
void Application::tickHunt() {
    if (mode_ != GameMode::Playing || trial_ || player_.stats().hp <= 0) return;
    ++floorTurns_;
    const int warn = floorTurns_ - (kFirstHunt - kHuntWarning), hunt = floorTurns_ - kFirstHunt;
    if (warn >= 0 && warn % kHuntEvery == 0) log("You feel watched.");
    if (hunt >= 0 && hunt % kHuntEvery == 0) spawnHunters();
}

// Two packs, from opposite sides of you and out of sight. They always know
// where you are, and they carry nothing.
void Application::spawnHunters() {
    const Position me = player_.position();
    std::array<MonsterType, 3> pack{MonsterType::GoblinRaider, MonsterType::GoblinStalker, MonsterType::Goblin};
    if (cathedralFloor(currentFloor_)) pack = {MonsterType::DeepLurker, MonsterType::DrownedOne, MonsterType::DeepLurker};
    else if (foundryFloor(currentFloor_)) pack = {MonsterType::OrcSmith, MonsterType::Slagling, MonsterType::OrcSmith};
    else if (floorTheme(currentFloor_).region == FloorRegion::Crypts) pack = {MonsterType::Skeleton, MonsterType::CryptShade, MonsterType::SkeletonGuard};
    const MonsterTier tier = floorDepth(currentFloor_) >= 4 ? MonsterTier::Elite : MonsterTier::Base;
    std::mt19937 rng(static_cast<unsigned>(floorTurns_ * 2654435761u) ^ static_cast<unsigned>(me.x * 73 + me.y));
    const float angle = std::uniform_real_distribution<float>(0.f, 6.2831853f)(rng);
    int spawned = 0;
    for (const float side : {angle, angle + 3.1415927f}) {
        // The hidden tile at a good distance that lies most nearly that way.
        const float ux = std::cos(side), uy = std::sin(side);
        std::optional<Position> lair;
        float best = -2.f;
        for (int y = 0; y < map_.height(); ++y)
            for (int x = 0; x < map_.width(); ++x) {
                const Position p{x, y};
                const int d = chebyshev(p, me);
                if (d < 12 || d > 30 || !map_.isWalkable(x, y) || exploredMap_.at(x, y) == Visibility::Visible || isOccupied(p, nullptr)) continue;
                if (vaultExists_ && chebyshev(p, vaultCenter_) <= 4) continue;
                const float len = std::sqrt(static_cast<float>((x - me.x) * (x - me.x) + (y - me.y) * (y - me.y)));
                const float facing = ((x - me.x) * ux + (y - me.y) * uy) / len;
                if (facing > best) { best = facing; lair = p; }
            }
        if (!lair || !findPath(map_, *lair, me)) continue;
        std::size_t placed = 0;
        for (int r = 0; r <= 2 && placed < pack.size(); ++r)
            for (int dy = -r; dy <= r && placed < pack.size(); ++dy)
                for (int dx = -r; dx <= r && placed < pack.size(); ++dx) {
                    const Position p{lair->x + dx, lair->y + dy};
                    if (std::max(std::abs(dx), std::abs(dy)) != r || !map_.isWalkable(p.x, p.y) || isOccupied(p, nullptr) ||
                        exploredMap_.at(p.x, p.y) == Visibility::Visible) continue;
                    auto m = createMonster(pack[placed], p, tier);
                    scaleDungeonMonster(*m, floorDepth(currentFloor_));
                    m->roam = Roam::Hunter;
                    m->setRewardsEligible(false);
                    m->recoveryActions = 1;
                    m->tactics.alert = 8; m->tactics.lastKnown = me;
                    m->tactics.concealed = false;
                    scheduler_.add(*m);
                    monsters_.push_back(std::move(m));
                    ++placed; ++spawned;
                }
    }
    if (spawned) log("Footsteps, closing in.");
}

} // namespace engine
