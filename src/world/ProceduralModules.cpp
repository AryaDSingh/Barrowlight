// Procedural rooms for the floor generator's grid: halls, rooms, ruins and
// caverns carved at random, always keeping the doorways that join
// neighbouring rooms and the centre open, and rejecting rooms with too
// little floor.

#include "world/ProceduralModules.hpp"

#include <algorithm>
#include <array>
#include <queue>
#include <string>

namespace engine {

namespace {

constexpr float kMinFloorShare = 0.5f;
constexpr int kAttempts = 40;

using Grid = std::vector<std::string>;

int roll(std::mt19937& rng, int low, int high) { return std::uniform_int_distribution<int>(low, high)(rng); }
bool chance(std::mt19937& rng, float p) { return std::uniform_real_distribution<float>(0.f, 1.f)(rng) < p; }

int width(const Grid& g) { return static_cast<int>(g.front().size()); }
int height(const Grid& g) { return static_cast<int>(g.size()); }
int centerX(const Grid& g) { return width(g) / 2; }
int centerY(const Grid& g) { return height(g) / 2; }

void fillInterior(Grid& g, char c) {
    for (int y = 1; y < height(g) - 1; ++y)
        for (int x = 1; x < width(g) - 1; ++x) g[y][x] = c;
}

// The three tiles of a doorway, `depth` deep from the border inwards.
template <typename F>
void forDoorway(const Grid& g, const Doorway& d, int depth, F&& f) {
    for (int i = 0; i < 3; ++i)
        for (int k = 0; k < depth; ++k) {
            switch (d.side) {
                case Side::Top: f(d.at + i, k); break;
                case Side::Bottom: f(d.at + i, height(g) - 1 - k); break;
                case Side::Left: f(k, d.at + i); break;
                case Side::Right: f(width(g) - 1 - k, d.at + i); break;
            }
        }
}

// Doorways, the tiles just inside them, and the centre are always open: the
// doorways join neighbouring rooms and the centre hosts encounters.
void openEssentials(Grid& g, const std::vector<Doorway>& doorways) {
    for (const Doorway& d : doorways) forDoorway(g, d, 2, [&](int x, int y) { g[y][x] = '.'; });
    for (int y = centerY(g) - 1; y <= centerY(g) + 1; ++y)
        for (int x = centerX(g) - 1; x <= centerX(g) + 1; ++x) g[y][x] = '.';
}

// Pockets cut off from the centre are filled in rather than left as
// unreachable floor.
void fillUnreachable(Grid& g) {
    std::vector<std::vector<bool>> seen(static_cast<std::size_t>(height(g)), std::vector<bool>(static_cast<std::size_t>(width(g)), false));
    std::queue<std::pair<int, int>> frontier;
    frontier.push({centerX(g), centerY(g)});
    seen[centerY(g)][centerX(g)] = true;
    while (!frontier.empty()) {
        const auto [x, y] = frontier.front();
        frontier.pop();
        for (const auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || ny < 0 || nx >= width(g) || ny >= height(g) || seen[ny][nx] || g[ny][nx] != '.') continue;
            seen[ny][nx] = true;
            frontier.push({nx, ny});
        }
    }
    for (int y = 0; y < height(g); ++y)
        for (int x = 0; x < width(g); ++x)
            if (g[y][x] == '.' && !seen[y][x]) g[y][x] = '#';
}

// A 3-wide lane from each doorway in to the centre's row or column, then
// along it to the centre: whatever the style did, every entrance reaches the
// middle of the room.
void carveLanes(Grid& g, const std::vector<Doorway>& doorways) {
    const int cx = centerX(g), cy = centerY(g);
    const auto open = [&](int x0, int y0, int x1, int y1) {
        for (int y = std::min(y0, y1); y <= std::max(y0, y1); ++y)
            for (int x = std::min(x0, x1); x <= std::max(x0, x1); ++x) g[y][x] = '.';
    };
    for (const Doorway& d : doorways) {
        const int a = d.at, b = d.at + 2;
        switch (d.side) {
            case Side::Top: open(a, 0, b, cy + 1); open(a, cy - 1, cx, cy + 1); break;
            case Side::Bottom: open(a, height(g) - 1, b, cy - 1); open(a, cy - 1, cx, cy + 1); break;
            case Side::Left: open(0, a, cx + 1, b); open(cx - 1, a, cx + 1, cy); break;
            case Side::Right: open(width(g) - 1, a, cx - 1, b); open(cx - 1, a, cx + 1, cy); break;
        }
    }
}

void hall(Grid& g, std::mt19937& rng) {
    fillInterior(g, '.');
    const int stepX = roll(rng, 3, 4), stepY = roll(rng, 2, 3);
    const int offsetX = roll(rng, 2, 3), offsetY = roll(rng, 2, 3);
    for (int y = offsetY; y < height(g) - 2; y += stepY)
        for (int x = offsetX; x < width(g) - 2; x += stepX) {
            if (!chance(rng, 0.75f)) continue;
            g[y][x] = '#';
            if (chance(rng, 0.3f) && x + 1 < width(g) - 1) g[y][x + 1] = '#';
        }
}

void rooms(Grid& g, std::mt19937& rng) {
    fillInterior(g, '.');
    const int w = width(g), h = height(g);
    // Wall lines with several gaps each, so no room is a dead end.
    const auto wallLine = [&](bool vertical, int at, int from, int to) {
        for (int i = from; i <= to; ++i) (vertical ? g[i][at] : g[at][i]) = '#';
        const int gaps = roll(rng, 2, 3) + (to - from) / 20;
        for (int n = 0; n < gaps; ++n) {
            const int start = roll(rng, from, std::max(from, to - 2)), size = roll(rng, 2, 3);
            for (int i = start; i < start + size && i <= to; ++i) (vertical ? g[i][at] : g[at][i]) = '.';
        }
    };
    // One or two dividing walls across each cell's width, one down each cell's height.
    for (int left = 0; left < w; left += kModuleWidth) {
        const int verticals = roll(rng, 1, 2);
        for (int n = 0; n < verticals; ++n)
            wallLine(true, left + (n == 0 ? roll(rng, 4, 7) : roll(rng, 13, 16)), 1, h - 2);
    }
    for (int top = 0; top < h; top += kModuleHeight)
        wallLine(false, top + (chance(rng, 0.5f) ? roll(rng, 3, 4) : roll(rng, 8, 9)), 1, w - 2);
}

void ruins(Grid& g, std::mt19937& rng) {
    fillInterior(g, '.');
    const int cells = (width(g) / kModuleWidth) * (height(g) / kModuleHeight);
    const int chunks = roll(rng, 9, 15) * cells;
    for (int n = 0; n < chunks; ++n) {
        const int w = roll(rng, 1, 3), h = roll(rng, 1, 2);
        const int x0 = roll(rng, 2, width(g) - 2 - w), y0 = roll(rng, 2, height(g) - 2 - h);
        for (int y = y0; y < y0 + h; ++y)
            for (int x = x0; x < x0 + w; ++x) g[y][x] = '#';
    }
}

void cavern(Grid& g, std::mt19937& rng, const std::vector<Doorway>& doorways) {
    for (int y = 1; y < height(g) - 1; ++y)
        for (int x = 1; x < width(g) - 1; ++x) g[y][x] = chance(rng, 0.42f) ? '#' : '.';
    for (int pass = 0; pass < 4; ++pass) {
        Grid next = g;
        for (int y = 1; y < height(g) - 1; ++y)
            for (int x = 1; x < width(g) - 1; ++x) {
                int walls = 0;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx)
                        if ((dx || dy) && g[y + dy][x + dx] == '#') ++walls;
                next[y][x] = walls >= 5 ? '#' : walls <= 3 ? '.' : g[y][x];
            }
        g = next;
    }
    carveLanes(g, doorways);
}

