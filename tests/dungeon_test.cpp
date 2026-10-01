// Standalone sanity check for generateDungeon and the module pool. No
// SFML, no window -- same pattern as the other tests. Checks every
// module's authoring rules (so a broken new module fails here with a
// readable message), then generates many floors and verifies the
// properties the rest of the game relies on: full connectivity, a sealed
// vault, a clear boss arena, distinct modules, and safe start spacing.
// Also prints one example layout for eyeballing.

#include <cstdlib>
#include <iostream>
#include <queue>
#include <set>
#include <vector>

#include "world/DungeonGenerator.hpp"
#include "world/DungeonModules.hpp"

using namespace engine;

namespace {

int failures = 0;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::cout << "[FAIL] " << message << '\n';
        ++failures;
    }
}

std::vector<bool> reachableFrom(const Map& map, Position start) {
    std::vector<bool> seen(static_cast<std::size_t>(map.width() * map.height()), false);
    std::queue<Position> frontier;
    frontier.push(start);
    seen[static_cast<std::size_t>(start.y * map.width() + start.x)] = true;
    while (!frontier.empty()) {
        const Position p = frontier.front();
        frontier.pop();
        for (const Position d : {Position{1, 0}, Position{-1, 0}, Position{0, 1}, Position{0, -1}}) {
            const Position n{p.x + d.x, p.y + d.y};
            if (!map.isWalkable(n.x, n.y)) continue;
            const auto index = static_cast<std::size_t>(n.y * map.width() + n.x);
            if (!seen[index]) {
                seen[index] = true;
                frontier.push(n);
            }
        }
    }
    return seen;
}

void checkFloor(const GeneratedDungeon& d, const DungeonGenerationParams& params, unsigned seed) {
    const std::string where = " (seed " + std::to_string(seed) + ")";
    const Map& map = d.map;
    const auto reached = reachableFrom(map, d.playerStart);
    const auto inVault = [&](int x, int y) {
        return d.hasVault && std::abs(x - d.vaultCenter.x) <= 2 && std::abs(y - d.vaultCenter.y) <= 2;
    };
    for (int y = 0; y < map.height(); ++y)
        for (int x = 0; x < map.width(); ++x) {
            const bool walkable = map.isWalkable(x, y);
            const bool edge = x == 0 || y == 0 || x == map.width() - 1 || y == map.height() - 1;
            if (edge) check(!walkable, "outer edge is wall" + where);
            if (walkable && !inVault(x, y))
                check(reached[static_cast<std::size_t>(y * map.width() + x)], "every floor tile reachable" + where);
        }

    check(d.hasBossRoom == params.includeBossRoom, "boss room present iff requested" + where);
    if (d.hasBossRoom)
        for (int y = d.bossRoomCenter.y - 3; y <= d.bossRoomCenter.y + 3; ++y)
            for (int x = d.bossRoomCenter.x - 3; x <= d.bossRoomCenter.x + 3; ++x)
                check(map.isWalkable(x, y), "boss arena's central 7x7 is floor" + where);

    check(d.hasVault == (params.includeVault && !params.includeBossRoom), "vault present iff requested" + where);
    if (d.hasVault) {
        check(!reached[static_cast<std::size_t>(d.vaultCenter.y * map.width() + d.vaultCenter.x)],
              "vault is sealed" + where);
        check(!map.isWalkable(d.vaultEntrance.x, d.vaultEntrance.y), "vault gate starts closed" + where);
        check(std::abs(d.vaultCenter.x - d.vaultEntrance.x) + std::abs(d.vaultCenter.y - d.vaultEntrance.y) == 3,
              "vault gate is 3 tiles from the cache" + where);
        Map opened = map;
        opened.setTile(d.vaultEntrance.x, d.vaultEntrance.y, Tile{TileType::Floor, true, true});
        check(reachableFrom(opened, d.playerStart)[static_cast<std::size_t>(d.vaultCenter.y * map.width() + d.vaultCenter.x)],
              "opening the gate reaches the cache" + where);
    }

    std::set<std::string> regular;
    for (const auto& name : d.moduleNames)
        if (name != bossModule().name && name != vaultModule().name) check(regular.insert(name).second, "modules are distinct" + where);

    check(!d.otherRoomCenters.empty(), "there are encounter anchors" + where);
    for (const Position& p : d.otherRoomCenters) {
        check(map.isWalkable(p.x, p.y), "anchors are floor" + where);
        const int dx = p.x - d.playerStart.x, dy = p.y - d.playerStart.y;
        check(dx * dx + dy * dy > 64, "no anchor right next to the start" + where);
    }
}

void printDungeon(const GeneratedDungeon& d) {
    for (int y = 0; y < d.map.height(); ++y) {
        for (int x = 0; x < d.map.width(); ++x) {
            const Position p{x, y};
            const auto is = [&](Position q) { return q.x == p.x && q.y == p.y; };
            char c = d.map.tileAt(x, y).type == TileType::Wall ? '#' : '.';
            for (const Position& a : d.otherRoomCenters) if (is(a)) c = 'g';
            if (d.hasBossRoom && is(d.bossRoomCenter)) c = 'B';
            if (d.hasVault && is(d.vaultCenter)) c = 'V';
            if (is(d.playerStart)) c = '@';
            std::cout << c;
        }
        std::cout << '\n';
    }
}

} // namespace

int main() {
    std::cout << "Dungeon generation test\n-----------------------\n";

    const auto moduleErrors = validateModules();
    for (const auto& error : moduleErrors) std::cout << "[FAIL] module " << error << '\n';
    failures += static_cast<int>(moduleErrors.size());
    std::cout << regularModules().size() << " regular modules in the pool\n";

    int floors = 0;
    for (const bool boss : {false, true})
        for (const bool vault : {false, true})
            for (unsigned seed = 1; seed <= 100; ++seed) {
                DungeonGenerationParams params;
                params.includeBossRoom = boss;
                params.includeVault = vault;
                checkFloor(generateDungeon(params, seed), params, seed);
                ++floors;
            }
    std::cout << floors << " generated floors checked\n";

    DungeonGenerationParams exampleParams;
    exampleParams.includeBossRoom = false;
    exampleParams.includeVault = true;
    const GeneratedDungeon example = generateDungeon(exampleParams, 42);
    std::cout << "\nExample (seed 42): @ start, g encounter anchors (last = door down), V vault\n";
    for (std::size_t i = 0; i < example.moduleNames.size(); ++i)
        std::cout << example.moduleNames[i] << ((i + 1) % kModuleGrid ? " | " : "\n");
    printDungeon(example);

    std::cout << (failures ? "\nFAILED: " + std::to_string(failures) + " check(s)\n" : "\nAll dungeon checks passed.\n");
    return failures ? 1 : 0;
}
