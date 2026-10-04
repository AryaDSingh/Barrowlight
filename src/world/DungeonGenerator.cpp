#include "world/DungeonGenerator.hpp"

#include <algorithm>
#include <cstdlib>
#include <array>
#include <numeric>
#include <queue>
#include <random>

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
void placeProps(GeneratedDungeon& d, FloorRegion region, std::mt19937& rng) {
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
        const int lx = p.x % kModuleWidth, ly = p.y % kModuleHeight;
        if ((lx >= 8 && lx <= 12 && (ly <= 2 || ly >= kModuleHeight - 3)) ||
            (ly >= 4 && ly <= 8 && (lx <= 2 || lx >= kModuleWidth - 3))) return false; // keep sockets clear
        if (near(p, d.playerStart, 2)) return false;
        for (const Position& a : d.otherRoomCenters) if (near(p, a, 2)) return false;
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
}

constexpr int kCells = kModuleGrid * kModuleGrid;
// Chance that a neighbor pair not already joined by the spanning tree is
// opened anyway. High on purpose: the floor should feel like one open
// space with several routes, not a single winding path.
constexpr float kExtraConnectionChance = 0.65f;

struct Edge { int a, b; };

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

} // namespace

GeneratedDungeon generateDungeon(const DungeonGenerationParams& params, unsigned int seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> roll(0.f, 1.f);

    // --- Which neighbor pairs are joined -----------------------------------
    // Random spanning tree first (Kruskal over shuffled edges) so every
    // cell is reachable, then extra openings on top.
    std::vector<Edge> edges = gridEdges();
    std::shuffle(edges.begin(), edges.end(), rng);
    std::array<int, kCells> parent{};
    std::iota(parent.begin(), parent.end(), 0);
    std::array<std::array<bool, kCells>, kCells> open{};
    for (const Edge& e : edges) {
        const int ra = findRoot(parent, e.a), rb = findRoot(parent, e.b);
        const bool treeEdge = ra != rb;
        if (treeEdge) parent[ra] = rb;
        if (treeEdge || roll(rng) < kExtraConnectionChance) open[e.a][e.b] = open[e.b][e.a] = true;
    }

    // --- Start, finish, vault ---------------------------------------------
    std::vector<int> edgeCells;
    for (int cell = 0; cell < kCells; ++cell)
        if (cell != kCells / 2) edgeCells.push_back(cell);
    const int startCell = edgeCells[std::uniform_int_distribution<std::size_t>(0, edgeCells.size() - 1)(rng)];

    std::array<int, kCells> distance{};
    distance.fill(-1);
    std::queue<int> frontier;
    distance[startCell] = 0;
    frontier.push(startCell);
    while (!frontier.empty()) {
        const int cell = frontier.front();
        frontier.pop();
        for (int next = 0; next < kCells; ++next)
            if (open[cell][next] && distance[next] < 0) {
                distance[next] = distance[cell] + 1;
                frontier.push(next);
            }
    }
    // Nearest modules first; ties in a random order.
    std::vector<int> order(kCells);
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return distance[a] < distance[b]; });
    const int finalCell = order.back();

    int vaultCell = -1;
    if (params.includeVault && !params.includeBossRoom) {
        std::vector<int> candidates;
        for (int cell = 0; cell < kCells; ++cell)
            if (cell != startCell && cell != finalCell) candidates.push_back(cell);
        vaultCell = candidates[std::uniform_int_distribution<std::size_t>(0, candidates.size() - 1)(rng)];
    }
    const int bossCell = params.includeBossRoom ? finalCell : -1;

    // Landmark: any cell except the start, the final cell and the vault.
    int landmarkCell = -1;
    const bool rareEvent = !params.includeBossRoom && params.rareEventChance > 0.f &&
                           std::uniform_real_distribution<float>(0.f, 1.f)(rng) < params.rareEventChance;
    if (!params.includeBossRoom && (rareEvent || std::uniform_real_distribution<float>(0.f, 1.f)(rng) < params.landmarkChance)) {
        std::vector<int> candidates;
        for (int cell = 0; cell < kCells; ++cell)
            if (cell != startCell && cell != finalCell && cell != vaultCell) candidates.push_back(cell);
        landmarkCell = candidates[std::uniform_int_distribution<std::size_t>(0, candidates.size() - 1)(rng)];
    }
    // Which set piece: shrines are commonest near the surface, dark rites below.
    // Shrine, Healing Fountain, Blood Font, Ritual Circle, Treasure Hoard, Prisoner's Cage, Champion's Pit
    // ..., and last the Strongbox.
    std::array<int, 8> landmarkWeights{3, 2, 1, 1, 2, 2, 2, 3};
    if (params.region == FloorRegion::Sanctum) landmarkWeights = {2, 2, 1, 2, 2, 2, 2, 3};
    if (params.region == FloorRegion::Crypts) landmarkWeights = {1, 1, 2, 3, 2, 1, 2, 3};
    const auto weightedKind = [&] {
        const int i = std::discrete_distribution<int>(landmarkWeights.begin(), landmarkWeights.end())(rng);
        return i == 7 ? LandmarkKind::Strongbox : static_cast<LandmarkKind>(i + 1);
    };
    auto landmarkKind = weightedKind();
    // About one ordinary landmark in eight is a lamplighter's rest instead.
    if (!rareEvent && landmarkCell >= 0 && std::uniform_int_distribution<int>(0, 7)(rng) == 0) landmarkKind = LandmarkKind::LamplighterRest;
    if (!rareEvent && landmarkCell >= 0 && params.bloodAltarChance > 0.f &&
        std::uniform_real_distribution<float>(0.f, 1.f)(rng) < params.bloodAltarChance) landmarkKind = LandmarkKind::BloodAltar;
    if (rareEvent) landmarkKind = static_cast<LandmarkKind>(static_cast<int>(LandmarkKind::SealedTomb) + std::uniform_int_distribution<int>(0, 2)(rng));
    // More events: a second (and then a third) ordinary landmark, each in a
    // cell of its own and of a kind not already on the floor.
    std::vector<std::pair<int, LandmarkKind>> extraCells;
    if (landmarkCell >= 0) {
        std::uniform_real_distribution<float> roll(0.f, 1.f);
        for (const float chance : {params.secondLandmarkChance, params.thirdLandmarkChance}) {
            if (roll(rng) >= chance) break;
            std::vector<int> candidates;
            for (int cell = 0; cell < kCells; ++cell) {
                const bool taken = cell == startCell || cell == finalCell || cell == vaultCell || cell == landmarkCell ||
                    std::any_of(extraCells.begin(), extraCells.end(), [&](const auto& e) { return e.first == cell; });
                if (!taken) candidates.push_back(cell);
            }
            if (candidates.empty()) break;
            LandmarkKind kind = landmarkKind;
            for (int tries = 0; tries < 12; ++tries) {
                kind = weightedKind();
                const bool repeated = kind == landmarkKind ||
                    std::any_of(extraCells.begin(), extraCells.end(), [&](const auto& e) { return e.second == kind; });
                if (!repeated) break;
            }
            extraCells.push_back({candidates[std::uniform_int_distribution<std::size_t>(0, candidates.size() - 1)(rng)], kind});
        }
    }
    const auto extraKind = [&](int cell) -> LandmarkKind {
        for (const auto& e : extraCells) if (e.first == cell) return e.second;
        return LandmarkKind::None;
    };

    // --- Pick and stamp modules -------------------------------------------
    const auto& pool = regularModules();
    std::vector<std::size_t> picks(pool.size());
    std::iota(picks.begin(), picks.end(), std::size_t{0});
    std::shuffle(picks.begin(), picks.end(), rng);
    std::size_t nextPick = 0;

    GeneratedDungeon result;
    result.map = Map(kModuleGrid * kModuleWidth, kModuleGrid * kModuleHeight);
    result.moduleNames.resize(kCells);
    std::array<std::vector<Position>, kCells> anchors;
    std::uniform_int_distribution<int> coin(0, 1);

    std::uniform_real_distribution<float> share(0.f, 1.f);
    for (int cell = 0; cell < kCells; ++cell) {
        ModuleTemplate generated;
        const LandmarkKind extra = extraKind(cell);
        const bool landmarkHere = cell == landmarkCell || extra != LandmarkKind::None;
        const bool special = cell == bossCell || cell == vaultCell || landmarkHere;
        const bool procedural = !special && share(rng) < params.proceduralShare;
        if (procedural) generated = proceduralModule(pickProceduralStyle(params.region, rng), rng);
        const ModuleTemplate& module = cell == bossCell ? bossModule()
                                     : cell == vaultCell ? vaultModule()
                                     : cell == landmarkCell ? landmarkModules()[landmarkModuleIndex(landmarkKind)]
                                     : extra != LandmarkKind::None ? landmarkModules()[landmarkModuleIndex(extra)]
                                     : procedural ? generated
                                     : pool[picks[nextPick++ % picks.size()]];
        result.moduleNames[cell] = module.name;
        const bool flipX = coin(rng) == 1, flipY = coin(rng) == 1 && !landmarkHere;
        const auto rows = mirrored(module.rows, flipX, flipY);
        const int originX = (cell % kModuleGrid) * kModuleWidth;
        const int originY = (cell / kModuleGrid) * kModuleHeight;
        for (int y = 0; y < kModuleHeight; ++y)
            for (int x = 0; x < kModuleWidth; ++x) {
                const char c = rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
                const Position p{originX + x, originY + y};
                const bool wall = c == '#' || c == 'V' || c == 'S';
                result.map.setTile(p.x, p.y, wall ? Tile{TileType::Wall, false, false}
                                                  : Tile{TileType::Floor, true, true});
                if (c == 'A') anchors[cell].push_back(p);
                if (c == 'C') result.vaultCenter = p;
                if (c == 'V') result.vaultEntrance = p;
                if (c == 'S') {
                    if (cell == landmarkCell) result.landmarkAltar = p;
                    else result.extraLandmarks.push_back({extra, p});
                }
            }
        if (anchors[cell].empty() && !landmarkHere)
            anchors[cell].push_back({originX + kModuleWidth / 2, originY + kModuleHeight / 2});

        // Seal every socket that doesn't lead into an opened neighbor
        // (including all of them on the floor's outer edge).
        const int col = cell % kModuleGrid, row = cell / kModuleGrid;
        const auto sealIfClosed = [&](bool hasNeighbor, int neighbor, auto sealTile) {
            if (!hasNeighbor || !open[cell][neighbor])
                for (int i = 0; i < 3; ++i) sealTile(i);
        };
        const Tile wall{TileType::Wall, false, false};
        sealIfClosed(row > 0, cell - kModuleGrid, [&](int i) { result.map.setTile(originX + 9 + i, originY, wall); });
        sealIfClosed(row + 1 < kModuleGrid, cell + kModuleGrid,
                     [&](int i) { result.map.setTile(originX + 9 + i, originY + kModuleHeight - 1, wall); });
        sealIfClosed(col > 0, cell - 1, [&](int i) { result.map.setTile(originX, originY + 5 + i, wall); });
        sealIfClosed(col + 1 < kModuleGrid, cell + 1,
                     [&](int i) { result.map.setTile(originX + kModuleWidth - 1, originY + 5 + i, wall); });
    }

    // --- Describe the floor to the caller ----------------------------------
    result.hasVault = vaultCell >= 0;
    result.playerStart = anchors[startCell].front();
    for (const int cell : order) {
        if (cell == startCell || cell == bossCell) continue;
        for (const Position& anchor : anchors[cell]) result.otherRoomCenters.push_back(anchor);
    }
    result.hasBossRoom = bossCell >= 0;
    if (result.hasBossRoom)
        result.bossRoomCenter = {(bossCell % kModuleGrid) * kModuleWidth + kModuleWidth / 2,
                                 (bossCell / kModuleGrid) * kModuleHeight + kModuleHeight / 2};
    result.roomCount = kCells;
    if (landmarkCell >= 0) result.landmark = landmarkKind;
    placeProps(result, params.region, rng);
    return result;
}

} // namespace engine
