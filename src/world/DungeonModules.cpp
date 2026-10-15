#include "world/DungeonModules.hpp"

#include <cstdlib>
#include <queue>
#include <set>
#include <utility>

namespace engine {

// Landmark set pieces. One may anchor a floor; the rest of the floor is
// procedural and hand-made cells around it.
const std::vector<ModuleTemplate>& landmarkModules() {
    static const std::vector<ModuleTemplate> modules{
    {"Shrine", {
        "#########...#########",
        "#...................#",
        "#..#.............#..#",
        "#.....#.......#.....#",
        "#...................#",
        ".......#.....#.......",
        "..........S..........",
        ".......#.....#.......",
        "#...................#",
        "#.....#.......#.....#",
        "#..#.............#..#",
        "#...................#",
        "#########...#########",
    }},
    // Fountains: the gargoyle (S) is set into the face of a central wall
    // block, spouting onto the floor in front of it.
    {"Fountain Court", {
        "#########...#########",
        "#...................#",
        "#.#...............#.#",
        "#...................#",
        "#.......#####.......#",
        "........##S##........",
        ".....................",
        ".....................",
        "#...................#",
        "#...#...........#...#",
        "#...................#",
        "#.#...............#.#",
        "#########...#########",
    }},
    // The ritual circle is painted on open floor around its altar stone.
    {"Ritual Chamber", {
        "#########...#########",
        "#...................#",
        "#..#.............#..#",
        "#...................#",
        "#...................#",
        "....#...........#....",
        "..........S..........",
        "....#...........#....",
        "#...................#",
        "#...................#",
        "#..#.............#..#",
        "#...................#",
        "#########...#########",
    }},
    // A hoard heaped in a walled treasury, open on four sides.
    {"Treasury", {
        "#########...#########",
        "#...................#",
        "#..####.......####..#",
        "#..#.............#..#",
        "#..#.............#..#",
        ".......#.....#.......",
        "..........S..........",
        ".......#.....#.......",
        "#..#.............#..#",
        "#..#.............#..#",
        "#..####.......####..#",
        "#...................#",
        "#########...#########",
    }},
    // A cellblock: empty cells along both walls, the occupied cage between.
    {"Cellblock", {
        "#########...#########",
        "#...#...#...#...#...#",
        "#...#...#...#...#...#",
        "#...................#",
        "#...................#",
        ".......##...##.......",
        "..........S..........",
        ".......##...##.......",
        "#...................#",
        "#...................#",
        "#...#...#...#...#...#",
        "#...#...#...#...#...#",
        "#########...#########",
    }},
    // A round fighting pit ringed by pillars, the brazier in its centre.
    {"Fighting Pit", {
        "#########...#########",
        "#...................#",
        "#.....#.......#.....#",
        "#...#...........#...#",
        "#...................#",
        ".....#.........#.....",
        "..........S..........",
        ".....#.........#.....",
        "#...................#",
        "#...#...........#...#",
        "#.....#.......#.....#",
        "#...................#",
        "#########...#########",
    }},
    // Very rare events. A burial hall: pillar rows lead to the tomb.
    {"Burial Hall", {
        "#########...#########",
        "#...................#",
        "#.#.#.#.......#.#.#.#",
        "#...................#",
        "#..######...######..#",
        "....#...........#....",
        "..........S..........",
        "....#...........#....",
        "#..######...######..#",
        "#...................#",
        "#.#.#.#.......#.#.#.#",
        "#...................#",
        "#########...#########",
    }},
    // The peddler's alcove: a quiet hall with his stall at its heart.
    {"Peddler's Alcove", {
        "#########...#########",
        "#...................#",
        "#...##.........##...#",
        "#...##.........##...#",
        "#...................#",
        "........#...#........",
        "..........S..........",
        "........#...#........",
        "#...................#",
        "#...##.........##...#",
        "#...##.........##...#",
        "#...................#",
        "#########...#########",
    }},
    // The demon's binding hall: four great pillars hold its chains.
    {"Binding Hall", {
        "#########...#########",
        "#...................#",
        "#...................#",
        "#....##.......##....#",
        "#....##.......##....#",
        ".....................",
        "..........S..........",
        ".....................",
        "#....##.......##....#",
        "#....##.......##....#",
        "#...................#",
        "#...................#",
        "#########...#########",
    }},
    };
    return modules;
}

// New modules go in regularModules() below -- see DungeonModules.hpp for
// the authoring rules. dungeon_test fails with a description of the
// problem if one is broken.

// Regular modules: any socket may be opened or sealed.
const std::vector<ModuleTemplate>& regularModules() {
    static const std::vector<ModuleTemplate> modules{
    {"Great Hall", {
        "#########...#########",
        "#...................#",
        "#..##...........##..#",
        "#..##...........##..#",
        "#...................#",
        ".....##.......##.....",
        ".....................",
        ".....##.......##.....",
        "#...................#",
        "#..##...........##..#",
        "#..##...........##..#",
        "#...................#",
        "#########...#########",
    }},
    {"Crossroads", {
        "#########...#########",
        "#.....#.......#.....#",
        "#.....#.......#.....#",
        "#...................#",
        "#.....#.......#.....#",
        "...........A.........",
        ".....................",
        ".....................",
        "#.....#.......#.....#",
        "#.....#.......#.....#",
        "#.....#.......#.....#",
        "#.....#.......#.....#",
        "#########...#########",
    }},
    {"Ring", {
        "#########...#########",
        "#...................#",
        "#...................#",
        "#...####.....####...#",
        "#...#...........#...#",
        "......A......A.......",
        ".....................",
        "......#.......#......",
        "#...#...........#...#",
        "#...####.....####...#",
        "#...................#",
        "#...................#",
        "#########...#########",
    }},
    {"Twin Chambers", {
        "#########...#########",
        "#.......#...#.......#",
        "#.......#...#.......#",
        "#.........A.........#",
        "#.......#...#.......#",
        "....###.......###....",
        ".....................",
        "....###.......###....",
        "#.......#...#.......#",
        "#.......#...#.......#",
        "#...................#",
        "#.......#...#.......#",
        "#########...#########",
    }},
    {"Colonnade", {
        "#########...#########",
        "#...................#",
        "#.#..#..#...#..#..#.#",
        "#...................#",
        "#...................#",
        "..#..#..#...#..#..#..",
        ".....................",
        "..#..#..#...#..#..#..",
        "#...................#",
        "#...................#",
        "#.#..#..#...#..#..#.#",
        "#...................#",
        "#########...#########",
    }},
    {"Cavern", {
        "#########...#########",
        "##.....##.....###..##",
        "#.......#.........###",
        "#...##.........##...#",
        "##..###.......###...#",
        ".....#...............",
        ".....................",
        ".............#.......",
        "#....##.....###....##",
        "#...###......#.....##",
        "##.................##",
        "###....###.....#...##",
        "#########...#########",
    }},
    {"Shrine", {
        "#########...#########",
        "#...................#",
        "#.#####.......#####.#",
        "#.#...............#.#",
        "#.#...............#.#",
        ".....A.........A.....",
        ".........###.........",
        ".........###.........",
        "#.#...............#.#",
        "#.#...............#.#",
        "#.#####.......#####.#",
        "#...................#",
        "#########...#########",
    }},
    {"Barracks", {
        "#########...#########",
        "#...................#",
        "#.####..#.....#..####",
        "#...................#",
        "#.####.............##",
        ".....................",
        ".....................",
        ".....................",
        "#.####.............##",
        "#...................#",
        "#.####..#.....#..####",
        "#...................#",
        "#########...#########",
    }},
    {"Broken Walls", {
        "#########...#########",
        "#.......#...........#",
        "#.#####.#.###.####..#",
        "#...........#.......#",
        "#.###.###...#.#####.#",
        ".....................",
        ".....................",
        ".....................",
        "#.###.##.....##.###.#",
        "#.......#...........#",
        "#.#####.#.###.####..#",
        "#...................#",
        "#########...#########",
    }},
    {"Courtyard", {
        "#########...#########",
        "#...###.......###...#",
        "#...###.......###...#",
        "#...................#",
        "#...................#",
        ".....................",
        ".....................",
        ".....................",
        "#...................#",
        "#...................#",
        "#...###.......###...#",
        "#...###.......###...#",
        "#########...#########",
    }},
    {"Crypt Niches", {
        "#########...#########",
        "#.#.#.#.......#.#.#.#",
        "#...................#",
        "###...............###",
        "#.....#.......#.....#",
        "...........A.........",
        ".....................",
        ".....................",
        "#.....#.......#.....#",
        "###...............###",
        "#...................#",
        "#.#.#.#.......#.#.#.#",
        "#########...#########",
    }},
    {"Staggered Hall", {
        "#########...#########",
        "#...................#",
        "#..###..............#",
        "#.......###.........#",
        "#...........###.....#",
        "..............###....",
        ".....................",
        "....###..............",
        "#.....###...........#",
        "#.........###.......#",
        "#.............###...#",
        "#...................#",
        "#########...#########",
    }},
    {"Cloister", {
        "#########...#########",
        "#...................#",
        "#.#.#.#.#...#.#.#.#.#",
        "#...................#",
        "#.#...###...###...#.#",
        ".....#.........#.....",
        ".....................",
        ".....#.........#.....",
        "#.#...###...###...#.#",
        "#...................#",
        "#.#.#.#.#...#.#.#.#.#",
        "#...................#",
        "#########...#########",
    }},
    {"Chapel", {
        "#########...#########",
        "#...................#",
        "#..###.........###..#",
        "#...................#",
        "#.####..#####..####.#",
        ".....................",
        ".....................",
        ".....................",
        "#.####..#####..####.#",
        "#...................#",
        "#..###.........###..#",
        "#...................#",
        "#########...#########",
    }},
    {"Guard Posts", {
        "#########...#########",
        "#.....#.......#.....#",
        "#.....#.......#.....#",
        "#...................#",
        "###.###.......###.###",
        ".....................",
        ".....................",
        ".....................",
        "###.###.......###.###",
        "#...................#",
        "#.....#.......#.....#",
        "#.....#.......#.....#",
        "#########...#########",
    }},
    {"Sunken Hall", {
        "#########...#########",
        "#...................#",
        "#...................#",
        "#...#####...#####...#",
        "#...#...........#...#",
        "......A.......A......",
        ".....................",
        "......A.......A......",
        "#...#...........#...#",
        "#...#####...#####...#",
        "#...................#",
        "#...................#",
        "#########...#########",
    }},
    {"Library", {
        "#########...#########",
        "#...................#",
        "#.#####.#...#.#####.#",
        "#...................#",
        "#.#####.#...#.#####.#",
        ".....................",
        ".....................",
        ".....................",
        "#.#####.#...#.#####.#",
        "#...................#",
        "#.#####.#...#.#####.#",
        "#...................#",
        "#########...#########",
    }},
    {"Catacomb Niches", {
        "#########...#########",
        "#.#.#.#.#...#.#.#.#.#",
        "#...................#",
        "#.##.##.......##.##.#",
        "#.#...............#.#",
        ".....................",
        ".....................",
        ".....................",
        "#.#...............#.#",
        "#.##.##.......##.##.#",
        "#...................#",
        "#.#.#.#.#...#.#.#.#.#",
        "#########...#########",
    }},
    {"Split Hall", {
        "#########...#########",
        "#.......#...........#",
        "#.......#...........#",
        "#.......#....#......#",
        "#............#......#",
        ".............#.......",
        ".....................",
        ".......#.............",
        "#......#............#",
        "#......#....#.......#",
        "#...........#.......#",
        "#...........#.......#",
        "#########...#########",
    }},
    {"Forum", {
        "#########...#########",
        "#...................#",
        "#.##.............##.#",
        "#.##.............##.#",
        "#.......#...#.......#",
        ".....................",
        ".....................",
        ".....................",
        "#.......#...#.......#",
        "#.##.............##.#",
        "#.##.............##.#",
        "#...................#",
        "#########...#########",
    }},
    // A rift splits the room three ways; one bridge leads up, one down. A
    // shove at the wrong moment drops you a floor. Each bridge is held, and
    // archers on the far ledges shoot across the gap.
    {"Rift Crossing", {
        "#########...#########",
        "#...R...............#",
        "#...................#",
        "#__________...______#",
        "#__________.P.______#",
        ".....................",
        "..........A..........",
        ".....................",
        "#____.P.____________#",
        "#____...____________#",
        "#...................#",
        "#...............R...#",
        "#########...#########",
    }},
    // The floor has given way in places: holes to fight around.
    {"Sinkholes", {
        "#########...#########",
        "#...................#",
        "#.__.....___.....__.#",
        "#.__.....___.....__.#",
        "#...................#",
        "......__.....__......",
        "......__.....__......",
        ".....................",
        "#...................#",
        "#.__.....___.....__.#",
        "#.__.....___.....__.#",
        "#...................#",
        "#########...#########",
    }},
    // Six side rooms off one corridor, and something waiting in them: walk
    // down it and they step out behind you.
    {"Gauntlet", {
        "#########...#########",
        "#.....##.....##.....#",
        "#.....##.....##.....#",
        "#..H..##.....##..H..#",
        "###.#####...#####.###",
        ".....................",
        ".....................",
        ".....................",
        "###.#####...#####.###",
        "#..H..##.....##..H..#",
        "#.....##.....##.....#",
        "#.....##.....##.....#",
        "#########...#########",
    }},
    // A well with no bottom, and a fight on either side of it.
    {"Well of Night", {
        "#########...#########",
        "#...................#",
        "#...................#",
        "#.....#.......#.....#",
        "#......_______......#",
        "...A..._______...A...",
        "......._______.......",
        "......._______.......",
        "#......_______......#",
        "#.....#.......#.....#",
        "#...................#",
        "#...................#",
        "#########...#########",
    }},
    };
    return modules;
}

// Big rooms: two cells wide or tall, with a socket on each cell's stretch of
// every side, so they sit in the grid like two ordinary cells.
const std::vector<ModuleTemplate>& bigModules() {
    static const std::vector<ModuleTemplate> modules{
    // Two cells long: pillar rows down a nave, a screened apse at the east end.
    {"Great Nave", {
        "#########...##################...#########",
        "#........................................#",
        "#...................................#....#",
        "#....##...##...##...##...##...##....#....#",
        "#...................................#....#",
        "..........................................",
        "..........A.............A.............A...",
        "..........................................",
        "#...................................#....#",
        "#....##...##...##...##...##...##....#....#",
        "#...................................#....#",
        "#........................................#",
        "#########...##################...#########",
    }},
    // Two cells long: rows of bunks either side of the aisle.
    {"Grand Barracks", {
        "#########...##################...#########",
        "#........................................#",
        "#..##..##..##..##..##..##..##..##..##....#",
        "#........................................#",
        "#..##..##..##..##..##..##..##..##..##....#",
        "..........................................",
        "..........A....................A..........",
        "..........................................",
        "#..##..##..##..##..##..##..##..##..##....#",
        "#........................................#",
        "#..##..##..##..##..##..##..##..##..##....#",
        "#........................................#",
        "#########...##################...#########",
    }},
    // Two cells tall: a ledge beside a long rift, crossed by two bridges.
    {"Chasm Gallery", {
        "#########...#########",
        "#...................#",
        "#...................#",
        "#...........______..#",
        "#...........______..#",
        "....##......______...",
        ".....................",
        "............______...",
        "#.......A...______..#",
        "#...........______..#",
        "#...........______..#",
        "#...........______..#",
        "#...##......______..#",
        "#...##......______..#",
        "#...........______..#",
        "#...........______..#",
        "#...........______..#",
        "#.......A...______..#",
        "............______...",
        ".....................",
        "....##......______...",
        "#...........______..#",
        "#...........______..#",
        "#...................#",
        "#...................#",
        "#########...#########",
    }},
    // Two cells tall: burial niches either side, a processional between pillars.
    {"Long Crypt", {
        "#########...#########",
        "#...................#",
        "#####...........#####",
        "#...................#",
        "#...................#",
        "..........A..........",
        ".....................",
        ".....................",
        "#####...........#####",
        "#......##...##......#",
        "#......##...##......#",
        "#####..##...##..#####",
        "#......##...##......#",
        "#......##...##......#",
        "#####..##...##..#####",
        "#......##...##......#",
        "#......##...##......#",
        "#####...........#####",
        ".....................",
        ".....................",
        "..........A..........",
        "#...................#",
        "#...................#",
        "#####...........#####",
        "#...................#",
        "#########...#########",
    }},
    };
    return modules;
}

const ModuleTemplate& bossModule() {
    static const ModuleTemplate module{"Boss Arena", {
        "#########...#########",
        "#...................#",
        "#..##...........##..#",
        "#..##...........##..#",
        "#...................#",
        ".....................",
        ".....................",
        ".....................",
        "#...................#",
        "#..##...........##..#",
        "#..##...........##..#",
        "#...................#",
        "#########...#########",
    }};
    return module;
}

const ModuleTemplate& vaultModule() {
    static const ModuleTemplate module{"Vault", {
        "#########...#########",
        "#...................#",
        "#...................#",
        "#.....#.....#######.#",
        "#.....#.....#.....#.#",
        "............#.....#..",
        ".....A......V..C..#..",
        "............#.....#..",
        "#.....#.....#.....#.#",
        "#.....#.....#######.#",
        "#...................#",
        "#...................#",
        "#########...#########",
    }};
    return module;
}


namespace {

bool inDoorway(int x, int y, int w, int h, const std::vector<Doorway>& doorways) {
    for (const Doorway& d : doorways) {
        const bool along = (d.side == Side::Top || d.side == Side::Bottom) ? x >= d.at && x <= d.at + 2 : y >= d.at && y <= d.at + 2;
        const bool edge = d.side == Side::Top ? y == 0 : d.side == Side::Bottom ? y == h - 1 : d.side == Side::Left ? x == 0 : x == w - 1;
        if (along && edge) return true;
    }
    return false;
}

bool isFloorChar(char c) { return c == '.' || c == 'A' || c == 'C' || c == 'P' || c == 'R' || c == 'H'; }

// Checks a room against the authoring rules. `doorways` null means the
// standard sockets for the room's size.
void validateModule(const ModuleTemplate& module, bool boss, bool vault, bool landmark,
                    const std::vector<Doorway>* doorways, std::vector<std::string>& errors) {
    const auto fail = [&](const std::string& what) { errors.push_back(module.name + ": " + what); };
    const int h = static_cast<int>(module.rows.size());
    const int w = module.rows.empty() ? 0 : static_cast<int>(module.rows.front().size());
    const bool single = boss || vault || landmark;
    const bool sized = (h == kModuleHeight && (w == kModuleWidth || (!single && w == 2 * kModuleWidth))) ||
                       (!single && h == 2 * kModuleHeight && w == kModuleWidth);
    if (!sized) {
        fail(single ? "needs " + std::to_string(kModuleHeight) + " rows of " + std::to_string(kModuleWidth) + " characters"
                    : "must be 1x1, 2x1 or 1x2 cells (" + std::to_string(kModuleWidth) + "x" + std::to_string(kModuleHeight) + " each)");
        return;
    }
    for (std::size_t y = 0; y < module.rows.size(); ++y)
        if (static_cast<int>(module.rows[y].size()) != w) {
            fail("row " + std::to_string(y) + " needs " + std::to_string(w) + " characters");
            return;
        }
    const auto at = [&](int x, int y) { return module.rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)]; };
    const std::vector<Doorway> sockets = doorways ? *doorways : centralSockets(w / kModuleWidth, h / kModuleHeight);
    const auto isSocket = [&](int x, int y) { return inDoorway(x, y, w, h, sockets); };

