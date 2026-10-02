#include "core/Application.hpp"

#include <algorithm>
#include <cmath>

#include "core/ScreenLayout.hpp"
#include "entities/Ascendancy.hpp"

namespace engine {

// The town square at night, drawn from the same art as the dungeon
// (assets/sprites/CREDITS.txt): the Evil Dungeon walls and gate, the
// Calciumtrice furniture and townsfolk, Siegmund's cobbles. Buildings line
// the back wall; each is a click target. Purely cosmetic: no state here
// beyond what the click handlers in ApplicationTravel.cpp already use.
namespace {
using namespace screen;
constexpr float kCell = kTownCell;
constexpr const char* kEvil = "evildungeon/evildungeon_0.png";
constexpr const char* kTiles = "calciumtrice/tiles/dungeon_tileset_calciumtrice.png";
constexpr const char* kCobbles = "siegmund/cobbles2.png";
constexpr const char* kWaterFountain = "evildungeon/water-fountain.png";
constexpr float kArt = 1.25f;  // 32px wall art fills a 40px cell
constexpr float kProp = 2.f;   // furniture and people stand larger, to read at a glance

unsigned cellHash(int x, int y) {
    unsigned h = static_cast<unsigned>(x) * 73856093u ^ static_cast<unsigned>(y) * 19349663u;
    h ^= h >> 13;
    return h * 1274126177u;
}
sf::Vector2f cell(float c, float r) { return {kTownScene.position.x + c * kCell, kTownScene.position.y + r * kCell}; }
// The point a sprite stands on: the bottom centre of cell (c, r).
sf::Vector2f foot(float c, float r) { return cell(c + 0.5f, r + 1.f); }

struct Station {
    const sf::FloatRect* spot;
    const char* name;
    const char* about;
    const char* action;
};
}

void Application::renderTownSquare() {
    const float now = animationClock_.getElapsedTime().asSeconds();
    const auto mouse = mousePixel_ ? std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)) : std::nullopt;
    const bool interactive = !merchantOpen_ && !inventoryOpen_;
    const auto hovering = [&](const sf::FloatRect& r) { return interactive && mouse && r.contains(*mouse); };

    // Draws a frame standing on `at`, scaled from its native pixels.
    const auto put = [&](const SpriteFrame& frame, sf::Vector2f at, float scale, sf::Color tint = sf::Color::White,
                         bool flip = false) {
        const float size = scale * static_cast<float>(std::max(frame.rect.size.x, frame.rect.size.y));
        sprites_.draw(window_, frame, {at.x - size / 2, at.y - size}, size, tint, flip);
    };
    const auto person = [&](const char* sheet, float c, float r, bool flip = false, float phase = 0.f) {
        const int frame = static_cast<int>((now + phase) * 6.f) % 10;
        const auto at = foot(c, r);
        sf::CircleShape shadow(15.f);
        shadow.setScale({1.f, 0.35f});
        shadow.setOrigin({15.f, 15.f});
        shadow.setPosition({at.x, at.y - 3});
        shadow.setFillColor(sf::Color(0, 0, 0, 90));
        window_.draw(shadow);
        put({sheet, sf::IntRect({frame * 32, 0}, {32, 32})}, {at.x, at.y + 6}, kProp, sf::Color::White, flip);
    };
    const auto tileArt = [&](int x, int y, int w, int h) { return SpriteFrame{kTiles, sf::IntRect({x, y}, {w, h})}; };
    const auto evilArt = [&](int x, int y, int w, int h) { return SpriteFrame{kEvil, sf::IntRect({x, y}, {w, h})}; };

    // --- Ground and back wall, batched ------------------------------------------
    const int cols = static_cast<int>(kTownScene.size.x / kCell), rows = static_cast<int>(kTownScene.size.y / kCell);
    sf::VertexArray cobbles(sf::PrimitiveType::Triangles), stone(sf::PrimitiveType::Triangles);
    for (int r = 3; r < rows; ++r)
        for (int c = 0; c < cols; ++c) {
            const unsigned h = cellHash(c, r);
            const auto shade = static_cast<std::uint8_t>(200 + h % 40);
            for (int q = 0; q < 4; ++q) {
                const unsigned v = cellHash(c * 2 + q % 2, r * 2 + q / 2);
                const SpriteFrame frame{kCobbles, sf::IntRect({static_cast<int>(v % 7) * 16, 160 + static_cast<int>((v >> 8) % 4) * 16}, {16, 16})};
                SpriteAtlas::append(cobbles, frame, cell(c + (q % 2) * 0.5f, r + (q / 2) * 0.5f), kCell / 2, sf::Color(shade, shade, shade));
            }
        }
    for (int c = 0; c < cols; ++c) {
        // A row of dark wall top, then a tall brick face.
        SpriteAtlas::append(stone, {kEvil, sf::IntRect({64, 0}, {32, 32})}, cell(c, 0), kCell, sf::Color(96, 88, 108));
        constexpr int kPlain[]{0, 32, 160, 192, 224, 0, 32, 192};
        const unsigned h = cellHash(c, 99);
        const int column = h % 9 == 0 ? (h % 2 ? 64 : 128) : kPlain[(h >> 4) % std::size(kPlain)];
        SpriteAtlas::append(stone, {kEvil, sf::IntRect({column, 128}, {32, 64})}, {cell(c, 1).x - kCell / 2, cell(c, 1).y}, kCell * 2);
    }
    if (const auto* tex = sprites_.texture(kCobbles)) { sf::RenderStates st; st.texture = tex; window_.draw(cobbles, st); }
    if (const auto* tex = sprites_.texture(kEvil)) { sf::RenderStates st; st.texture = tex; window_.draw(stone, st); }
    // A soft shadow where the wall meets the ground.
    {
        sf::VertexArray shade(sf::PrimitiveType::Triangles);
        const sf::Vector2f a = cell(0, 3), b = cell(static_cast<float>(cols), 3.6f);
        const sf::Color dark(0, 0, 0, 110), none(0, 0, 0, 0);
        for (const auto& v : {sf::Vertex{a, dark}, sf::Vertex{{b.x, a.y}, dark}, sf::Vertex{b, none},
                              sf::Vertex{a, dark}, sf::Vertex{b, none}, sf::Vertex{{a.x, b.y}, none}}) shade.append(v);
        window_.draw(shade);
    }

    std::vector<std::tuple<sf::Vector2f, sf::Color, float>> lights; // centre, colour, radius in cells
    const auto torch = [&](float c) {
        const int frame = (static_cast<int>(now * 6.f) + static_cast<int>(c) * 7) % 3;
        const auto at = cell(c + 0.5f, 2.55f);
        put(tileArt(176 + frame * 16, 304, 16, 16), at, kCell / 16.f);
        lights.push_back({{at.x, at.y - kCell * 0.5f}, sf::Color(255, 160, 80), 3.2f});
    };
    const auto banner = [&](float c, int base) { put(tileArt(base + (static_cast<int>(c) % 2) * 16, 176, 16, 32), cell(c + 0.5f, 2.6f), 2.4f); };

    // --- The inn: open-air tables before its door -----------------------------
    banner(2, 288); banner(7, 288);
    torch(1); torch(8);
    put(tileArt(64, 304, 16, 16), foot(1, 3), kProp);
    put(tileArt(80, 304, 16, 16), foot(1.7f, 3), kProp);
    put(tileArt(0, 304, 16, 16), foot(7.6f, 3), kProp);
    put(tileArt(64, 304, 16, 16), foot(8.3f, 3), kProp);
    put(tileArt(112, 304, 16, 16), foot(1.2f, 3.8f), kProp);
    person("calciumtrice/heroes/Peasant.png", 4.5f, 3.4f, false, 0.3f);
    for (const float c : {2.5f, 6.5f}) {
        put(tileArt(208, 240, 16, 32), foot(c - 0.9f, 5.6f), kProp);
        put(tileArt(288, 240, 32, 32), foot(c, 5.9f), kProp);
        put(tileArt(224, 240, 16, 32), foot(c + 0.9f, 5.6f), kProp);
    }
    person("calciumtrice/heroes/Simpleton.png", 1.1f, 5.7f, false, 1.7f);
    person("calciumtrice/heroes/StreetThief.png", 7.9f, 5.7f, true, 0.9f);
    lights.push_back({cell(4.5f, 5.f), sf::Color(255, 180, 110), 5.5f});

    // --- The fountain: a gargoyle spouting into a grated basin -----------------
    {
        const int frame = static_cast<int>(now * 6.f) % 4;
        put({"evildungeon/drain-water.png", sf::IntRect({frame * 96, 32}, {96, 96})}, foot(15, 5.4f), kArt);
        put({kWaterFountain, sf::IntRect({frame * 32, 2 * 96}, {32, 96})}, foot(15, 3.3f), 3.3f * kCell / 96.f);
        lights.push_back({cell(15.5f, 3.6f), sf::Color(120, 180, 255), 3.6f});
    }
    torch(13); torch(17);
    person("calciumtrice/heroes/Priest.png", 12.8f, 4.6f, false, 2.3f);

    // --- The stash ---------------------------------------------------------------
    put(tileArt(48, 304, 16, 16), foot(11, 9), kProp * 1.3f);
    put(tileArt(64, 304, 16, 16), foot(10.2f, 8.8f), kProp);
    lights.push_back({cell(11.5f, 9.5f), sf::Color(255, 200, 140), 2.2f});

    // --- The merchant: a counter of goods --------------------------------------
    banner(19, 320); banner(23, 320);
    torch(18); torch(24);
    person("calciumtrice/heroes/RoyalMessenger.png", 21.5f, 4.3f, false, 0.6f);
    for (const float c : {20.f, 21.5f, 23.f}) put(tileArt(288, 240, 32, 32), foot(c, 5.6f), kProp);
    put(tileArt(0, 304, 16, 16), foot(20, 5.f), kProp * 0.8f);
    put(tileArt(96, 304, 16, 16), foot(21.5f, 5.f), kProp * 0.8f);
    put(tileArt(32, 304, 16, 16), foot(23, 5.f), kProp * 0.8f);
    put(tileArt(64, 304, 16, 16), foot(18.4f, 4.6f), kProp);
    put(tileArt(128, 304, 16, 16), foot(24.4f, 4.6f), kProp);
    person("calciumtrice/heroes/DesertRogue.png", 17.6f, 6.3f, true, 1.1f);
    lights.push_back({cell(21.5f, 5.f), sf::Color(255, 200, 120), 4.5f});

    // --- The descent: the great gate, guarded -----------------------------------
    put(evilArt(128, 192, 128, 96), foot(27.5f, 2.f), kArt);
    torch(25); torch(30);
    put(evilArt(176, 408, 72, 112), foot(25.3f, 5.f), 0.9f);
    put(evilArt(176, 408, 72, 112), foot(29.7f, 5.f), 0.9f, sf::Color::White, true);
    person("calciumtrice/heroes/BronzeKnight.png", 26.4f, 5.6f, false, 0.2f);
    lights.push_back({cell(28.f, 2.6f), sf::Color(200, 70, 120), 4.f});

    // --- The trial obelisk: a skull-crowned pillar that glows while a trial waits ---
    {
        bool ready = false;
        for (int t = 1; t <= kTrialCount; ++t) ready = ready || trialAvailability(t).empty();
        const auto base = foot(22.5f, 10.6f);
        if (ready) {
            const float pulse = 0.5f + 0.5f * std::sin(now * 2.f);
            sf::CircleShape glow(kCell * 1.2f);
            glow.setScale({1.f, 0.45f}); glow.setOrigin({kCell * 1.2f, kCell * 1.2f}); glow.setPosition({base.x, base.y - 4});
            glow.setFillColor(sf::Color(200, 110, 255, static_cast<std::uint8_t>(60 + 70 * pulse)));
            window_.draw(glow);
        }
        put(evilArt(64, 96, 32, 96), base, 1.7f, ready ? sf::Color(235, 215, 255) : sf::Color(170, 165, 180));
        lights.push_back({{base.x, base.y - kCell * 2.5f}, ready ? sf::Color(200, 120, 255) : sf::Color(140, 130, 170), ready ? 3.4f : 2.2f});
    }

    // --- Townsfolk in the square ------------------------------------------------
    person("calciumtrice/heroes/Peasant.png", 5.f, 10.2f, true, 3.1f);
    person("calciumtrice/heroes/YoungThief.png", 27.5f, 10.f, false, 2.6f);
    put(tileArt(0, 304, 16, 16), foot(0.6f, 11.6f), kProp);
    put(tileArt(16, 304, 16, 16), foot(1.4f, 11.8f), kProp);
    put(tileArt(64, 304, 16, 16), foot(29.6f, 11.6f), kProp);
    put(tileArt(112, 304, 16, 16), foot(30.3f, 11.9f), kProp);

    // --- You, by the fountain ---------------------------------------------------
    {
        const int frame = static_cast<int>(now * 6.f) % 10;
        const auto at = foot(15, 8);
        sf::CircleShape shadow(16.f);
        shadow.setScale({1.f, 0.35f}); shadow.setOrigin({16.f, 16.f}); shadow.setPosition({at.x, at.y - 3});
        shadow.setFillColor(sf::Color(0, 0, 0, 100));
        window_.draw(shadow);
        put(animatedFrame(playerSpriteFrame(), 0, frame), {at.x, at.y + 6}, kProp);
        lights.push_back({cell(15.5f, 8.f), sf::Color(255, 236, 205), 5.f});
    }

    const Station stations[]{
        {&kTownInnSpot, "The Inn", "Rest by the fire: life, mana and cooldowns restored, free of charge.", "Click to rest (R)"},
        {&kTownStashSpot, "Your Stash", "Your equipment and bag. Changes are free in town.", "Click to open (B)"},
        {&kTownFountainSpot, "The Old Fountain",
         "Floors you leave pause. Returning resumes where you stood; nothing respawns.", ""},
        {&kTownMerchantSpot, "The Merchant", "Basic gear for 30 gold each. Buys what you bring back, for a fraction.",
         "Click to trade (Tab)"},
        {&kTownGateSpot, "The Descent", "Choose a dungeon and depth, or resume the floor you left.",
         "Click to choose (M). D resumes where you left off."},
        {&kTownObeliskSpot, "Trial Obelisk", "Sigils from the Goblin Warlord and the Lich open trials here. Win them to ascend.",
         "Click to approach."},
    };
    for (const auto& s : stations)
        if (hovering(*s.spot)) lights.push_back({{s.spot->position.x + s.spot->size.x / 2, s.spot->position.y + s.spot->size.y * 0.55f},
                                                 sf::Color(255, 220, 150), 4.5f});

    // --- Night: a cool ambient, warm pools of light ----------------------------
    if (ensureLightBlob()) {
        const sf::Vector2u size{static_cast<unsigned>(kTownScene.size.x), static_cast<unsigned>(kTownScene.size.y)};
        if (!townLight_ || townLight_->getSize() != size) townLight_.emplace(size);
        townLight_->clear(sf::Color(124, 126, 160));
        for (const auto& [center, color, radius] : lights) {
            const bool fire = color.r > 200 && color.g < 200;
            const float flicker = fire ? 0.92f + 0.08f * std::sin(now * 9.f + center.x * 0.37f) : 1.f;
            sf::Sprite light(*lightBlob_);
            const float scale = radius * flicker * kCell * 2 / 128.f;
            light.setOrigin({64, 64});
            light.setScale({scale, scale});
            light.setPosition(center - kTownScene.position);
            light.setColor(color);
            townLight_->draw(light, sf::BlendAdd);
        }
        townLight_->display();
        sf::Sprite overlay(townLight_->getTexture());
        overlay.setPosition(kTownScene.position);
        window_.draw(overlay, sf::BlendMultiply);
    }
    // Vignette, heavier at the bottom where the square fades into the night.
    {
        sf::VertexArray edges(sf::PrimitiveType::Triangles);
        const float l = kTownScene.position.x, t = kTownScene.position.y, r = l + kTownScene.size.x, b = t + kTownScene.size.y;
        const auto strip = [&](sf::Vector2f p0, sf::Vector2f p1, sf::Vector2f p2, sf::Vector2f p3, std::uint8_t alpha) {
            const sf::Color edge(0, 0, 0, alpha), none(0, 0, 0, 0);
            for (const auto& v : {sf::Vertex{p0, edge}, sf::Vertex{p1, edge}, sf::Vertex{p2, none},
                                  sf::Vertex{p0, edge}, sf::Vertex{p2, none}, sf::Vertex{p3, none}}) edges.append(v);
        };
        strip({l, b}, {r, b}, {r, b - 120}, {l, b - 120}, 170);
        strip({l, t}, {l, b}, {l + 80, b}, {l + 80, t}, 130);
        strip({r, t}, {r, b}, {r - 80, b}, {r - 80, t}, 130);
        window_.draw(edges);
    }
    ui_.frame(window_, kTownScene);

    // --- Names over each building; the hovered one explains itself --------------
    const Station* hovered = nullptr;
    sf::FloatRect hoveredPlate;
    for (const auto& s : stations) {
        const bool hot = hovering(*s.spot);
        const float width = ui_.textWidth(s.name, 17, ui::Font::Title) + 22;
        const sf::FloatRect plate{{s.spot->position.x + (s.spot->size.x - width) / 2, s.spot->position.y + s.spot->size.y - 30}, {width, 26}};
        sf::RectangleShape back(plate.size);
        back.setPosition(plate.position);
        back.setFillColor(sf::Color(10, 8, 8, hot ? 220 : 160));
        back.setOutlineThickness(1.f);
        back.setOutlineColor(hot ? ui::kGold : sf::Color(140, 108, 62, 140));
        window_.draw(back);
        if (hot) { hovered = &s; hoveredPlate = plate; }
        ui_.textCentered(window_, s.name, plate, 17, hot ? ui::kRare : ui::kGold, ui::Font::Title);
    }
    if (hovered) {
        std::vector<ui::Line> lines{{hovered->name, ui::kGold, 18, ui::Font::Title}, {hovered->about, ui::kText, 15}};
        if (*hovered->action) lines.push_back({hovered->action, ui::kInfo, 14});
        if (hovered->spot == &kTownGateSpot) lines.push_back({"You left off on floor " + std::to_string(currentFloor_) + ".", ui::kMuted, 14});
        // Under the building's name, so the name stays readable.
        ui_.tooltip(window_, lines, {hoveredPlate.position.x, hoveredPlate.position.y + hoveredPlate.size.y + 6}, 320, kTownScene);
    }
}

} // namespace engine