float floorShare(const Grid& g) {
    int floor = 0;
    for (const auto& row : g)
        for (const char c : row) floor += c == '.';
    return static_cast<float>(floor) / static_cast<float>(width(g) * height(g));
}

bool doorwaysOpen(const Grid& g, const std::vector<Doorway>& doorways) {
    bool open = true;
    for (const Doorway& d : doorways) forDoorway(g, d, 1, [&](int x, int y) { open = open && g[y][x] == '.'; });
    return open;
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

ModuleTemplate proceduralModule(ProceduralStyle style, int cellsWide, int cellsHigh,
                                const std::vector<Doorway>& doorways, std::mt19937& rng) {
    const int w = cellsWide * kModuleWidth, h = cellsHigh * kModuleHeight;
    const std::string name = std::string("Procedural ") + proceduralStyleName(style);
    Grid best;
    for (int attempt = 0; attempt < kAttempts; ++attempt) {
        Grid g(static_cast<std::size_t>(h), std::string(static_cast<std::size_t>(w), '#'));
        switch (style) {
            case ProceduralStyle::Hall: hall(g, rng); break;
            case ProceduralStyle::Rooms: rooms(g, rng); break;
            case ProceduralStyle::Ruins: ruins(g, rng); break;
            case ProceduralStyle::Cavern: cavern(g, rng, doorways); break;
        }
        // The border is wall but for the doorways.
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                if (x == 0 || y == 0 || x == w - 1 || y == h - 1) g[y][x] = '#';
        openEssentials(g, doorways);
        fillUnreachable(g);
        // Unreachable doorways would break the floor: lanes guarantee them.
        if (!doorwaysOpen(g, doorways)) {
            carveLanes(g, doorways);
            fillUnreachable(g);
        }
        best = g;
        if (floorShare(g) >= kMinFloorShare && validateRoom({name, g}, doorways).empty()) break;
        // Too closed in: fall through to another attempt (lanes keep the
        // last attempt valid even if none reach the target openness).
        carveLanes(best, doorways);
        fillUnreachable(best);
    }
    return {name, best};
}

ModuleTemplate proceduralModule(ProceduralStyle style, std::mt19937& rng) {
    return proceduralModule(style, 1, 1, centralSockets(1, 1), rng);
}

} // namespace engine
