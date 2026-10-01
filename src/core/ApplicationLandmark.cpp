#include "core/Application.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "core/PlayLayout.hpp"
#include "core/ScreenLayout.hpp"
#include "entities/StatusEffectLogic.hpp"
#include "world/FloorTheme.hpp"

namespace engine {

namespace {

constexpr const char* kTileset = "calciumtrice/tiles/dungeon_tileset_calciumtrice.png";
constexpr const char* kWaterFountain = "evildungeon/water-fountain.png";
constexpr const char* kBloodFountain = "evildungeon/blood-fountain.png";
constexpr const char* kPentagram = "evildungeon/pentagramm.png";

// Statue altars from the tileset: a warrior in the Barracks, the antlered
// idol in the Sanctum and the beast altar in the Crypts.
SpriteFrame altarFrame(FloorRegion region) {
    if (region == FloorRegion::Barracks) return {kTileset, sf::IntRect({304, 304}, {16, 32})};
    if (region == FloorRegion::Sanctum) return {kTileset, sf::IntRect({336, 304}, {16, 32})};
    return {kTileset, sf::IntRect({384, 304}, {32, 32})};
}

constexpr int kMightLifePercent = 15;
constexpr int kGraceGold = 25;
constexpr int kBloodFontLife = 4;
constexpr int kBloodFontCostPercent = 30;
constexpr int kRitualDoomPercent = 30;
constexpr int kRitualDoomTurns = 10;

const char* verb(LandmarkKind kind) {
    switch (kind) {
        case LandmarkKind::HealingFountain: case LandmarkKind::BloodFont: return "drink";
        case LandmarkKind::RitualCircle: return "step into the circle";
        default: return "kneel";
    }
}

} // namespace

bool Application::nearAltar() const {
    if (landmark_ == LandmarkKind::None) return false;
    const auto p = player_.position();
    return std::abs(p.x - landmarkAltar_.x) <= 1 && std::abs(p.y - landmarkAltar_.y) <= 1;
}

void Application::openShrine() {
    if (landmark_ == LandmarkKind::None) return;
    if (landmarkUsed_) { log("The ", landmarkName(landmark_, floorTheme(currentFloor_).region), " is spent."); return; }
    cancelTargeting();
    shrineMenu_ = true;
}

// What this floor's landmark offers. Each choice is used at most once,
// since any choice spends the landmark.
std::vector<Application::LandmarkChoice> Application::landmarkChoices() const {
    const auto& stats = player_.stats();
    switch (landmark_) {
        case LandmarkKind::Shrine: {
            const int might = std::max(1, stats.maxHp * kMightLifePercent / 100);
            return {{"Restoration", "healing", "Restore all life and mana and reset every cooldown.", "Free", true},
                    {"Might", "glowing-hands", "+3 damage on direct attacks for 80 turns.",
                     "Costs " + std::to_string(might) + " life", stats.hp > might},
                    {"Grace", "dodging", "+10% dodge for 80 turns.", "Costs " + std::to_string(kGraceGold) + " gold",
                     gold_ >= kGraceGold}};
        }
        case LandmarkKind::HealingFountain:
            return {{"Drink", "healing", "Restore all life and mana, and wash away poison, burns, chills and curses.", "Free", true}};
        case LandmarkKind::BloodFont: {
            const int cost = std::max(1, stats.hp * kBloodFontCostPercent / 100);
            return {{"Drink deep", "bleeding-heart", "+" + std::to_string(kBloodFontLife) + " maximum life, for the rest of the run.",
                     "Costs " + std::to_string(cost) + " life now", stats.hp > cost}};
        }
        case LandmarkKind::RitualCircle: {
            const int doom = std::max(1, stats.maxHp * kRitualDoomPercent / 100);
            return {{"Accept the pact", "skull-crossed-bones", "Gain an attribute point to spend at once.",
                     "Doom: lose " + std::to_string(doom) + " life in " + std::to_string(kRitualDoomTurns) +
                         " turns unless you cleanse it", true}};
        }
        case LandmarkKind::None: break;
    }
    return {};
}

void Application::chooseBlessing(int choice) {
    const auto choices = landmarkChoices();
    if (!shrineMenu_ || landmarkUsed_ || choice < 0 || choice >= static_cast<int>(choices.size())) return;
    if (!choices[static_cast<std::size_t>(choice)].affordable) {
        log("You can't pay that price right now.");
        return;
    }
    auto& stats = player_.stats();
    bool attributePoint = false;
    switch (landmark_) {
        case LandmarkKind::Shrine:
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
            break;
        case LandmarkKind::HealingFountain: {
            stats.hp = stats.maxHp; stats.mana = stats.maxMana;
            auto& effects = player_.statusEffects().active();
            effects.erase(std::remove_if(effects.begin(), effects.end(),
                [](const StatusEffectInstance& e) { return isCleansable(e.type); }), effects.end());
            break;
        }
        case LandmarkKind::BloodFont: {
            const int cost = std::max(1, stats.hp * kBloodFontCostPercent / 100);
            player_.baseStats().maxHp += kBloodFontLife;
            const int hp = stats.hp - cost;
            player_.refreshEquipmentStats();
            player_.stats().hp = std::max(1, hp);
            break;
        }
        case LandmarkKind::RitualCircle:
            player_.statusEffects().apply({StatusEffectType::Doom, kRitualDoomTurns, std::max(1, stats.maxHp * kRitualDoomPercent / 100)});
            ++player_.unspentAttributePoints();
            attributePoint = true;
            break;
        case LandmarkKind::None: return;
    }
    landmarkUsed_ = true;
    shrineMenu_ = false;
    soundManager_.play(SoundEffect::LevelUp);
    log("The ", landmarkName(landmark_, floorTheme(currentFloor_).region), " grants you ",
        choices[static_cast<std::size_t>(choice)].name, ".");
    finishInventoryTurn(); // using a landmark takes a turn
    if (attributePoint && mode_ == GameMode::Playing) mode_ = GameMode::AttributeAllocation;
}

// Which dialog card a choice uses: a single offer sits in the middle.
static int choiceSlot(int choice, int count) { return count == 1 ? 1 : choice; }

void Application::handleShrineKey(sf::Keyboard::Key key) {
    if (key == sf::Keyboard::Key::Escape) { shrineMenu_ = false; return; }
    if (key == sf::Keyboard::Key::Num1) chooseBlessing(0);
    else if (key == sf::Keyboard::Key::Num2) chooseBlessing(1);
    else if (key == sf::Keyboard::Key::Num3) chooseBlessing(2);
    else if (key == sf::Keyboard::Key::Enter && landmarkChoices().size() == 1) chooseBlessing(0);
}

void Application::handleShrineMouse(const sf::Event& event) {
    if (const auto* move = event.getIf<sf::Event::MouseMoved>()) mousePixel_ = move->position;
    const auto* click = event.getIf<sf::Event::MouseButtonPressed>();
    if (!click || click->button != sf::Mouse::Button::Left) return;
    const auto p = sf::Vector2f(click->position);
    if (screen::kShrineLeave.contains(p)) { shrineMenu_ = false; return; }
    const int count = static_cast<int>(landmarkChoices().size());
    for (int i = 0; i < count; ++i)
        if (screen::shrineChoice(choiceSlot(i, count)).contains(p)) { chooseBlessing(i); return; }
}

// The landmark's art, glow and nearby hint; drawn after walls, before actors.
void Application::renderLandmark() {
    if (landmark_ == LandmarkKind::None) return;
    const auto vis = exploredMap_.at(landmarkAltar_.x, landmarkAltar_.y);
    if (vis == Visibility::Hidden) return;
    const auto at = worldToScreen(landmarkAltar_.x, landmarkAltar_.y);
    const float tile = static_cast<float>(playLayout::tileSize);
    const bool lit = vis == Visibility::Visible;
    const float now = animationClock_.getElapsedTime().asSeconds();
    const sf::Color tint = !lit ? sf::Color(90, 90, 100) : landmarkUsed_ ? sf::Color(150, 150, 160) : sf::Color::White;
    const int frame = static_cast<int>(now * 6.f) % 4;

    switch (landmark_) {
        case LandmarkKind::Shrine: {
            if (lit && !landmarkUsed_) {
                // A slow pulse marks a shrine that still answers.
                const float pulse = 0.5f + 0.5f * std::sin(now * 2.2f);
                sf::CircleShape glow(tile * 0.95f);
                glow.setOrigin({tile * 0.95f, tile * 0.95f});
                glow.setPosition({at.x + tile / 2, at.y + tile / 2});
                glow.setFillColor(sf::Color(255, 210, 120, static_cast<std::uint8_t>(40 + 50 * pulse)));
                window_.draw(glow);
            }
            const float size = tile * 1.6f;
            sprites_.draw(window_, altarFrame(floorTheme(currentFloor_).region), {at.x + (tile - size) / 2, at.y + tile - size},
                          size, tint);
            break;
        }
        case LandmarkKind::HealingFountain: case LandmarkKind::BloodFont: {
            // A gargoyle in the wall face, spouting onto the floor below it.
            // Rows of the sheet: idle, starting, flowing, stopping.
            const char* sheet = landmark_ == LandmarkKind::BloodFont ? kBloodFountain : kWaterFountain;
            const SpriteFrame gargoyle{sheet, sf::IntRect({(landmarkUsed_ ? 0 : frame) * 32, (landmarkUsed_ ? 0 : 2) * 96}, {32, 96})};
            sprites_.draw(window_, gargoyle, {at.x - tile, at.y - tile}, tile * 3, tint);
            break;
        }
        case LandmarkKind::RitualCircle: {
            // The circle on the floor; candles and flames burn while it's unspent.
            const SpriteFrame circle{kPentagram, sf::IntRect({(landmarkUsed_ ? 0 : frame) * 110, (landmarkUsed_ ? 0 : 2) * 120}, {110, 120})};
            sprites_.draw(window_, circle, {at.x + tile / 2 - 60, at.y + tile / 2 - 85}, 120.f, tint);
            break;
        }
        case LandmarkKind::None: break;
    }
    if (nearAltar())
        mapHints_.push_back({std::string(landmarkName(landmark_, floorTheme(currentFloor_).region)) +
            (landmarkUsed_ ? ". Its power is spent." : std::string(". G or click it to ") + verb(landmark_) + "."), ui::kRare});
}

sf::Color Application::landmarkLightColor() const {
    switch (landmark_) {
        case LandmarkKind::HealingFountain: return sf::Color(110, 180, 255);
        case LandmarkKind::BloodFont: return sf::Color(255, 70, 60);
        case LandmarkKind::RitualCircle: return sf::Color(200, 110, 255);
        default: return sf::Color(255, 205, 120);
    }
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
    const char* subtitle = landmark_ == LandmarkKind::Shrine ? "Kneel and choose one blessing. The shrine answers only once."
        : landmark_ == LandmarkKind::RitualCircle ? "Power, for a price. The circle burns out once used."
        : "The water runs only once for each traveller.";
    if (landmark_ == LandmarkKind::BloodFont) subtitle = "It flows red, and only once.";
    ui_.textCentered(window_, subtitle, {{x, kShrineDialog.position.y + 66}, {w, 24}}, 16, ui::kMuted);
    const auto choices = landmarkChoices();
    const int count = static_cast<int>(choices.size());
    for (int i = 0; i < count; ++i) {
        const auto& c = choices[static_cast<std::size_t>(i)];
        const auto r = shrineChoice(choiceSlot(i, count));
        ui_.inset(window_, r, hovered(r) && c.affordable ? ui::kRare : sf::Color::Transparent);
        ui_.text(window_, std::to_string(i + 1), {r.position.x + 10, r.position.y + 6}, 15, ui::kMuted, ui::Font::Bold);
        const sf::FloatRect art{{r.position.x + r.size.x / 2 - 38, r.position.y + 18}, {76, 76}};
        ui_.inset(window_, art, sf::Color(255, 214, 96, 70));
        ui_.icon(window_, c.icon, {{art.position.x + 12, art.position.y + 12}, {52, 52}}, c.affordable ? ui::kRare : ui::kMuted);
        ui_.textCentered(window_, c.name, {{r.position.x, r.position.y + 104}, {r.size.x, 32}}, 22,
            c.affordable ? ui::kGold : ui::kMuted, ui::Font::Title);
        float y = r.position.y + 144;
        ui_.paragraph(window_, c.effect, r.position.x + 18, y, r.size.x - 36, 15, ui::kText);
        float costY = r.position.y + r.size.y - 50;
        ui_.paragraph(window_, c.cost, r.position.x + 18, costY, r.size.x - 36, 14, c.affordable ? ui::kGood : ui::kBad, ui::Font::Bold);
    }
    ui_.button(window_, kShrineLeave, "Leave (Esc)", hovered(kShrineLeave), true, 16);
}

} // namespace engine