    std::pair<int, int> start{-1, -1};
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const bool border = x == 0 || y == 0 || x == w - 1 || y == h - 1;
            if (border && isFloorChar(at(x, y)) != isSocket(x, y))
                fail("border tile (" + std::to_string(x) + "," + std::to_string(y) +
                     (isSocket(x, y) ? ") is a socket and must be floor" : ") must be wall"));
            if (border && isSocket(x, y) && start.first < 0) start = {x, y};
            const char c = at(x, y);
            if (c != '#' && !isFloorChar(c) && c != 'V' && c != 'S' && c != '_')
                fail(std::string("unknown character '") + c + "'");
            if ((c == 'V' || c == 'C') && !vault) fail("V/C are only allowed in the vault module");
            if (c == 'S' && !landmark) fail("S (altar) is only allowed in landmark modules");
        }

    if (start.first < 0) { fail("needs at least one socket"); return; }
    std::set<std::pair<int, int>> reached;
    std::queue<std::pair<int, int>> frontier;
    frontier.push(start);
    reached.insert(start);
    while (!frontier.empty()) {
        const auto [x, y] = frontier.front();
        frontier.pop();
        for (const auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
            if (isFloorChar(at(nx, ny)) && reached.insert({nx, ny}).second) frontier.push({nx, ny});
        }
    }

    std::pair<int, int> cache{-1, -1}, gate{-1, -1}, altar{-1, -1};
    std::vector<std::pair<int, int>> anchors;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (at(x, y) == 'C') cache = {x, y};
            if (at(x, y) == 'V') gate = {x, y};
            if (at(x, y) == 'A') anchors.push_back({x, y});
            if (at(x, y) == 'S') altar = {x, y};
        }
    // Landmarks are calm set pieces: they only get encounters where an A asks for one.
    if (anchors.empty() && !landmark) anchors.push_back({w / 2, h / 2});
    if (landmark) {
        if (altar.first < 0) fail("needs an S (altar)");
        else {
            bool approachable = false;
            for (const auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}})
                approachable |= reached.count({altar.first + dx, altar.second + dy}) > 0;
            if (!approachable) fail("the altar needs reachable floor beside it");
        }
    }

    const auto inVault = [&](int x, int y) {
        return vault && std::abs(x - cache.first) <= 2 && std::abs(y - cache.second) <= 2;
    };
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            if (isFloorChar(at(x, y)) && !reached.count({x, y}) && !inVault(x, y))
                fail("floor (" + std::to_string(x) + "," + std::to_string(y) + ") is unreachable");

    for (const auto [ax, ay] : anchors)
        for (int y = ay - 1; y <= ay + 1; ++y)
            for (int x = ax - 1; x <= ax + 1; ++x)
                if (!isFloorChar(at(x, y)))
                    fail("anchor (" + std::to_string(ax) + "," + std::to_string(ay) + ") needs floor around it");

    if (boss)
        for (int y = kModuleHeight / 2 - 3; y <= kModuleHeight / 2 + 3; ++y)
            for (int x = kModuleWidth / 2 - 3; x <= kModuleWidth / 2 + 3; ++x)
                if (!isFloorChar(at(x, y))) fail("the central 7x7 must be floor");

    if (vault) {
        if (cache.first < 0 || gate.first < 0) { fail("needs one V and one C"); return; }
        if (std::abs(cache.first - gate.first) + std::abs(cache.second - gate.second) != 3)
            fail("gate must be exactly 3 tiles from the cache");
        if (reached.count(cache)) fail("vault must be sealed until its gate opens");
        for (int y = cache.second - 2; y <= cache.second + 2; ++y)
            for (int x = cache.first - 2; x <= cache.first + 2; ++x)
                if (!isFloorChar(at(x, y))) fail("vault interior must be a 5x5 floor");
    }
}

} // namespace

