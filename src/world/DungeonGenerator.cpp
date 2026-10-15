#include "world/DungeonGenerator.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <deque>
#include <numeric>
#include <queue>
#include <random>

#include "entities/RunProgression.hpp"
#include "world/DungeonModules.hpp"
#include "world/ProceduralModules.hpp"

namespace engine {

namespace {

// Props are placed as small arrangements with a purpose, never one by one:
//   Throne rooms - a throne centred on a long back wall, facing a deep room,
//                  flanked by a matching pair of statues (Sanctum, Crypts).
//   Galleries    - a pair of statues at either end of a long back wall.
//   Storage      - barrels, crates and sacks stacked into a room corner.
// "Back wall" means floor with a visible wall face directly behind it. Every
// arrangement keeps clear of encounters, the start, sockets, set pieces and
// other arrangements, and is kept only if every floor tile stays reachable.
void placeProps(GeneratedDungeon& d, FloorRegion region, bool cathedral, std::mt19937& rng) {
    Map& map = d.map;
    const int w = map.width(), h = map.height();
    const auto reachable = [&]() {
        std::vector<bool> seen(static_cast<std::size_t>(w * h), false);
        std::queue<Position> frontier;
        frontier.push(d.playerStart);
        seen[static_cast<std::size_t>(d.playerStart.y * w + d.playerStart.x)] = true;
        int count = 0;
        while (!frontier.empty()) {
            const Position p = frontier.front();
            frontier.pop();
            ++count;
            for (const Position step : {Position{1, 0}, Position{-1, 0}, Position{0, 1}, Position{0, -1}}) {
                const Position n{p.x + step.x, p.y + step.y};
                if (!map.isWalkable(n.x, n.y)) continue;
                const auto i = static_cast<std::size_t>(n.y * w + n.x);
                if (!seen[i]) { seen[i] = true; frontier.push(n); }
            }
        }
        return count;
    };
    const auto wall = [&](int x, int y) { return !map.inBounds(x, y) || map.tileAt(x, y).type == TileType::Wall; };
    const auto floorAt = [&](int x, int y) { return map.inBounds(x, y) && map.tileAt(x, y).type == TileType::Floor && map.isWalkable(x, y); };
    const auto near = [](Position a, Position b, int r) { return std::abs(a.x - b.x) <= r && std::abs(a.y - b.y) <= r; };
    const auto allowed = [&](Position p) {
        if (!floorAt(p.x, p.y)) return false;
        for (const Position& door : d.doorways) if (near(p, door, 2)) return false; // keep doorways clear
        if (near(p, d.playerStart, 2)) return false;
        for (const Position& a : d.otherRoomCenters) if (near(p, a, 2)) return false;
        for (const auto& posts : d.roomPosts)
            for (const Post& post : posts) if (near(p, post.pos, 1)) return false;
        if (d.hasBossRoom && near(p, d.bossRoomCenter, 4)) return false;
        if (d.hasVault && near(p, d.vaultCenter, 4)) return false;
        if (d.landmark != LandmarkKind::None && near(p, d.landmarkAltar, 4)) return false;
        for (const auto& extra : d.extraLandmarks) if (near(p, extra.second, 4)) return false;
        for (const Prop& prop : d.props)
            for (int i = 0; i < propWidth(prop.kind); ++i)
                if (near(p, {prop.pos.x + i, prop.pos.y}, 1)) return false; // arrangements never touch
        return true;
    };
    int baseline = reachable();
    // Places a whole arrangement or nothing.
    const auto tryGroup = [&](const std::vector<Prop>& group) {
        for (const Prop& prop : group)
            for (int i = 0; i < propWidth(prop.kind); ++i)
                if (!allowed({prop.pos.x + i, prop.pos.y})) return false;
        int tiles = 0;
        for (const Prop& prop : group)
            for (int i = 0; i < propWidth(prop.kind); ++i, ++tiles)
                map.setTile(prop.pos.x + i, prop.pos.y, Tile{TileType::Wall, false, true});
        if (reachable() != baseline - tiles) {
            for (const Prop& prop : group)
                for (int i = 0; i < propWidth(prop.kind); ++i)
                    map.setTile(prop.pos.x + i, prop.pos.y, Tile{TileType::Floor, true, true});
            return false;
        }
        baseline -= tiles;
        d.props.insert(d.props.end(), group.begin(), group.end());
        return true;
    };
    const auto roll = [&](int low, int high) { return std::uniform_int_distribution<int>(low, high)(rng); };

    // Back-wall runs: horizontal stretches of floor with wall directly behind.
    struct Run { int y, x0, x1; };
    std::vector<Run> runs;
    for (int y = 1; y < h - 1; ++y)
        for (int x = 1; x < w - 1; ++x) {
            if (!(floorAt(x, y) && wall(x, y - 1))) continue;
            int end = x;
            while (end + 1 < w - 1 && floorAt(end + 1, y) && wall(end + 1, y - 1)) ++end;
            runs.push_back({y, x, end});
            x = end;
        }
    std::shuffle(runs.begin(), runs.end(), rng);
    const auto depth = [&](int x, int y) { int n = 0; while (n < 6 && floorAt(x, y + n)) ++n; return n; };

    // --- Lived-in rooms ------------------------------------------------------
    // Most encounter rooms are dressed for what their occupants do there.
    // Furniture sits three tiles out from the room's anchor (clear of where
    // the encounter stands); decals scatter within four tiles of it.
    d.roomVignettes.assign(d.otherRoomCenters.size(), Vignette::None);
    {
        struct Piece { PropKind kind; int dx, dy; };
        struct Litter { DecalKind kind; int count; };
        struct Template { std::vector<Piece> props; std::vector<Litter> litter; };
        const auto layout = [](Vignette v) -> Template {
            switch (v) {
                case Vignette::Mess: return {{{PropKind::Chair, -2, -3}, {PropKind::Table, -1, -3}, {PropKind::Chair, 1, -3}},
                                             {{DecalKind::Debris, 2}, {DecalKind::Bones, 2}}};
                case Vignette::Armoury: return {{{PropKind::Crate, -3, -2}, {PropKind::Crate, -3, -1}, {PropKind::Barrel, -3, 1}},
                                                {{DecalKind::Weapon, 3}}};
                case Vignette::Dormitory: return {{{PropKind::Sacks, -3, -1}, {PropKind::Sacks, -3, 1}, {PropKind::Sacks, 3, 0}},
                                                  {{DecalKind::Debris, 2}, {DecalKind::Coins, 1}}};
                case Vignette::Nest: return {{}, {{DecalKind::Web, 5}, {DecalKind::Bones, 2}, {DecalKind::Skull, 1}}};
                case Vignette::Shrine: return {{{PropKind::Idol, 0, -3}}, {{DecalKind::Candle, 3}, {DecalKind::Bones, 2}, {DecalKind::Skull, 1}}};
                case Vignette::Library: return {{{PropKind::Bookcase, -2, -3}, {PropKind::Bookcase, -1, -3}, {PropKind::Bookcase, 1, -3}},
                                                {{DecalKind::Book, 3}, {DecalKind::Candle, 1}}};
                case Vignette::Ossuary: return {{{PropKind::Sarcophagus, -3, -2}, {PropKind::Sarcophagus, 3, -2}},
                                                {{DecalKind::Skull, 2}, {DecalKind::Bones, 3}}};
                case Vignette::GraveDig: return {{{PropKind::Sarcophagus, -3, -1}, {PropKind::Sacks, 3, 1}},
                                                 {{DecalKind::Dirt, 4}, {DecalKind::Coins, 2}, {DecalKind::Pick, 1}, {DecalKind::Bones, 2}}};
                case Vignette::None: break;
            }
            return {};
        };
        std::vector<Vignette> menu;
        if (region == FloorRegion::Barracks) menu = {Vignette::Mess, Vignette::Mess, Vignette::Armoury, Vignette::Dormitory, Vignette::Nest};
        else if (region == FloorRegion::Sanctum) menu = {Vignette::Shrine, Vignette::Shrine, Vignette::Library, Vignette::Mess};
        else if (!cathedral) menu = {Vignette::Ossuary, Vignette::Ossuary, Vignette::GraveDig};
        int digs = 0;
        for (std::size_t i = 0; i < d.otherRoomCenters.size() && !menu.empty(); ++i) {
            if (roll(0, 99) >= 45) continue;
            if (i < d.roomPosts.size() && !d.roomPosts[i].empty()) continue; // its set encounter stays as drawn
            Vignette v = menu[static_cast<std::size_t>(roll(0, static_cast<int>(menu.size()) - 1))];
            if (v == Vignette::GraveDig && digs >= 1) v = Vignette::Ossuary;
            const Position a = d.otherRoomCenters[i];
            const auto plan = layout(v);
            bool placed = plan.props.empty();
            // Try the layout as drawn, then mirrored, then flipped.
            for (int t = 0; t < 4 && !placed; ++t) {
                const int sx = (t & 1) ? -1 : 1, sy = (t & 2) ? -1 : 1;
                std::vector<Prop> group;
                for (const auto& piece : plan.props) {
                    int x = a.x + piece.dx * sx;
                    if (sx < 0) x -= propWidth(piece.kind) - 1; // a mirrored table still starts at its left
                    group.push_back({piece.kind, {x, a.y + piece.dy * sy}});
                }
                placed = tryGroup(group);
            }
            if (!placed) continue;
            d.roomVignettes[i] = v;
            digs += v == Vignette::GraveDig;
            for (const auto& [kind, count] : plan.litter)
                for (int n = 0, tries = 0; n < count && tries < 40; ++tries) {
                    const Position p{a.x + roll(-4, 4), a.y + roll(-4, 4)};
                    if (!floorAt(p.x, p.y)) continue;
                    if (std::any_of(d.decals.begin(), d.decals.end(), [&](const Decal& e) { return e.pos.x == p.x && e.pos.y == p.y; })) continue;
                    d.decals.push_back({kind, p});
                    ++n;
                }
        }
    }

    // --- Throne rooms and galleries ---------------------------------------
    const bool grand = region != FloorRegion::Barracks;
    int thrones = grand ? roll(1, 2) : roll(0, 1);
    int galleries = grand ? roll(1, 2) : 0;
    const PropKind throne = region == FloorRegion::Crypts ? PropKind::SkeletonThrone : PropKind::Throne;
    for (const Run& run : runs) {
        const int length = run.x1 - run.x0 + 1, c = (run.x0 + run.x1) / 2;
        if (thrones > 0 && length >= 5 && depth(c, run.y) >= 4) {
            std::vector<Prop> group{{throne, {c, run.y}}};
            if (grand && length >= 9) {
                group.push_back({PropKind::Statue, {c - 4, run.y}});
                group.push_back({PropKind::Statue, {c + 3, run.y}});
            }
            if (tryGroup(group)) { --thrones; continue; }
        }
        if (galleries > 0 && length >= 12 && depth(run.x0 + 1, run.y) >= 3 && depth(run.x1 - 1, run.y) >= 3) {
            if (tryGroup({{PropKind::Statue, {run.x0 + 1, run.y}}, {PropKind::Statue, {run.x1 - 2, run.y}}})) --galleries;
        }
    }

    // --- Storage corners -------------------------------------------------------
    std::vector<PropKind> stock{PropKind::Barrel, PropKind::Crate, PropKind::Sacks};
    int storage = roll(4, 6);
    if (region == FloorRegion::Sanctum) { stock = {PropKind::Crate, PropKind::Barrel}; storage = roll(2, 3); }
    if (region == FloorRegion::Crypts) { stock = {PropKind::Crate}; storage = roll(1, 2); }
    for (const Run& run : runs) {
        if (storage <= 0) break;
        // A corner: the run's end meets a side wall.
        const bool left = wall(run.x0 - 1, run.y), right = wall(run.x1 + 1, run.y);
        if (!left && !right) continue;
        const int dir = left ? 1 : -1;
        const int x = left ? run.x0 : run.x1;
        const auto item = [&]() { return stock[static_cast<std::size_t>(roll(0, static_cast<int>(stock.size()) - 1))]; };
        std::vector<Prop> group{{item(), {x, run.y}}, {item(), {x + dir, run.y}}};
        const int extra = roll(0, 2);
        if (extra >= 1) group.push_back({item(), {x, run.y + 1}});
        if (extra >= 2) group.push_back({item(), {x + 2 * dir, run.y}});
        if (tryGroup(group)) --storage;
    }
    d.decals.erase(std::remove_if(d.decals.begin(), d.decals.end(), [&](const Decal& e) { return !map.isWalkable(e.pos.x, e.pos.y); }), d.decals.end());
}

constexpr int kCells = kModuleGrid * kModuleGrid;
// Chance that a neighbor pair not already joined by the spanning tree is
// opened anyway. High on purpose: the floor should feel like one open
// space with several routes, not a single winding path.
constexpr float kExtraConnectionChance = 0.65f;
// How many cells are solid rock (none, one, two), and how many pairs of
// cells merge into one big room (none, one, two): weights.
constexpr std::array<int, 3> kRockWeights{4, 4, 2};
constexpr std::array<int, 3> kBigRoomWeights{5, 4, 1};
// Where a doorway sits along its edge: before, at, or past the middle.
constexpr std::array<int, 3> kDoorwayWeights{1, 2, 1};

struct Edge { int a, b; };

// A room: one grid cell, or two merged into a big one.
struct Room {
    int col = 0, row = 0, cellsWide = 1, cellsHigh = 1;
    std::vector<int> cells;
    bool single() const { return cells.size() == 1; }
    int x0() const { return col * kModuleWidth; }
    int y0() const { return row * kModuleHeight; }
    int width() const { return cellsWide * kModuleWidth; }
    int height() const { return cellsHigh * kModuleHeight; }
};

std::size_t landmarkModuleIndex(LandmarkKind kind) {
    switch (kind) {
        case LandmarkKind::Shrine: return 0;
        case LandmarkKind::RitualCircle: return 2;
        case LandmarkKind::TreasureHoard: return 3;
        case LandmarkKind::PrisonerCage: return 4;
        case LandmarkKind::ChampionPit: return 5;
        case LandmarkKind::SealedTomb: return 6;
        case LandmarkKind::PalePeddler: return 7;
        case LandmarkKind::ChainedDemon: return 8;
        case LandmarkKind::LamplighterRest: return 0; // a quiet room like the shrine's
        case LandmarkKind::BloodAltar: return 8;      // the demon's hall: room for a fight
        case LandmarkKind::Strongbox: return 5;       // the pit: room to be surrounded
        case LandmarkKind::Essence: return 5;
        case LandmarkKind::Breach: return 8;          // the demon's hall: room for the rift to spread
        default: return 1; // both fountains
    }
}

std::vector<Edge> gridEdges() {
    std::vector<Edge> edges;
    for (int r = 0; r < kModuleGrid; ++r)
        for (int c = 0; c < kModuleGrid; ++c) {
            const int cell = r * kModuleGrid + c;
            if (c + 1 < kModuleGrid) edges.push_back({cell, cell + 1});
            if (r + 1 < kModuleGrid) edges.push_back({cell, cell + kModuleGrid});
        }
    return edges;
}

int findRoot(std::array<int, kCells>& parent, int i) {
    while (parent[i] != i) i = parent[i] = parent[parent[i]];
    return i;
}

std::vector<std::string> mirrored(const std::vector<std::string>& rows, bool flipX, bool flipY) {
    std::vector<std::string> out = rows;
    if (flipX) for (auto& row : out) std::reverse(row.begin(), row.end());
    if (flipY) std::reverse(out.begin(), out.end());
    return out;
}

// Whether the given cells are all joined to each other through grid neighbours.
bool connectedCells(const std::array<bool, kCells>& active) {
    int first = -1, count = 0;
    for (int cell = 0; cell < kCells; ++cell)
        if (active[cell]) { if (first < 0) first = cell; ++count; }
    if (first < 0) return false;
    std::array<bool, kCells> seen{};
    std::queue<int> frontier;
    frontier.push(first);
    seen[first] = true;
    int reached = 0;
    while (!frontier.empty()) {
        const int cell = frontier.front();
        frontier.pop();
        ++reached;
        for (const Edge& e : gridEdges()) {
            const int next = e.a == cell ? e.b : e.b == cell ? e.a : -1;
            if (next >= 0 && active[next] && !seen[next]) { seen[next] = true; frontier.push(next); }
        }
    }
    return reached == count;
}

// Joins every walkable tile to `start` by carving the fewest walls. Carving
// never touches a protected tile (room borders, chasms, altars, the vault),
// so it only ever links up the inside of a room with its own doorways.
void connectAll(Map& map, Position start, const std::vector<bool>& isProtected, const std::vector<bool>& ignore) {
    const int w = map.width(), h = map.height();
    const auto index = [w](int x, int y) { return static_cast<std::size_t>(y * w + x); };
    const Position steps[4] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (int pass = 0; pass < w * h; ++pass) {
        std::vector<bool> reached(static_cast<std::size_t>(w * h), false);
        std::queue<Position> frontier;
        frontier.push(start);
        reached[index(start.x, start.y)] = true;
        while (!frontier.empty()) {
            const Position p = frontier.front();
            frontier.pop();
            for (const Position s : steps) {
                const Position n{p.x + s.x, p.y + s.y};
                if (!map.isWalkable(n.x, n.y) || reached[index(n.x, n.y)]) continue;
                reached[index(n.x, n.y)] = true;
                frontier.push(n);
            }
        }
        Position lost{-1, -1};
        for (int y = 0; y < h && lost.x < 0; ++y)
            for (int x = 0; x < w && lost.x < 0; ++x)
                if (map.isWalkable(x, y) && !reached[index(x, y)] && !ignore[index(x, y)]) lost = {x, y};
        if (lost.x < 0) return;
        // 0-1 search from the lost tile: floor is free, wall costs one.
        std::vector<int> cost(static_cast<std::size_t>(w * h), 1 << 30);
        std::vector<int> from(static_cast<std::size_t>(w * h), -1);
        std::deque<Position> open;
        cost[index(lost.x, lost.y)] = 0;
        open.push_back(lost);
        Position goal{-1, -1};
        while (!open.empty()) {
            const Position p = open.front();
            open.pop_front();
            if (reached[index(p.x, p.y)]) { goal = p; break; }
            for (const Position s : steps) {
                const Position n{p.x + s.x, p.y + s.y};
                if (!map.inBounds(n.x, n.y)) continue;
                const bool walkable = map.isWalkable(n.x, n.y);
                if (!walkable && (isProtected[index(n.x, n.y)] || map.tileAt(n.x, n.y).type != TileType::Wall)) continue;
                const int c = cost[index(p.x, p.y)] + (walkable ? 0 : 1);
                if (c >= cost[index(n.x, n.y)]) continue;
                cost[index(n.x, n.y)] = c;
                from[index(n.x, n.y)] = static_cast<int>(index(p.x, p.y));
                if (walkable) open.push_front(n); else open.push_back(n);
            }
        }
        if (goal.x < 0) {
            // Nothing to carve to: give the pocket up rather than loop forever.
            for (int y = 0; y < h; ++y)
                for (int x = 0; x < w; ++x)
                    if (map.isWalkable(x, y) && !reached[index(x, y)] && !ignore[index(x, y)])
                        map.setTile(x, y, Tile{TileType::Wall, false, false});
            return;
        }
        for (int i = static_cast<int>(index(goal.x, goal.y)); i >= 0; i = from[static_cast<std::size_t>(i)])
            if (!map.isWalkable(i % w, i / w)) map.setTile(i % w, i / w, Tile{TileType::Floor, true, true});
    }
}

} // namespace

GeneratedDungeon generateDungeon(const DungeonGenerationParams& params, unsigned int seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> roll(0.f, 1.f);
    const auto pickFrom = [&](const auto& items) {
        return items[std::uniform_int_distribution<std::size_t>(0, items.size() - 1)(rng)];
    };

    // --- The floor's shape -------------------------------------------------
    // A few cells are solid rock, so floors come in L, U and ring shapes,
    // never cut in two. Then a pair of neighbouring cells may merge into one
    // big room.
    std::array<bool, kCells> active{};
    active.fill(true);
    const int rock = std::discrete_distribution<int>(kRockWeights.begin(), kRockWeights.end())(rng);
    for (int n = 0; n < rock; ++n) {
        std::vector<int> candidates;
        for (int cell = 0; cell < kCells; ++cell) {
            if (!active[cell]) continue;
            auto without = active;
            without[cell] = false;
            if (connectedCells(without)) candidates.push_back(cell);
        }
        if (!candidates.empty()) active[pickFrom(candidates)] = false;
    }
    std::array<int, kCells> roomOf{};
    roomOf.fill(-1);
    std::vector<Room> rooms;
    std::vector<Edge> edges = gridEdges();
    const int merges = std::discrete_distribution<int>(kBigRoomWeights.begin(), kBigRoomWeights.end())(rng);
    for (int n = 0; n < merges; ++n) {
        std::vector<Edge> candidates;
        for (const Edge& e : edges)
            if (active[e.a] && active[e.b] && roomOf[e.a] < 0 && roomOf[e.b] < 0) candidates.push_back(e);
        if (candidates.empty()) break;
        const Edge e = pickFrom(candidates);
        const bool across = e.b == e.a + 1;
        rooms.push_back({e.a % kModuleGrid, e.a / kModuleGrid, across ? 2 : 1, across ? 1 : 2, {e.a, e.b}});
        roomOf[e.a] = roomOf[e.b] = static_cast<int>(rooms.size()) - 1;
    }
    for (int cell = 0; cell < kCells; ++cell)
        if (active[cell] && roomOf[cell] < 0) {
            rooms.push_back({cell % kModuleGrid, cell / kModuleGrid, 1, 1, {cell}});
            roomOf[cell] = static_cast<int>(rooms.size()) - 1;
        }
    const int roomTotal = static_cast<int>(rooms.size());

    // --- Which neighbor pairs are joined -----------------------------------
    // Random spanning tree first (Kruskal over shuffled edges) so every
    // room is reachable, then extra openings on top. A big room's two cells
    // are already one.
    std::shuffle(edges.begin(), edges.end(), rng);
    std::array<int, kCells> parent{};
    std::iota(parent.begin(), parent.end(), 0);
    for (const Room& room : rooms)
        if (!room.single()) parent[findRoot(parent, room.cells[0])] = findRoot(parent, room.cells[1]);
    std::array<std::array<bool, kCells>, kCells> open{};
    for (const Edge& e : edges) {
        if (!active[e.a] || !active[e.b] || roomOf[e.a] == roomOf[e.b]) continue;
        const int ra = findRoot(parent, e.a), rb = findRoot(parent, e.b);
        const bool treeEdge = ra != rb;
        if (treeEdge) parent[ra] = rb;
        if (treeEdge || roll(rng) < kExtraConnectionChance) open[e.a][e.b] = open[e.b][e.a] = true;
    }

    // --- Start, finish, vault ---------------------------------------------
    std::vector<int> edgeCells;
    for (int cell = 0; cell < kCells; ++cell)
        if (active[cell] && cell != kCells / 2) edgeCells.push_back(cell);
    const int startRoom = roomOf[pickFrom(edgeCells)];

    std::vector<int> distance(static_cast<std::size_t>(roomTotal), -1);
    std::queue<int> frontier;
    distance[static_cast<std::size_t>(startRoom)] = 0;
    frontier.push(startRoom);
    while (!frontier.empty()) {
        const int room = frontier.front();
        frontier.pop();
        for (const int a : rooms[static_cast<std::size_t>(room)].cells)
            for (int b = 0; b < kCells; ++b) {
                if (!open[a][b]) continue;
                const int next = roomOf[b];
                if (distance[static_cast<std::size_t>(next)] < 0) {
                    distance[static_cast<std::size_t>(next)] = distance[static_cast<std::size_t>(room)] + 1;
                    frontier.push(next);
                }
            }
    }
    // Nearest rooms first; ties in a random order.
    std::vector<int> order(static_cast<std::size_t>(roomTotal));
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        return distance[static_cast<std::size_t>(a)] < distance[static_cast<std::size_t>(b)];
    });
    // The boss needs a single cell: the farthest one.
    int finalRoom = order.back();
    if (params.includeBossRoom)
        for (auto it = order.rbegin(); it != order.rend(); ++it)
            if (rooms[static_cast<std::size_t>(*it)].single() && *it != startRoom) { finalRoom = *it; break; }
    const int bossRoom = params.includeBossRoom ? finalRoom : -1;
    // Set pieces take single cells away from the start and the finish.
    std::vector<int> taken{startRoom, finalRoom};
    const auto freeSingles = [&] {
        std::vector<int> candidates;
        for (int room = 0; room < roomTotal; ++room)
            if (rooms[static_cast<std::size_t>(room)].single() && std::find(taken.begin(), taken.end(), room) == taken.end())
                candidates.push_back(room);
        return candidates;
    };

    int vaultRoom = -1;
    if (params.includeVault && !params.includeBossRoom) {
        const auto candidates = freeSingles();
        if (!candidates.empty()) { vaultRoom = pickFrom(candidates); taken.push_back(vaultRoom); }
    }

    // Landmark: any single cell except the start, the final room and the vault.
    int landmarkRoom = -1;
    const bool rareEvent = !params.includeBossRoom && params.rareEventChance > 0.f &&
                           std::uniform_real_distribution<float>(0.f, 1.f)(rng) < params.rareEventChance;
    if (!params.includeBossRoom && (rareEvent || std::uniform_real_distribution<float>(0.f, 1.f)(rng) < params.landmarkChance)) {
        const auto candidates = freeSingles();
        if (!candidates.empty()) { landmarkRoom = pickFrom(candidates); taken.push_back(landmarkRoom); }
    }
    // Which set piece: shrines are commonest near the surface, dark rites below.
    // Shrine, Healing Fountain, Blood Font, Ritual Circle, Treasure Hoard, Prisoner's Cage, Champion's Pit
    // ..., and last the Strongbox, Breach and Essence.
    std::array<int, 10> landmarkWeights{3, 2, 1, 1, 2, 2, 2, 3, 2, 2};
    if (params.region == FloorRegion::Sanctum) landmarkWeights = {2, 2, 1, 2, 2, 2, 2, 3, 2, 2};
    if (params.region == FloorRegion::Crypts) landmarkWeights = {1, 1, 2, 3, 2, 1, 2, 3, 2, 2};
    const auto weightedKind = [&] {
        const int i = std::discrete_distribution<int>(landmarkWeights.begin(), landmarkWeights.end())(rng);
        return i == 7 ? LandmarkKind::Strongbox : i == 8 ? LandmarkKind::Breach : i == 9 ? LandmarkKind::Essence : static_cast<LandmarkKind>(i + 1);
    };
    auto landmarkKind = weightedKind();
    // About one ordinary landmark in eight is a lamplighter's rest instead.
    if (!rareEvent && landmarkRoom >= 0 && std::uniform_int_distribution<int>(0, 7)(rng) == 0) landmarkKind = LandmarkKind::LamplighterRest;
    if (!rareEvent && landmarkRoom >= 0 && params.bloodAltarChance > 0.f &&
        std::uniform_real_distribution<float>(0.f, 1.f)(rng) < params.bloodAltarChance) landmarkKind = LandmarkKind::BloodAltar;
    if (rareEvent) landmarkKind = static_cast<LandmarkKind>(static_cast<int>(LandmarkKind::SealedTomb) + std::uniform_int_distribution<int>(0, 2)(rng));
    // More events: a second (and then a third) ordinary landmark, each in a
    // room of its own and of a kind not already on the floor.
    std::vector<std::pair<int, LandmarkKind>> extraRooms;
    if (landmarkRoom >= 0) {
        for (const float chance : {params.secondLandmarkChance, params.thirdLandmarkChance}) {
            if (roll(rng) >= chance) break;
            const auto candidates = freeSingles();
            if (candidates.empty()) break;
            LandmarkKind kind = landmarkKind;
            for (int tries = 0; tries < 12; ++tries) {
                kind = weightedKind();
                const bool repeated = kind == landmarkKind ||
                    std::any_of(extraRooms.begin(), extraRooms.end(), [&](const auto& e) { return e.second == kind; });
                if (!repeated) break;
            }
            extraRooms.push_back({pickFrom(candidates), kind});
            taken.push_back(extraRooms.back().first);
        }
    }
    const auto extraKind = [&](int room) -> LandmarkKind {
        for (const auto& e : extraRooms) if (e.first == room) return e.second;
        return LandmarkKind::None;
    };
    // Set pieces keep their doorways in the middle of each side.
    const auto setPiece = [&](int room) {
        return room == bossRoom || room == vaultRoom || room == landmarkRoom || extraKind(room) != LandmarkKind::None;
    };

    // --- Doorways ------------------------------------------------------------
    // Each opened join gets a doorway before, at or past the middle of its
    // edge, the same on both sides.
    std::array<std::array<int, kCells>, kCells> shift{};
    for (const Edge& e : gridEdges()) {
        if (!open[e.a][e.b]) continue;
        const int s = setPiece(roomOf[e.a]) || setPiece(roomOf[e.b])
                          ? 0 : std::discrete_distribution<int>(kDoorwayWeights.begin(), kDoorwayWeights.end())(rng) - 1;
        shift[e.a][e.b] = shift[e.b][e.a] = s;
    }
    // A room's doorways, in its own coordinates.
    const auto doorwaysOf = [&](const Room& room) {
        std::vector<Doorway> doorways;
        for (const int cell : room.cells) {
            const int c = cell % kModuleGrid, r = cell / kModuleGrid;
            const int lx = (c - room.col) * kModuleWidth, ly = (r - room.row) * kModuleHeight;
            const auto add = [&](int other, Side side) {
                if (other < 0 || !open[cell][other]) return;
                const int s = shift[cell][other];
                const bool horizontal = side == Side::Top || side == Side::Bottom;
                doorways.push_back({side, horizontal ? lx + 9 + 6 * s : ly + 5 + 3 * s});
            };
            add(r > 0 ? cell - kModuleGrid : -1, Side::Top);
            add(r + 1 < kModuleGrid ? cell + kModuleGrid : -1, Side::Bottom);
            add(c > 0 ? cell - 1 : -1, Side::Left);
            add(c + 1 < kModuleGrid ? cell + 1 : -1, Side::Right);
        }
        return doorways;
    };

    // --- Pick and stamp modules -------------------------------------------
    const auto& pool = regularModules();
    std::vector<std::size_t> picks(pool.size());
    std::iota(picks.begin(), picks.end(), std::size_t{0});
    std::shuffle(picks.begin(), picks.end(), rng);
    std::size_t nextPick = 0;
    std::vector<std::size_t> bigPicks(bigModules().size());
    std::iota(bigPicks.begin(), bigPicks.end(), std::size_t{0});
    std::shuffle(bigPicks.begin(), bigPicks.end(), rng);

    GeneratedDungeon result;
    result.map = Map(kModuleGrid * kModuleWidth, kModuleGrid * kModuleHeight);
    result.moduleNames.assign(kCells, "Solid Rock");
    result.cellRooms.assign(roomOf.begin(), roomOf.end());
    const int w = result.map.width(), h = result.map.height();
    std::vector<bool> isProtected(static_cast<std::size_t>(w * h), false), ignore(static_cast<std::size_t>(w * h), false);
    const auto tileIndex = [w](int x, int y) { return static_cast<std::size_t>(y * w + x); };
    std::vector<std::vector<Position>> anchors(static_cast<std::size_t>(roomTotal));
    std::vector<std::vector<Post>> posts(static_cast<std::size_t>(roomTotal));
    std::uniform_int_distribution<int> coin(0, 1);

    std::uniform_real_distribution<float> share(0.f, 1.f);
    for (int index = 0; index < roomTotal; ++index) {
        const Room& room = rooms[static_cast<std::size_t>(index)];
        const LandmarkKind extra = extraKind(index);
        const bool landmarkHere = index == landmarkRoom || extra != LandmarkKind::None;
        const auto doorways = doorwaysOf(room);
        bool procedural = !setPiece(index) && share(rng) < params.proceduralShare;
        // A big room with no hand-made module left is generated.
        const ModuleTemplate* big = nullptr;
        if (!room.single() && !procedural) {
            const auto found = std::find_if(bigPicks.begin(), bigPicks.end(), [&](std::size_t b) {
                const auto& m = bigModules()[b];
                return static_cast<int>(m.rows.size()) == room.height() && static_cast<int>(m.rows.front().size()) == room.width();
            });
            if (found != bigPicks.end()) { big = &bigModules()[*found]; bigPicks.erase(found); }
            procedural = big == nullptr;
        }
        ModuleTemplate generated;
        if (procedural)
            generated = proceduralModule(pickProceduralStyle(params.region, rng), room.cellsWide, room.cellsHigh, doorways, rng);
        const ModuleTemplate& module = index == bossRoom ? bossModule()
                                     : index == vaultRoom ? vaultModule()
                                     : index == landmarkRoom ? landmarkModules()[landmarkModuleIndex(landmarkKind)]
                                     : extra != LandmarkKind::None ? landmarkModules()[landmarkModuleIndex(extra)]
                                     : procedural ? generated
                                     : big ? *big
                                     : pool[picks[nextPick++ % picks.size()]];
        for (const int cell : room.cells) result.moduleNames[static_cast<std::size_t>(cell)] = module.name;
        // Generated rooms are built around their doorways, so they stay as they are.
        const bool flipX = !procedural && coin(rng) == 1, flipY = !procedural && coin(rng) == 1 && !landmarkHere;
        const auto rows = mirrored(module.rows, flipX, flipY);
        for (int y = 0; y < room.height(); ++y)
            for (int x = 0; x < room.width(); ++x) {
                const char c = rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
                const Position p{room.x0() + x, room.y0() + y};
                const bool wall = c == '#' || c == 'V' || c == 'S';
                result.map.setTile(p.x, p.y, c == '_' ? Tile{TileType::Chasm, false, true}
                                             : wall ? Tile{TileType::Wall, false, false}
                                                    : Tile{TileType::Floor, true, true});
                const bool border = x == 0 || y == 0 || x == room.width() - 1 || y == room.height() - 1;
                if (border || c == '_' || c == 'S') isProtected[tileIndex(p.x, p.y)] = true;
                if (c == 'A') anchors[static_cast<std::size_t>(index)].push_back(p);
                if (c == 'P' || c == 'R' || c == 'H')
                    posts[static_cast<std::size_t>(index)].push_back({c == 'P' ? PostKind::Guard : c == 'R' ? PostKind::Ledge : PostKind::Hidden, p});
                if (c == 'C') result.vaultCenter = p;
                if (c == 'V') result.vaultEntrance = p;
                if (c == 'S') {
                    if (index == landmarkRoom) result.landmarkAltar = p;
                    else result.extraLandmarks.push_back({extra, p});
                }
            }
        if (anchors[static_cast<std::size_t>(index)].empty() && !landmarkHere)
            anchors[static_cast<std::size_t>(index)].push_back({room.x0() + room.width() / 2, room.y0() + room.height() / 2});

        // The border is wall except this room's doorways; the tiles just
        // inside a moved doorway open too (set pieces keep theirs as drawn).
        for (int y = 0; y < room.height(); ++y)
            for (int x = 0; x < room.width(); ++x)
                if (x == 0 || y == 0 || x == room.width() - 1 || y == room.height() - 1)
                    result.map.setTile(room.x0() + x, room.y0() + y, Tile{TileType::Wall, false, false});
        for (const Doorway& d : doorways)
            for (int i = 0; i < 3; ++i)
                for (int depth = 0; depth < (setPiece(index) ? 1 : 2); ++depth) {
                    const bool horizontal = d.side == Side::Top || d.side == Side::Bottom;
                    const int x = horizontal ? d.at + i : d.side == Side::Left ? depth : room.width() - 1 - depth;
                    const int y = !horizontal ? d.at + i : d.side == Side::Top ? depth : room.height() - 1 - depth;
                    const Position p{room.x0() + x, room.y0() + y};
                    if (depth > 0 && result.map.tileAt(p.x, p.y).type != TileType::Wall) continue;
                    if (depth > 0 && isProtected[tileIndex(p.x, p.y)]) continue;
                    result.map.setTile(p.x, p.y, Tile{TileType::Floor, true, true});
                    if (depth == 0) result.doorways.push_back(p);
                }
    }
    if (vaultRoom >= 0)
        for (int y = result.vaultCenter.y - 3; y <= result.vaultCenter.y + 3; ++y)
            for (int x = result.vaultCenter.x - 3; x <= result.vaultCenter.x + 3; ++x) {
                isProtected[tileIndex(x, y)] = true;
                if (std::abs(x - result.vaultCenter.x) <= 2 && std::abs(y - result.vaultCenter.y) <= 2) ignore[tileIndex(x, y)] = true;
            }

    // --- Describe the floor to the caller ----------------------------------
    result.hasVault = vaultRoom >= 0;
    result.playerStart = anchors[static_cast<std::size_t>(startRoom)].front();
    // A moved doorway may open onto a hand-made room's wall: carve the
    // shortest way in, so the floor is one connected space.
    connectAll(result.map, result.playerStart, isProtected, ignore);
    // On a floor without a boss the final room is the farthest, so its
    // anchors come last and the last of them hosts the door down.
    for (const int room : order) {
        if (room == startRoom || room == bossRoom) continue;
        const auto& here = anchors[static_cast<std::size_t>(room)];
        for (std::size_t i = 0; i < here.size(); ++i) {
            result.otherRoomCenters.push_back(here[i]);
            result.roomPosts.push_back(i == 0 ? posts[static_cast<std::size_t>(room)] : std::vector<Post>{});
        }
    }
    result.hasBossRoom = bossRoom >= 0;
    if (result.hasBossRoom) {
        const Room& room = rooms[static_cast<std::size_t>(bossRoom)];
        result.bossRoomCenter = {room.x0() + kModuleWidth / 2, room.y0() + kModuleHeight / 2};
    }
    result.roomCount = roomTotal;
    if (landmarkRoom >= 0) result.landmark = landmarkKind;
    placeProps(result, params.region, params.cathedral, rng);
    return result;
}

bool bossFloor(int floor) {
    return floor == 5 || floor == 10 || floor == kRunFinalFloor || floor == kCathedralLast || floor == kFoundryLast ||
           floor == kThornLast || floor == kRimeLast;
}

DungeonGenerationParams floorGenerationParams(int floor, bool bloodMagicUnlocked) {
    DungeonGenerationParams params;
    params.region = floorTheme(floor).region;
    // Very rare events only stir on deep floors.
    params.rareEventChance = floor >= kRareEventFloor ? kRareEventChance : 0.f;
    // The Blood Altar waits from floor 4 until you have made your offering.
    params.bloodAltarChance = floor >= kBloodAltarFloor && !bloodMagicUnlocked ? 0.3f : 0.f;
    params.includeBossRoom = bossFloor(floor);
    params.cathedral = cathedralFloor(floor);
    params.includeVault = floor >= 3 && !params.includeBossRoom;
    return params;
}

} // namespace engine
