// The title screen: a living scene from the game behind a clean menu.
//
// The backdrop is a real floor, generated and drawn by the engine itself:
// one of the six dungeons, your character standing by a lit brazier, the
// torchlight and fires flickering. It changes every so often, fading
// through black. The menu sits over a dark gradient on the left.

#include "core/Application.hpp"
#include "core/PlayLayout.hpp"
#include "entities/PlayerClassFactory.hpp"

#include <array>
#include <filesystem>
#include <tuple>

namespace engine {

namespace {
// A floor from each dungeon, as deep as its most characteristic stretch.
constexpr std::array<int, 6> kTitleFloors{3, 15, 23, 29, 33, 40};
constexpr float kTitleSceneSeconds = 40.f, kTitleFade = 1.2f;
constexpr float kMenuX = 74.f, kMenuTop = 330.f, kMenuStep = 54.f;
}

std::vector<Application::TitleItem> Application::titleItems() const {
    std::vector<TitleItem> items;
    if (std::filesystem::exists("savegame.txt")) items.push_back(TitleItem::Continue);
    for (const auto item : {TitleItem::NewGame, TitleItem::Lab, TitleItem::Sandbox, TitleItem::Quit}) items.push_back(item);
    return items;
}

const char* Application::titleLabel(TitleItem item) {
    switch (item) {
        case TitleItem::Continue: return "Continue";
        case TitleItem::NewGame: return "New Game";
        case TitleItem::Lab: return "Encounter Lab";
        case TitleItem::Sandbox: return "Sandbox";
        case TitleItem::Quit: return "Quit";
    }
    return "";
}

sf::FloatRect Application::titleItemRect(std::size_t index) const {
    return {{kMenuX - 18.f, kMenuTop + kMenuStep * static_cast<float>(index) - 6.f}, {380.f, 46.f}};
}

void Application::enterTitle() {
    mode_ = GameMode::Title;
    pauseMenu_ = false; sandboxMenu_ = false; journalOpen_ = false; inventoryOpen_ = false;
    labMode_ = false; sandboxMode_ = false;
    titleSelection_ = 0;
    buildTitleScene();
}

// A fresh scene: the next dungeon in turn, your character, a brazier beside them.
void Application::buildTitleScene() {
    titleScene_ = (titleScene_ + 1) % static_cast<int>(kTitleFloors.size());
    static constexpr std::array<PlayerClass, 3> kOrigins{PlayerClass::Warrior, PlayerClass::Mage, PlayerClass::Thief};
    playerClass_ = kOrigins[static_cast<std::size_t>(titleScene_) % kOrigins.size()];
    player_.baseStats() = statsForClass(playerClass_);
    player_.stats() = player_.baseStats();
    player_.statusEffects().active().clear();
    player_.lightSource = 1; player_.lightLit = true;
    darknessEnabled_ = true; // the scene is lit by its own torches and braziers
    trial_ = 0; boss_ = nullptr; floorCache_.clear();
    currentFloor_ = kTitleFloors[static_cast<std::size_t>(titleScene_)];
    regenerateLevel(freshSeed());
    // A brazier beside you, on the first spot with room for one.
    const auto me = player_.position();
    for (const Position o : {Position{2, 1}, Position{-2, 1}, Position{2, -1}, Position{-2, -1}, Position{0, 2}, Position{3, 0}, Position{-3, 0}}) {
        const auto before = props_.size();
        placeBraziers(me, {o});
        if (props_.size() > before) break;
    }
    updateFieldOfView();
    // Everything within 12 tiles is in view, so the scene shows its rooms; the
    // lighting still keeps what no torch or brazier reaches in deep gloom.
    std::vector<Visibility> seen(static_cast<std::size_t>(map_.width() * map_.height()), Visibility::Hidden);
    for (int y = 0; y < map_.height(); ++y)
        for (int x = 0; x < map_.width(); ++x)
            if (std::max(std::abs(x - me.x), std::abs(y - me.y)) <= 12) seen[static_cast<std::size_t>(y * map_.width() + x)] = Visibility::Visible;
    exploredMap_.restoreAll(std::move(seen));
    titleClock_.restart();
}

void Application::tickTitle() {
    if (titleClock_.getElapsedTime().asSeconds() >= kTitleSceneSeconds) buildTitleScene();
}

void Application::chooseTitle(TitleItem item) {
    switch (item) {
        case TitleItem::Continue: loadGame(); break;
        case TitleItem::NewGame: labMode_ = false; sandboxMode_ = false; mode_ = GameMode::ClassSelection; break;
        case TitleItem::Lab: labMode_ = true; sandboxMode_ = false; mode_ = GameMode::ClassSelection; break;
        case TitleItem::Sandbox: sandboxMode_ = true; labMode_ = false; mode_ = GameMode::ClassSelection; break;
        case TitleItem::Quit: window_.close(); break;
    }
}

void Application::handleTitleEvent(const sf::Event& event) {
    const auto items = titleItems();
    titleSelection_ = std::min(titleSelection_, items.size() - 1);
    if (const auto* key = event.getIf<sf::Event::KeyPressed>()) {
        using K = sf::Keyboard::Key;
        if (key->code == K::Up || key->code == K::W) titleSelection_ = (titleSelection_ + items.size() - 1) % items.size();
        if (key->code == K::Down || key->code == K::S) titleSelection_ = (titleSelection_ + 1) % items.size();
        if (key->code == K::Enter || key->code == K::Space) chooseTitle(items[titleSelection_]);
        return;
    }
    if (const auto* move = event.getIf<sf::Event::MouseMoved>()) {
        mousePixel_ = move->position;
        const auto p = window_.mapPixelToCoords(move->position, playView_);
        for (std::size_t i = 0; i < items.size(); ++i) if (titleItemRect(i).contains(p)) titleSelection_ = i;
        return;
    }
    if (const auto* click = event.getIf<sf::Event::MouseButtonPressed>(); click && click->button == sf::Mouse::Button::Left) {
        const auto p = window_.mapPixelToCoords(click->position, playView_);
        for (std::size_t i = 0; i < items.size(); ++i) if (titleItemRect(i).contains(p)) { chooseTitle(items[i]); return; }
    }
}

// Drawn over the scene: a dark fall-off on the left, the name, the menu, and a fade.
void Application::renderTitle() {
    window_.setView(playView_);
    const float width = playLayout::screenWidth, height = 720.f;
    sf::VertexArray shade(sf::PrimitiveType::Triangles);
    const auto band = [&](float x0, float x1, std::uint8_t a0, std::uint8_t a1) {
        const sf::Color c0(6, 6, 10, a0), c1(6, 6, 10, a1);
        for (const auto& v : {sf::Vertex{{x0, 0}, c0}, sf::Vertex{{x1, 0}, c1}, sf::Vertex{{x1, height}, c1},
                              sf::Vertex{{x0, 0}, c0}, sf::Vertex{{x1, height}, c1}, sf::Vertex{{x0, height}, c0}})
            shade.append(v);
    };
    band(0, 300, 235, 190);
    band(300, 640, 190, 0);
    // A soft vignette along the top and bottom edges.
    for (const auto& [y0, y1, a0, a1] : {std::tuple<float, float, std::uint8_t, std::uint8_t>{0.f, 110.f, std::uint8_t{150}, std::uint8_t{0}}, {610.f, 720.f, std::uint8_t{0}, std::uint8_t{170}}}) {
        const sf::Color c0(0, 0, 0, a0), c1(0, 0, 0, a1);
        for (const auto& v : {sf::Vertex{{0, y0}, c0}, sf::Vertex{{width, y0}, c0}, sf::Vertex{{width, y1}, c1},
                              sf::Vertex{{0, y0}, c0}, sf::Vertex{{width, y1}, c1}, sf::Vertex{{0, y1}, c1}})
            shade.append(v);
    }
    window_.draw(shade);

    ui_.text(window_, "Barrowlight", {kMenuX - 4.f, 150.f}, 74, ui::kGold, ui::Font::Title);
    ui_.text(window_, "Descend. Keep your light.", {kMenuX, 246.f}, 18, ui::kMuted, ui::Font::Bold);

    const auto items = titleItems();
    titleSelection_ = std::min(titleSelection_, items.size() - 1);
    for (std::size_t i = 0; i < items.size(); ++i) {
        const auto r = titleItemRect(i);
        const bool on = i == titleSelection_;
        if (on) {
            // A small bronze diamond marks the choice.
            sf::ConvexShape mark(4);
            const sf::Vector2f c{r.position.x + 6.f, r.position.y + r.size.y / 2.f};
            mark.setPoint(0, {c.x, c.y - 6}); mark.setPoint(1, {c.x + 6, c.y}); mark.setPoint(2, {c.x, c.y + 6}); mark.setPoint(3, {c.x - 6, c.y});
            mark.setFillColor(ui::kGold);
            window_.draw(mark);
        }
        ui_.text(window_, titleLabel(items[i]), {kMenuX, r.position.y + 4.f}, on ? 32 : 28, on ? ui::kGold : ui::kText, ui::Font::Title);
    }

    // Each scene fades in from black and out again before the next.
    const float t = titleClock_.getElapsedTime().asSeconds();
    const float fade = t < kTitleFade ? 1.f - t / kTitleFade : t > kTitleSceneSeconds - kTitleFade ? (t - (kTitleSceneSeconds - kTitleFade)) / kTitleFade : 0.f;
    if (fade > 0.f) {
        sf::RectangleShape black({width, height});
        black.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(255 * std::clamp(fade, 0.f, 1.f))));
        window_.draw(black);
    }
}

} // namespace engine
