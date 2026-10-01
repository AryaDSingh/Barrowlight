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

// Props stand against walls, away from encounters, the start, sockets and
// set pieces, and only where they leave every floor tile reachable.
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
    const auto near = [](Position a, Position b, int r) { return std::abs(a.x - b.x) <= r && std::abs(a.y - b.y) <= r; };
    const auto allowed = [&](Position p) {
        if (!map.isWalkable(p.x, p.y) || map.tileAt(p.x, p.y).type != TileType::Floor) return false;
        const int lx = p.x % kModuleWidth, ly = p.y % kModuleHeight;
        if ((lx >= 8 && lx <= 12 && (ly <= 2 || ly >= kModuleHeight - 3)) ||
            (ly >= 4 && ly <= 8 && (lx <= 2 || lx >= kModuleWidth - 3))) return false; // keep sockets clear
        if (near(p, d.playerStart, 2)) return false;
        for (const Position& a : d.otherRoomCenters) if (near(p, a, 2)) return false;
        if (d.hasBossRoom && near(p, d.bossRoomCenter, 4)) return false;
        if (d.hasVault && near(p, d.vaultCenter, 4)) return false;
        if (d.landmark != LandmarkKind::None && near(p, d.landmarkAltar, 4)) return false;
        for (const Prop& prop : d.props)
            for (int i = 0; i < propWidth(prop.kind); ++i)
                if (near(p, {prop.pos.x + i, prop.pos.y}, 1)) return false; // never clumped
        // Against a wall: props line rooms instead of floating in them.
        const auto wall = [&](int x, int y) { return map.inBounds(x, y) && map.tileAt(x, y).type == TileType::Wall; };
        return wall(p.x, p.y - 1) || wall(p.x - 1, p.y) || wall(p.x + 1, p.y);
    };

    // Weights per kind: Barrel, Crate, Sacks, Throne, SkeletonThrone, Statue.
    std::array<int, 6> weights{4, 4, 2, 1, 0, 0};
    if (region == FloorRegion::Sanctum) weights = {1, 1, 0, 2, 0, 2};
    if (region == FloorRegion::Crypts) weights = {0, 1, 0, 1, 2, 1};
    std::discrete_distribution<int> pickKind(weights.begin(), weights.end());

    std::vector<Position> candidates;
    for (int y = 1; y < h - 1; ++y)
        for (int x = 1; x < w - 1; ++x) candidates.push_back({x, y});
    std::shuffle(candidates.begin(), candidates.end(), rng);
    const int target = std::uniform_int_distribution<int>(10, 16)(rng);
    int baseline = reachable();
    for (const Position p : candidates) {
        if (static_cast<int>(d.props.size()) >= target) break;
        const auto kind = static_cast<PropKind>(pickKind(rng) + 1);
        const int width = propWidth(kind);
        bool fits = true;
        for (int i = 0; i < width && fits; ++i) fits = allowed({p.x + i, p.y});
        if (!fits) continue;
        for (int i = 0; i < width; ++i) map.setTile(p.x + i, p.y, Tile{TileType::Wall, false, true});
        const int now = reachable();
        if (now != baseline - width) {
            for (int i = 0; i < width; ++i) map.setTile(p.x + i, p.y, Tile{TileType::Floor, true, true});
            continue;
        }
        baseline = now;
        d.props.push_back({kind, p});
    }
}

constexpr int kCells = kModuleGrid * kModuleGrid;
// Chance that a neighbor pair not already joined by the spanning tree is
// opened anyway. High on purpose: the floor should feel like one open
// space with several routes, not a single winding path.
constexpr float kExtraConnectionChance = 0.65f;

struct Edge { int a, b; };

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
    if (!params.includeBossRoom && std::uniform_real_distribution<float>(0.f, 1.f)(rng) < params.landmarkChance) {
        std::vector<int> candidates;
        for (int cell = 0; cell < kCells; ++cell)
            if (cell != startCell && cell != finalCell && cell != vaultCell) candidates.push_back(cell);
        landmarkCell = candidates[std::uniform_int_distribution<std::size_t>(0, candidates.size() - 1)(rng)];
    }

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
        const bool special = cell == bossCell || cell == vaultCell || cell == landmarkCell;
        const bool procedural = !special && share(rng) < params.proceduralShare;
        if (procedural) generated = proceduralModule(pickProceduralStyle(params.region, rng), rng);
        const ModuleTemplate& module = cell == bossCell ? bossModule()
                                     : cell == vaultCell ? vaultModule()
                                     : cell == landmarkCell ? landmarkModules()[0]
                                     : procedural ? generated
                                     : pool[picks[nextPick++ % picks.size()]];
        result.moduleNames[cell] = module.name;
        const auto rows = mirrored(module.rows, coin(rng) == 1, coin(rng) == 1);
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
                if (c == 'S') result.landmarkAltar = p;
            }
        if (anchors[cell].empty() && cell != landmarkCell)
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
    if (landmarkCell >= 0) result.landmark = LandmarkKind::Shrine;
    placeProps(result, params.region, rng);
    return result;
}

} // namespace engine