std::vector<std::string> validateModules() {
    std::vector<std::string> errors;
    if (static_cast<int>(regularModules().size()) < kModuleGrid * kModuleGrid)
        errors.push_back("regular pool needs at least " + std::to_string(kModuleGrid * kModuleGrid) + " modules");
    for (const auto& module : regularModules()) {
        validateModule(module, false, false, false, nullptr, errors);
        if (module.rows.size() != static_cast<std::size_t>(kModuleHeight) || module.rows.front().size() != static_cast<std::size_t>(kModuleWidth))
            errors.push_back(module.name + ": regular modules are one cell; big ones go in bigModules()");
    }
    for (const auto& module : bigModules()) {
        validateModule(module, false, false, false, nullptr, errors);
        if (module.rows.size() == static_cast<std::size_t>(kModuleHeight) && module.rows.front().size() == static_cast<std::size_t>(kModuleWidth))
            errors.push_back(module.name + ": a big module spans two cells");
    }
    validateModule(bossModule(), true, false, false, nullptr, errors);
    validateModule(vaultModule(), false, true, false, nullptr, errors);
    for (const auto& module : landmarkModules()) validateModule(module, false, false, true, nullptr, errors);
    return errors;
}

std::vector<std::string> validateRegularModule(const ModuleTemplate& module) {
    std::vector<std::string> errors;
    validateModule(module, false, false, false, nullptr, errors);
    return errors;
}

std::vector<std::string> validateRoom(const ModuleTemplate& module, const std::vector<Doorway>& doorways) {
    std::vector<std::string> errors;
    validateModule(module, false, false, false, &doorways, errors);
    return errors;
}

std::vector<Doorway> centralSockets(int cellsWide, int cellsHigh) {
    std::vector<Doorway> doorways;
    for (int c = 0; c < cellsWide; ++c) {
        doorways.push_back({Side::Top, c * kModuleWidth + 9});
        doorways.push_back({Side::Bottom, c * kModuleWidth + 9});
    }
    for (int r = 0; r < cellsHigh; ++r) {
        doorways.push_back({Side::Left, r * kModuleHeight + 5});
        doorways.push_back({Side::Right, r * kModuleHeight + 5});
    }
    return doorways;
}

} // namespace engine
