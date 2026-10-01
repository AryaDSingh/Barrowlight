#include "core/Application.hpp"

#include <cmath>
#include <cstdlib>

#include "core/PlayLayout.hpp"
#include "core/ScreenLayout.hpp"
#include "world/FloorTheme.hpp"

namespace engine {

namespace {

constexpr const char* kTileset = "calciumtrice/tiles/dungeon_tileset_calciumtrice.png";

// Statue altars from the tileset: a warrior in the Barracks, the antlered
// idol in the Sanctum and the beast altar in the Crypts.
SpriteFrame altarFrame(FloorRegion region) {
    if (region == FloorRegion::Barracks) return {kTileset, sf::IntRect({304, 304}, {16, 32})};
    if (region == FloorRegion::Sanctum) return {kTileset, sf::IntRect({336, 304}, {16, 32})};
    return {kTileset, sf::IntRect({384, 304}, {32, 32})};
}

struct Blessing { const char* name; const char* icon; const char* effect; };
constexpr Blessing kBlessings[]{
    {"Restoration", "healing", "Restore all life and mana and reset every cooldown."},
    {"Might", "glowing-hands", "+3 damage on direct attacks for 80 turns."},
    {"Grace", "dodging", "+10% dodge for 80 turns."},
};
constexpr int kMightLifePercent = 15;
constexpr int kGraceGold = 25;

} // namespace

bool Application::nearAltar() const {
    if (landmark_ == LandmarkKind::None) return false;
    const auto p = player_.position();
    return std::abs(p.x - landmarkAltar_.x) <= 1 && std::abs(p.y - landmarkAltar_.y) <= 1;
}

void Application::openShrine() {
    if (landmark_ == LandmarkKind::None) return;
    if (landmarkUsed_) { log("The ", landmarkName(landmark_, floorTheme(currentFloor_).region), " is silent now."); return; }
    cancelTargeting();
    shrineMenu_ = true;
}

std::string Application::blessingCost(int choice) const {
    if (choice == 1) return "Costs " + std::to_string(std::max(1, player_.stats().maxHp * kMightLifePercent / 100)) + " life";
    if (choice == 2) return "Costs " + std::to_string(kGraceGold) + " gold";
    return "Free";
}

bool Application::canAffordBlessing(int choice) const {
    if (choice == 1) return player_.stats().hp > std::max(1, player_.stats().maxHp * kMightLifePercent / 100);
    if (choice == 2) return gold_ >= kGraceGold;
    return true;
}

void Application::chooseBlessing(int choice) {
    if (!shrineMenu_ || landmarkUsed_ || choice < 0 || choice > 2) return;
    if (!canAffordBlessing(choice)) {
        log(choice == 1 ? "You are too wounded to offer your blood." : "You don't have enough gold for that offering.");
        return;
    }
    auto& stats = player_.stats();
    if (choice == 0) {
        stats.hp = stats.maxHp; stats.mana = stats.maxMana;
        player_.talents().resetCooldowns();
    } else if (choice == 1) {
        stats.hp -= std::max(1, stats.maxHp * kMightLifePercent / 100);
        player_.statusEffects().apply({StatusEffectType::Empowered, 80, 3});
    } else {
        gold_ -= kGraceGold;
        player_.statusEffects().apply({StatusEffectType::Evasion, 80, 10});
    }
    landmarkUsed_ = true;
    shrineMenu_ = false;
    soundManager_.play(SoundEffect::LevelUp);
    log("The ", landmarkName(landmark_, floorTheme(currentFloor_).region), " grants you ", kBlessings[choice].name, ".");
    finishInventoryTurn(); // kneeling takes a turn
}

void Application::handleShrineKey(sf::Keyboard::Key key) {
    if (key == sf::Keyboard::Key::Escape) { shrineMenu_ = false; return; }
    if (key == sf::Keyboard::Key::Num1) chooseBlessing(0);
    else if (key == sf::Keyboard::Key::Num2) chooseBlessing(1);
    else if (key == sf::Keyboard::Key::Num3) chooseBlessing(2);
}

void Application::handleShrineMouse(const sf::Event& event) {
    if (const auto* move = event.getIf<sf::Event::MouseMoved>()) mousePixel_ = move->position;
    const auto* click = event.getIf<sf::Event::MouseButtonPressed>();
    if (!click || click->button != sf::Mouse::Button::Left) return;
    const auto p = sf::Vector2f(click->position);
    if (screen::kShrineLeave.contains(p)) { shrineMenu_ = false; return; }
    for (int i = 0; i < 3; ++i)
        if (screen::shrineChoice(i).contains(p)) { chooseBlessing(i); return; }
}

// The altar, its glow and the nearby hint; drawn after the floor tiles.
void Application::renderLandmark() {
    if (landmark_ == LandmarkKind::None) return;
    const auto vis = exploredMap_.at(landmarkAltar_.x, landmarkAltar_.y);
    if (vis == Visibility::Hidden) return;
    const auto at = worldToScreen(landmarkAltar_.x, landmarkAltar_.y);
    const float tile = static_cast<float>(playLayout::tileSize);
    if (!onMap(at)) return;
    const bool lit = vis == Visibility::Visible;
    if (lit && !landmarkUsed_) {
        // A slow pulse marks a shrine that still answers.
        const float pulse = 0.5f + 0.5f * std::sin(animationClock_.getElapsedTime().asSeconds() * 2.2f);
        sf::CircleShape glow(tile * 0.95f);
        glow.setOrigin({tile * 0.95f, tile * 0.95f});
        glow.setPosition({at.x + tile / 2, at.y + tile / 2});
        glow.setFillColor(sf::Color(255, 210, 120, static_cast<std::uint8_t>(40 + 50 * pulse)));
        window_.draw(glow);
    }
    const auto frame = altarFrame(floorTheme(currentFloor_).region);
    const float size = tile * 1.6f;
    const sf::Color tint = !lit ? sf::Color(90, 90, 100) : landmarkUsed_ ? sf::Color(150, 150, 160) : sf::Color::White;
    sprites_.draw(window_, frame, {at.x + (tile - size) / 2, at.y + tile - size}, size, tint);
    if (nearAltar())
        mapHints_.push_back({std::string(landmarkName(landmark_, floorTheme(currentFloor_).region)) +
            (landmarkUsed_ ? ". Its power is spent." : ". G or click the altar to kneel."), ui::kRare});
}

void Application::renderShrine() {
    if (!shrineMenu_) return;
    using namespace screen;
    const auto mouse = mousePixel_ ? std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)) : std::nullopt;
    const auto hovered = [&](const sf::FloatRect& r) { return mouse && r.contains(*mouse); };
    sf::RectangleShape dim({1280, 720}); dim.setFillColor(sf::Color(0, 0, 0, 140)); window_.draw(dim);
    ui_.panel(window_, kShrineDialog, true, sf::Color(150, 140, 130));
    const float x = kShrineDialog.position.x, w = kShrineDialog.size.x;
    ui_.textCentered(window_, landmarkName(landmark_, floorTheme(currentFloor_).region),
        {{x, kShrineDialog.position.y + 20}, {w, 44}}, 34, ui::kRare, ui::Font::Title);
    ui_.textCentered(window_, "Kneel and choose one blessing. The shrine answers only once.",
        {{x, kShrineDialog.position.y + 66}, {w, 24}}, 16, ui::kMuted);
    for (int i = 0; i < 3; ++i) {
        const auto r = shrineChoice(i);
        const bool affordable = canAffordBlessing(i);
        ui_.inset(window_, r, hovered(r) && affordable ? ui::kRare : sf::Color::Transparent);
        ui_.text(window_, std::to_string(i + 1), {r.position.x + 10, r.position.y + 6}, 15, ui::kMuted, ui::Font::Bold);
        const sf::FloatRect art{{r.position.x + r.size.x / 2 - 38, r.position.y + 18}, {76, 76}};
        ui_.inset(window_, art, sf::Color(255, 214, 96, 70));
        ui_.icon(window_, kBlessings[i].icon, {{art.position.x + 12, art.position.y + 12}, {52, 52}},
            affordable ? ui::kRare : ui::kMuted);
        ui_.textCentered(window_, kBlessings[i].name, {{r.position.x, r.position.y + 104}, {r.size.x, 32}}, 24,
            affordable ? ui::kGold : ui::kMuted, ui::Font::Title);
        float y = r.position.y + 144;
        ui_.paragraph(window_, kBlessings[i].effect, r.position.x + 18, y, r.size.x - 36, 15, ui::kText);
        ui_.textCentered(window_, blessingCost(i), {{r.position.x, r.position.y + r.size.y - 34}, {r.size.x, 24}}, 15,
            affordable ? ui::kGood : ui::kBad, ui::Font::Bold);
    }
    ui_.button(window_, kShrineLeave, "Leave (Esc)", hovered(kShrineLeave), true, 16);
}

} // namespace engine
