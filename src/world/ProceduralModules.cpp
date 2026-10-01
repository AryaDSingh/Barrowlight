#include "world/ProceduralModules.hpp"

#include <algorithm>
#include <array>
#include <queue>

namespace engine {

namespace {

constexpr int W = kModuleWidth, H = kModuleHeight;
constexpr int kCenterX = W / 2, kCenterY = H / 2;
constexpr float kMinFloorShare = 0.5f;
constexpr int kAttempts = 40;

using Grid = std::array<std::array<char, W>, H>;

int roll(std::mt19937& rng, int low, int high) { return std::uniform_int_distribution<int>(low, high)(rng); }
bool chance(std::mt19937& rng, float p) { return std::uniform_real_distribution<float>(0.f, 1.f)(rng) < p; }

void fillInterior(Grid& g, char c) {
    for (int y = 1; y < H - 1; ++y)
        for (int x = 1; x < W - 1; ++x) g[y][x] = c;
}

// Sockets, the tiles just inside them, and the centre are always open: the
// sockets join neighbouring cells and the centre hosts encounters.
void openEssentials(Grid& g) {
    for (int i = 9; i <= 11; ++i) {
        g[0][i] = g[1][i] = '.';
        g[H - 1][i] = g[H - 2][i] = '.';
    }
    for (int i = 5; i <= 7; ++i) {
        g[i][0] = g[i][1] = '.';
        g[i][W - 1] = g[i][W - 2] = '.';
    }
    for (int y = kCenterY - 1; y <= kCenterY + 1; ++y)
        for (int x = kCenterX - 1; x <= kCenterX + 1; ++x) g[y][x] = '.';
}

// Pockets cut off from the centre are filled in rather than left as
// unreachable floor.
void fillUnreachable(Grid& g) {
    std::array<std::array<bool, W>, H> seen{};
    std::queue<std::pair<int, int>> frontier;
    frontier.push({kCenterX, kCenterY});
    seen[kCenterY][kCenterX] = true;
    while (!frontier.empty()) {
        const auto [x, y] = frontier.front();
        frontier.pop();
        for (const auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= W || ny >= H || seen[ny][nx] || g[ny][nx] != '.') continue;
            seen[ny][nx] = true;
            frontier.push({nx, ny});
        }
    }
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (g[y][x] == '.' && !seen[y][x]) g[y][x] = '#';
}

// A 3-wide lane from each socket straight to the centre: whatever the style
// did, every entrance reaches the middle of the cell.
void carveLanes(Grid& g) {
    for (int y = 0; y <= kCenterY; ++y)
        for (int x = 9; x <= 11; ++x) g[y][x] = g[H - 1 - y][x] = '.';
    for (int x = 0; x <= kCenterX; ++x)
        for (int y = 5; y <= 7; ++y) g[y][x] = g[y][W - 1 - x] = '.';
}

void hall(Grid& g, std::mt19937& rng) {
    fillInterior(g, '.');
    const int stepX = roll(rng, 3, 4), stepY = roll(rng, 2, 3);
    const int offsetX = roll(rng, 2, 3), offsetY = roll(rng, 2, 3);
    for (int y = offsetY; y < H - 2; y += stepY)
        for (int x = offsetX; x < W - 2; x += stepX) {
            if (!chance(rng, 0.75f)) continue;
            g[y][x] = '#';
            if (chance(rng, 0.3f) && x + 1 < W - 1) g[y][x + 1] = '#';
        }
}

void rooms(Grid& g, std::mt19937& rng) {
    fillInterior(g, '.');
    // Wall lines with several gaps each, so no room is a dead end.
    const auto wallLine = [&](bool vertical, int at, int from, int to) {
        for (int i = from; i <= to; ++i) (vertical ? g[i][at] : g[at][i]) = '#';
        const int gaps = roll(rng, 2, 3);
        for (int n = 0; n < gaps; ++n) {
            const int start = roll(rng, from, std::max(from, to - 2)), width = roll(rng, 2, 3);
            for (int i = start; i < start + width && i <= to; ++i) (vertical ? g[i][at] : g[at][i]) = '.';
        }
    };
    const int verticals = roll(rng, 1, 2);
    for (int n = 0; n < verticals; ++n) wallLine(true, n == 0 ? roll(rng, 4, 7) : roll(rng, 13, 16), 1, H - 2);
    wallLine(false, chance(rng, 0.5f) ? roll(rng, 3, 4) : roll(rng, 8, 9), 1, W - 2);
}

void ruins(Grid& g, std::mt19937& rng) {
    fillInterior(g, '.');
    const int chunks = roll(rng, 9, 15);
    for (int n = 0; n < chunks; ++n) {
        const int w = roll(rng, 1, 3), h = roll(rng, 1, 2);
        const int x0 = roll(rng, 2, W - 2 - w), y0 = roll(rng, 2, H - 2 - h);
        for (int y = y0; y < y0 + h; ++y)
            for (int x = x0; x < x0 + w; ++x) g[y][x] = '#';
    }
}

void cavern(Grid& g, std::mt19937& rng) {
    for (int y = 1; y < H - 1; ++y)
        for (int x = 1; x < W - 1; ++x) g[y][x] = chance(rng, 0.42f) ? '#' : '.';
    for (int pass = 0; pass < 4; ++pass) {
        Grid next = g;
        for (int y = 1; y < H - 1; ++y)
            for (int x = 1; x < W - 1; ++x) {
                int walls = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if ((dx || dy) && g[y + dy][x + dx] == '#') ++walls;
                next[y][x] = walls >= 5 ? '#' : walls <= 3 ? '.' : g[y][x];
            }
        g = next;
    }
    carveLanes(g);
}

float floorShare(const Grid& g) {
    int floor = 0;
    for (const auto& row : g)
        for (const char c : row) floor += c == '.';
    return static_cast<float>(floor) / (W * H);
}

ModuleTemplate toTemplate(const Grid& g, ProceduralStyle style) {
    ModuleTemplate module{std::string("Procedural ") + proceduralStyleName(style), {}};
    for (const auto& row : g) module.rows.emplace_back(row.begin(), row.end());
    return module;
}

} // namespace

