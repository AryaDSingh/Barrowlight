#include "world/DungeonGenerator.hpp"

#include <algorithm>
#include <array>
#include <numeric>
#include <queue>
#include <random>

#include "world/DungeonModules.hpp"

namespace engine {

namespace {

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

    for (int cell = 0; cell < kCells; ++cell) {
        const ModuleTemplate& module = cell == bossCell ? bossModule()
                                     : cell == vaultCell ? vaultModule()
                                     : pool[picks[nextPick++ % picks.size()]];
        result.moduleNames[cell] = module.name;
        const auto rows = mirrored(module.rows, coin(rng) == 1, coin(rng) == 1);
        const int originX = (cell % kModuleGrid) * kModuleWidth;
        const int originY = (cell / kModuleGrid) * kModuleHeight;
        for (int y = 0; y < kModuleHeight; ++y)
            for (int x = 0; x < kModuleWidth; ++x) {
                const char c = rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
                const Position p{originX + x, originY + y};
                const bool wall = c == '#' || c == 'V';
                result.map.setTile(p.x, p.y, wall ? Tile{TileType::Wall, false, false}
                                                  : Tile{TileType::Floor, true, true});
                if (c == 'A') anchors[cell].push_back(p);
                if (c == 'C') result.vaultCenter = p;
                if (c == 'V') result.vaultEntrance = p;
            }
        if (anchors[cell].empty())
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
    return result;
}

} // namespace engine
