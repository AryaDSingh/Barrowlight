// Renders generated floors to a self-contained HTML page, one SVG per floor,
// so the generator can be tuned by eye without starting the game. Links
// engine_core only (no SFML, no window), like the console tests.
//
//   dungeon_viewer [--floor N | --floors A-B] [--seed S] [--count C]
//                  [--blood] [--out FILE] [--ascii] [--svg DIR] [--back URL]
//
//   --floor N     generate floor N as the game would (default 1)
//   --floors A-B  one floor per number from A to B (a tour of the run)
//   --seed S      first seed (default 1); each further floor uses the next
//   --count C     floors per floor number (default 12 with --floor, 1 with --floors)
//   --blood       as if the Blood Altar's offering is made (no altar rolls)
//   --out FILE    where to write the page (default dungeons.html)
//   --ascii       print each floor to the console instead
//   --svg DIR     also write each floor as a standalone SVG image in DIR
//   --back URL    add a back link to the page (for publishing it on a site)
//
// The parameters for a floor number come from floorGenerationParams, the
// same function the game calls, so a floor shown here for a seed is the
// layout the game builds from that seed on that floor.

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "world/DungeonGenerator.hpp"
#include "world/DungeonModules.hpp"
#include "world/FloorTheme.hpp"
#include "world/Landmark.hpp"
#include "world/Props.hpp"

using namespace engine;