const char* proceduralStyleName(ProceduralStyle style) {
    switch (style) {
        case ProceduralStyle::Hall: return "Hall";
        case ProceduralStyle::Rooms: return "Rooms";
        case ProceduralStyle::Ruins: return "Ruins";
        case ProceduralStyle::Cavern: return "Cavern";
    }
    return "";
}

ProceduralStyle pickProceduralStyle(FloorRegion region, std::mt19937& rng) {
    // Weights for Hall, Rooms, Ruins, Cavern.
    std::array<int, 4> weights{2, 2, 2, 1};
    if (region == FloorRegion::Barracks) weights = {2, 4, 2, 1};
    if (region == FloorRegion::Sanctum) weights = {4, 2, 2, 1};
    if (region == FloorRegion::Crypts) weights = {1, 2, 2, 4};
    std::discrete_distribution<int> pick(weights.begin(), weights.end());
    return static_cast<ProceduralStyle>(pick(rng));
}

ModuleTemplate proceduralModule(ProceduralStyle style, std::mt19937& rng) {
    Grid best{};
    for (int attempt = 0; attempt < kAttempts; ++attempt) {
        Grid g;
        for (auto& row : g) row.fill('#');
        switch (style) {
            case ProceduralStyle::Hall: hall(g, rng); break;
            case ProceduralStyle::Rooms: rooms(g, rng); break;
            case ProceduralStyle::Ruins: ruins(g, rng); break;
            case ProceduralStyle::Cavern: cavern(g, rng); break;
        }
        openEssentials(g);
        fillUnreachable(g);
        // Unreachable sockets would break the floor: lanes guarantee them.
        if (g[0][10] != '.' || g[H - 1][10] != '.' || g[6][0] != '.' || g[6][W - 1] != '.') {
            carveLanes(g);
            fillUnreachable(g);
        }
        best = g;
        if (floorShare(g) >= kMinFloorShare && validateRegularModule(toTemplate(g, style)).empty()) break;
        // Too closed in: fall through to another attempt (lanes keep the
        // last attempt valid even if none reach the target openness).
        carveLanes(best);
        fillUnreachable(best);
    }
    return toTemplate(best, style);
}

} // namespace engine