namespace {

constexpr int kTile = 10; // pixels per tile in the SVG

const char* kPageTop = R"(
:root { color-scheme: dark; --bg:#121015; --panel:#1c1920; --text:#e8e2d6; --muted:#9a9187; --line:#2e2a33; }
* { box-sizing: border-box; }
body { margin: 0; background: var(--bg); color: var(--text); font: 14px/1.45 system-ui, sans-serif; }
header { padding: 20px 16px 8px; max-width: 1400px; margin: 0 auto; }
h1 { margin: 0 0 4px; font: 600 22px Georgia, serif; letter-spacing: .02em; }
header p { margin: 0 0 10px; color: var(--muted); }
.controls { display: flex; flex-wrap: wrap; gap: 6px 16px; color: var(--muted); }
.legend { display: flex; flex-wrap: wrap; gap: 4px 14px; margin-top: 10px; color: var(--muted); font-size: 13px; }
.legend i { display: inline-block; width: 12px; height: 12px; border-radius: 3px; vertical-align: -1px; margin-right: 5px; }
main { display: grid; grid-template-columns: repeat(auto-fill, minmax(min(100%, 520px), 1fr)); gap: 16px;
       padding: 8px 16px 32px; max-width: 1400px; margin: 0 auto; }
.card { margin: 0; background: var(--panel); border: 1px solid var(--line); border-top: 3px solid var(--door);
        border-radius: 8px; padding: 10px; }
figcaption { margin-bottom: 8px; }
.seed { color: var(--muted); float: right; font-variant-numeric: tabular-nums; }
svg { display: block; width: 100%; height: auto; border-radius: 4px; }
dl { display: grid; grid-template-columns: max-content 1fr; gap: 2px 12px; margin: 8px 0 0; font-size: 13px; }
dt { color: var(--muted); } dd { margin: 0; }

)";

// The maps' own style: inlined into the page, and into each standalone SVG.
const char* kSvgStyle = R"(
.grid line { stroke: #fff; stroke-opacity: .18; stroke-dasharray: 4 4; }
.names text { fill: #fff; fill-opacity: .7; font: 600 10px system-ui, sans-serif; paint-order: stroke; stroke: #000; stroke-width: 2.5px; stroke-opacity: .6; }
.names text.proc { font-style: italic; fill: #c9e4ff; }
.decals circle { fill: #d8cbb0; fill-opacity: .55; }
.props rect { fill: #b98a52; stroke: #3a2a18; stroke-width: 1; }
.anchors circle { fill: none; stroke: #ff6b5a; stroke-width: 1.6; }
.anchors .vig { fill: #ffd2c8; font: 9px system-ui, sans-serif; text-anchor: middle; paint-order: stroke; stroke: #000; stroke-width: 2px; stroke-opacity: .6; }
.mark rect { stroke: #000; stroke-opacity: .5; }
.mark text { font: 700 11px system-ui, sans-serif; text-anchor: middle; fill: #111; }
.start rect { fill: #7ee081; } .exit rect { fill: var(--door); } .boss rect { fill: #ff5a5a; }
.vault rect { fill: #ffd34d; } .event rect { fill: #c9a6ff; }
.gate { fill: #ffd34d; fill-opacity: .6; }
)";

const char* kPageStyle = R"(
a.back { color: #c9a6ff; font-size: 13px; }
body.no-names .names, body.no-props .props, body.no-props .decals, body.no-anchors .anchors, body.no-grid .grid { display: none; }
)";

struct Options {
    int firstFloor = 1, lastFloor = 1;
    unsigned seed = 1;
    int count = -1;
    bool blood = false, ascii = false;
    std::string out = "dungeons.html", svgDir, back;
};

struct Floor {
    int number;
    unsigned seed;
    DungeonGenerationParams params;
    GeneratedDungeon dungeon;
};

[[noreturn]] void usage(const std::string& error) {
    std::cerr << "dungeon_viewer: " << error << "\n"
              << "usage: dungeon_viewer [--floor N | --floors A-B] [--seed S] [--count C] [--blood] [--out FILE] [--ascii]\n"
                 "                      [--svg DIR] [--back URL]\n";
    std::exit(2);
}

int parseInt(const std::string& text, const std::string& flag) {
    try {
        std::size_t used = 0;
        const int value = std::stoi(text, &used);
        if (used == text.size()) return value;
    } catch (const std::exception&) {
    }
    usage("bad number for " + flag + ": " + text);
}

Options parseOptions(int argc, char** argv) {
    Options o;
    bool range = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto value = [&]() -> std::string {
            if (i + 1 >= argc) usage(arg + " needs a value");
            return argv[++i];
        };
        if (arg == "--floor") o.firstFloor = o.lastFloor = parseInt(value(), arg);
        else if (arg == "--floors") {
            const std::string v = value();
            const auto dash = v.find('-');
            if (dash == std::string::npos) usage("--floors takes A-B");
            o.firstFloor = parseInt(v.substr(0, dash), arg);
            o.lastFloor = parseInt(v.substr(dash + 1), arg);
            range = true;
        } else if (arg == "--seed") o.seed = static_cast<unsigned>(parseInt(value(), arg));
        else if (arg == "--count") o.count = parseInt(value(), arg);
        else if (arg == "--out") o.out = value();
        else if (arg == "--blood") o.blood = true;
        else if (arg == "--ascii") o.ascii = true;
        else if (arg == "--svg") o.svgDir = value();
        else if (arg == "--back") o.back = value();
        else usage("unknown option " + arg);
    }
    if (o.firstFloor < 1 || o.lastFloor > kMaxFloorId || o.firstFloor > o.lastFloor)
        usage("floors run from 1 to " + std::to_string(kMaxFloorId));
    if (o.count < 0) o.count = range ? 1 : 12;
    if (o.count < 1) usage("--count must be at least 1");
    return o;
}

std::string escape(const std::string& text) {
    std::string out;
    for (const char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            default: out += c;
        }
    }
    return out;
}

std::string hex(ThemeColor c) {
    char buffer[8];
    std::snprintf(buffer, sizeof buffer, "#%02x%02x%02x", c.r, c.g, c.b);
    return buffer;
}

const char* vignetteName(Vignette v) {
    switch (v) {
        case Vignette::Mess: return "Mess";
        case Vignette::Armoury: return "Armoury";
        case Vignette::Dormitory: return "Dormitory";
        case Vignette::Nest: return "Nest";
        case Vignette::Shrine: return "Shrine";
        case Vignette::Library: return "Library";
        case Vignette::Ossuary: return "Ossuary";
        case Vignette::GraveDig: return "Grave-dig";
        case Vignette::None: break;
    }
    return "";
}

const char* decalName(DecalKind kind) {
    switch (kind) {
        case DecalKind::Bones: return "Bones";
        case DecalKind::Skull: return "Skull";
        case DecalKind::Web: return "Web";
        case DecalKind::Dirt: return "Dirt";
        case DecalKind::Coins: return "Coins";
        case DecalKind::Weapon: return "Weapon";
        case DecalKind::Book: return "Book";
        case DecalKind::Debris: return "Debris";
        case DecalKind::Candle: return "Candle";
        case DecalKind::Pick: return "Pick";
    }
    return "";
}

// Props are see-through walls; everything else that isn't walkable is wall.
bool isProp(const GeneratedDungeon& d, int x, int y) {
    const Tile& t = d.map.tileAt(x, y);
    return t.type == TileType::Wall && t.transparent;
}

// The exit: on a non-boss floor the last encounter anchor sits in the
// farthest module and hosts the door down (see GeneratedDungeon).
bool hasExit(const GeneratedDungeon& d) { return !d.hasBossRoom && !d.otherRoomCenters.empty(); }

void printAscii(const Floor& f) {
    const GeneratedDungeon& d = f.dungeon;
    std::cout << "Floor " << f.number << " (" << floorTheme(f.number).name << "), seed " << f.seed << "\n";
    for (int y = 0; y < d.map.height(); ++y) {
        for (int x = 0; x < d.map.width(); ++x) {
            const auto is = [&](Position p) { return p.x == x && p.y == y; };
            char c = d.map.isWalkable(x, y) ? '.' : isProp(d, x, y) ? 'o' : '#';
            for (const Position& a : d.otherRoomCenters) if (is(a)) c = 'g';
            if (hasExit(d) && is(d.otherRoomCenters.back())) c = '>';
            if (d.hasBossRoom && is(d.bossRoomCenter)) c = 'B';
            if (d.hasVault && is(d.vaultCenter)) c = 'V';
            if (d.landmark != LandmarkKind::None && is(d.landmarkAltar)) c = 'S';
            for (const auto& extra : d.extraLandmarks) if (is(extra.second)) c = 'S';
            if (is(d.playerStart)) c = '@';
            std::cout << c;
        }
        std::cout << '\n';
    }
    std::cout << '\n';
}

// How many of the twelve cell-to-cell joins are open: a socket is open when
// its middle tile is floor on the shared edge.
int openJoins(const GeneratedDungeon& d) {
    int joins = 0;
    for (int r = 0; r < kModuleGrid; ++r)
        for (int c = 0; c < kModuleGrid; ++c) {
            const int x0 = c * kModuleWidth, y0 = r * kModuleHeight;
            if (c + 1 < kModuleGrid) joins += d.map.isWalkable(x0 + kModuleWidth - 1, y0 + 6);
            if (r + 1 < kModuleGrid) joins += d.map.isWalkable(x0 + 10, y0 + kModuleHeight - 1);
        }
    return joins;
}

// A standalone SVG carries its own style; one inside the page uses the page's.
void writeSvg(std::ostream& out, const Floor& f, bool standalone) {
    const GeneratedDungeon& d = f.dungeon;
    const FloorTheme theme = floorTheme(f.number);
    const int w = d.map.width(), h = d.map.height();
    out << "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 " << w * kTile << ' ' << h * kTile
        << "\" style=\"--door:" << hex(theme.door) << "\" role=\"img\" aria-label=\"Barrowlight floor " << f.number
        << ", " << escape(theme.name) << ", seed " << f.seed << "\">\n";
    if (standalone) out << "<style>" << kSvgStyle << "</style>\n";
    out << "<rect width=\"100%\" height=\"100%\" fill=\"" << hex(theme.floor) << "\"/>\n";

    // Walls, merged into horizontal runs to keep the file small.
    out << "<g fill=\"" << hex(theme.wall) << "\">";
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            if (d.map.isWalkable(x, y) || isProp(d, x, y)) continue;
            int end = x;
            while (end + 1 < w && !d.map.isWalkable(end + 1, y) && !isProp(d, end + 1, y)) ++end;
            out << "<rect x=\"" << x * kTile << "\" y=\"" << y * kTile << "\" width=\"" << (end - x + 1) * kTile
                << "\" height=\"" << kTile << "\"/>";
            x = end;
        }
    out << "</g>\n";

    // Module grid and names.
    out << "<g class=\"grid\">";
    for (int i = 1; i < kModuleGrid; ++i) {
        out << "<line x1=\"" << i * kModuleWidth * kTile << "\" y1=\"0\" x2=\"" << i * kModuleWidth * kTile << "\" y2=\""
            << h * kTile << "\"/>";
        out << "<line x1=\"0\" y1=\"" << i * kModuleHeight * kTile << "\" x2=\"" << w * kTile << "\" y2=\""
            << i * kModuleHeight * kTile << "\"/>";
    }
    out << "</g>\n<g class=\"names\">";
    for (int cell = 0; cell < kModuleGrid * kModuleGrid; ++cell) {
        const std::string& name = d.moduleNames[static_cast<std::size_t>(cell)];
        const bool procedural = name.rfind("Procedural ", 0) == 0;
        out << "<text x=\"" << (cell % kModuleGrid) * kModuleWidth * kTile + 4 << "\" y=\""
            << (cell / kModuleGrid) * kModuleHeight * kTile + 12 << "\"" << (procedural ? " class=\"proc\"" : "") << '>'
            << escape(procedural ? name.substr(11) : name) << "</text>";
    }
    out << "</g>\n";

    // Decals, then props.
    out << "<g class=\"decals\">";
    for (const Decal& decal : d.decals)
        out << "<circle cx=\"" << decal.pos.x * kTile + kTile / 2 << "\" cy=\"" << decal.pos.y * kTile + kTile / 2
            << "\" r=\"1.6\"><title>" << decalName(decal.kind) << "</title></circle>";
    out << "</g>\n<g class=\"props\">";
    for (const Prop& prop : d.props)
        out << "<rect x=\"" << prop.pos.x * kTile + 1 << "\" y=\"" << prop.pos.y * kTile + 1 << "\" width=\""
            << propWidth(prop.kind) * kTile - 2 << "\" height=\"" << kTile - 2 << "\"><title>" << escape(propName(prop.kind))
            << "</title></rect>";
    out << "</g>\n";

    // Encounter anchors, with the vignette that dresses each room.
    out << "<g class=\"anchors\">";
    for (std::size_t i = 0; i < d.otherRoomCenters.size(); ++i) {
        const Position a = d.otherRoomCenters[i];
        const bool exit = hasExit(d) && i + 1 == d.otherRoomCenters.size();
        const Vignette v = i < d.roomVignettes.size() ? d.roomVignettes[i] : Vignette::None;
        std::string title = exit ? "Encounter and the door down" : "Encounter";
        if (v != Vignette::None) title += std::string(" (") + vignetteName(v) + ")";
        out << "<circle cx=\"" << a.x * kTile + kTile / 2 << "\" cy=\"" << a.y * kTile + kTile / 2 << "\" r=\"4\"><title>"
            << escape(title) << "</title></circle>";
        if (v != Vignette::None)
            out << "<text class=\"vig\" x=\"" << a.x * kTile + kTile / 2 << "\" y=\"" << a.y * kTile + kTile + 10 << "\">"
                << vignetteName(v) << "</text>";
    }
    out << "</g>\n";

    // Markers: a filled square with a letter, centred on its tile.
    const auto marker = [&](Position p, const char* cls, const char* letter, const std::string& title) {
        const int cx = p.x * kTile + kTile / 2, cy = p.y * kTile + kTile / 2;
        out << "<g class=\"mark " << cls << "\"><title>" << escape(title) << "</title><rect x=\"" << cx - 8 << "\" y=\""
            << cy - 8 << "\" width=\"16\" height=\"16\" rx=\"3\"/><text x=\"" << cx << "\" y=\"" << cy + 4 << "\">"
            << letter << "</text></g>\n";
    };
    if (hasExit(d)) marker(d.otherRoomCenters.back(), "exit", "&gt;", "The door down");
    if (d.hasBossRoom) marker(d.bossRoomCenter, "boss", "B", "Boss arena");
    if (d.hasVault) {
        marker(d.vaultCenter, "vault", "V", "Sealed vault");
        out << "<rect class=\"gate\" x=\"" << d.vaultEntrance.x * kTile << "\" y=\"" << d.vaultEntrance.y * kTile
            << "\" width=\"" << kTile << "\" height=\"" << kTile << "\"><title>Vault gate</title></rect>\n";
    }
    if (d.landmark != LandmarkKind::None)
        marker(d.landmarkAltar, "event", "!", landmarkName(d.landmark, f.params.region));
    for (const auto& [kind, altar] : d.extraLandmarks) marker(altar, "event", "!", landmarkName(kind, f.params.region));
    marker(d.playerStart, "start", "@", "Start");
    out << "</svg>\n";
}

void writeCard(std::ostream& out, const Floor& f) {
    const GeneratedDungeon& d = f.dungeon;
    const FloorTheme theme = floorTheme(f.number);
    int floorTiles = 0;
    for (int y = 0; y < d.map.height(); ++y)
        for (int x = 0; x < d.map.width(); ++x) floorTiles += d.map.isWalkable(x, y);
    int procedural = 0;
    for (const auto& name : d.moduleNames) procedural += name.rfind("Procedural ", 0) == 0;
    std::vector<std::string> events;
    if (d.landmark != LandmarkKind::None) events.push_back(landmarkName(d.landmark, f.params.region));
    for (const auto& extra : d.extraLandmarks) events.push_back(landmarkName(extra.first, f.params.region));
    int dressed = 0;
    for (const Vignette v : d.roomVignettes) dressed += v != Vignette::None;

    out << "<figure class=\"card\" style=\"--door:" << hex(theme.door) << "\">\n<figcaption><b>Floor " << f.number
        << "</b> " << escape(theme.name) << " <span class=\"seed\">seed " << f.seed << "</span></figcaption>\n";
    writeSvg(out, f, false);
    out << "<dl>"
        << "<dt>Open</dt><dd>" << (100 * floorTiles) / (d.map.width() * d.map.height()) << "% floor</dd>"
        << "<dt>Joins</dt><dd>" << openJoins(d) << " of 12</dd>"
        << "<dt>Cells</dt><dd>" << procedural << " procedural</dd>"
        << "<dt>Rooms</dt><dd>" << d.otherRoomCenters.size() << " encounters, " << dressed << " dressed</dd>"
        << "<dt>Props</dt><dd>" << d.props.size() << ", " << d.decals.size() << " decals</dd>";
    if (!events.empty()) {
        out << "<dt>Events</dt><dd>";
        for (std::size_t i = 0; i < events.size(); ++i) out << (i ? ", " : "") << escape(events[i]);
        out << "</dd>";
    }
    out << "</dl>\n</figure>\n";
}

const char* kScript = R"(
for (const box of document.querySelectorAll('.controls input'))
  box.addEventListener('change', () => document.body.classList.toggle('no-' + box.name, !box.checked));
)";

void writePage(const std::string& path, const std::vector<Floor>& floors, const Options& o) {
    std::ofstream out(path);
    if (!out) usage("can't write " + path);
    std::ostringstream what;
    if (o.firstFloor == o.lastFloor) what << "Floor " << o.firstFloor;
    else what << "Floors " << o.firstFloor << "&ndash;" << o.lastFloor;
    what << ", " << floors.size() << (floors.size() == 1 ? " layout" : " layouts") << " from seed " << o.seed;
    out << "<!doctype html>\n<html lang=\"en\"><head><meta charset=\"utf-8\">"
        << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
        << "<title>Barrowlight Floors</title><style>" << kPageTop << kSvgStyle << kPageStyle
        << "</style></head>\n<body>\n<header>\n";
    if (!o.back.empty()) out << "<a class=\"back\" href=\"" << escape(o.back) << "\">&larr; Back</a>\n";
    out << "<h1>Barrowlight floors</h1>\n<p>" << what.str()
        << ". Each floor is a 3&times;3 grid of modules; italic names are procedural cells. Hover anything for details.</p>\n"
        << "<div class=\"controls\">"
        << "<label><input type=\"checkbox\" name=\"names\" checked> Module names</label>"
        << "<label><input type=\"checkbox\" name=\"grid\" checked> Grid</label>"
        << "<label><input type=\"checkbox\" name=\"anchors\" checked> Encounters</label>"
        << "<label><input type=\"checkbox\" name=\"props\" checked> Props and decals</label></div>\n"
        << "<div class=\"legend\"><span><i style=\"background:#7ee081\"></i>Start</span>"
        << "<span><i style=\"background:#c84a8a\"></i>Door down (in the floor&rsquo;s door colour)</span>"
        << "<span><i style=\"background:#ff5a5a\"></i>Boss</span><span><i style=\"background:#ffd34d\"></i>Vault</span>"
        << "<span><i style=\"background:#c9a6ff\"></i>Event</span>"
        << "<span><i style=\"border:2px solid #ff6b5a\"></i>Encounter</span>"
        << "<span><i style=\"background:#b98a52\"></i>Prop</span></div>\n</header>\n<main>\n";
    for (const Floor& f : floors) writeCard(out, f);
    out << "</main>\n<script>" << kScript << "</script>\n</body></html>\n";
}

} // namespace

int main(int argc, char** argv) {
    const Options o = parseOptions(argc, argv);
    std::vector<Floor> floors;
    unsigned seed = o.seed;
    for (int number = o.firstFloor; number <= o.lastFloor; ++number)
        for (int i = 0; i < o.count; ++i, ++seed) {
            const DungeonGenerationParams params = floorGenerationParams(number, o.blood);
            floors.push_back({number, seed, params, generateDungeon(params, seed)});
        }
    if (o.ascii) {
        for (const Floor& f : floors) printAscii(f);
        return 0;
    }
    if (!o.svgDir.empty()) {
        std::filesystem::create_directories(o.svgDir);
        for (const Floor& f : floors) {
            const std::string path = o.svgDir + "/floor-" + std::to_string(f.number) + "-seed-" + std::to_string(f.seed) + ".svg";
            std::ofstream svg(path);
            if (!svg) usage("can't write " + path);
            writeSvg(svg, f, true);
        }
        std::cout << "Wrote " << floors.size() << " SVGs to " << o.svgDir << '\n';
    }
    writePage(o.out, floors, o);
    std::cout << "Wrote " << floors.size() << " floors to " << o.out << '\n';
    return 0;
}
