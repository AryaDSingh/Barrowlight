#include "core/Application.hpp"

#include <cstdint>
#include <optional>

#include "core/GameRules.hpp"
#include "core/PlayLayout.hpp"
#include "entities/HybridSpec.hpp"
#include "entities/PlayerLeveling.hpp"

namespace engine {

namespace {
constexpr unsigned int kWindowHeight = playLayout::windowHeight;
constexpr float kTileSize = static_cast<float>(playLayout::tileSize);
constexpr unsigned int kMapWidth = playLayout::mapWidth;
constexpr unsigned int kMapHeight = playLayout::mapHeight;
constexpr float kMapTop = static_cast<float>(playLayout::mapTop);

sf::Color dim(sf::Color c) {
    constexpr float kDimFactor = 0.35f;
    return sf::Color(static_cast<std::uint8_t>(c.r * kDimFactor),
                      static_cast<std::uint8_t>(c.g * kDimFactor),
                      static_cast<std::uint8_t>(c.b * kDimFactor));
}

// No sprite/tile art exists yet (see ARCHITECTURE_DECISIONS.md) -- each
// enemy type gets a distinct flat color so the roster is at least
// visually distinguishable at a glance.
// Keyed by MonsterType, not the display name string -- Prompt 22's
// Elite/Nightmare tiers prefix the name ("Elite Goblin", "Nightmare
// Goblin"), which would silently fail an exact-string match like
// `name == "Goblin"` and fall through to the default gray for every
// tiered monster. MonsterType is stable regardless of tier or display
// name, so this can't have the same failure mode again.
sf::Color monsterColor(MonsterType type) {
    switch (type) {
        case MonsterType::Goblin: return sf::Color(200, 60, 60);
        case MonsterType::Spider: return sf::Color(120, 200, 60);
        case MonsterType::Ogre: return sf::Color(140, 90, 50);
        case MonsterType::Archer: return sf::Color(210, 170, 60);
        case MonsterType::Shaman: return sf::Color(170, 70, 210);
        case MonsterType::Bomber: return sf::Color(230, 110, 30);
        case MonsterType::GoblinWarlord: return sf::Color(255, 215, 0);
        case MonsterType::Lich: return sf::Color(140, 220, 210); // pale, ghostly teal
        case MonsterType::Skeleton: return sf::Color(220, 220, 200); // bone white
    }
    return sf::Color(190, 190, 190); // unreachable -- all enum values handled above
}

// A visual border color for Elite/Nightmare monsters (Prompt 22) --
// drawn as a slightly larger square behind the monster's own type-
// colored tile, so a tiered monster is identifiable at a glance without
// needing to read the combat log. Base tier returns no value: nothing
// extra is drawn, a tier-0 monster looks exactly as it always has.
std::optional<sf::Color> tierBorderColor(MonsterTier tier) {
    switch (tier) {
        case MonsterTier::Base:
            return std::nullopt;
        case MonsterTier::Elite:
            return sf::Color(255, 200, 60); // amber
        case MonsterTier::Nightmare:
            // Stark white, not a saturated color -- a deep red border
            // (an earlier attempt) blended almost invisibly into the
            // Goblin's own red tile color when checked against an
            // actual screenshot, not just reasoned about in the
            // abstract. White has to contrast against every monster
            // color in the roster at once (red, green, brown, tan,
            // purple, orange), not just look distinct in isolation.
            return sf::Color(255, 255, 255);
    }
    return std::nullopt; // unreachable
}
} // namespace

void Application::drawText(const std::string& text, float x, float y, unsigned int size,
                            sf::Color color) {
    if (!fontLoaded_) return;
    sf::Text sfText(font_);
    sfText.setString(text);
    sfText.setCharacterSize(size);
    sfText.setFillColor(color);
    sfText.setPosition({x, y});
    window_.draw(sfText);
}

void Application::updateCamera() {
    // Viewport size in whole tiles -- derived from the window/tile
    // constants rather than hardcoded again, so this stays correct if
    // either ever changes.
    constexpr int kViewportWidthTiles = static_cast<int>(kMapWidth / kTileSize);
    constexpr int kViewportHeightTiles = static_cast<int>(kMapHeight / kTileSize);

    const int desiredX = player_.position().x - kViewportWidthTiles / 2;
    const int desiredY = player_.position().y - kViewportHeightTiles / 2;

    // Clamped to [0, map dimension - viewport dimension] so the camera
    // never scrolls past the map's own edges and shows empty space
    // beyond it. If the map is smaller than the viewport in either
    // dimension (not expected at the current 60x32 default, but not
    // assumed impossible either), max(0, ...) keeps the clamp range
    // valid instead of inverting.
    const int maxCameraX = std::max(0, map_.width() - kViewportWidthTiles);
    const int maxCameraY = std::max(0, map_.height() - kViewportHeightTiles);

    cameraX_ = std::clamp(desiredX, 0, maxCameraX);
    cameraY_ = std::clamp(desiredY, 0, maxCameraY);
}

sf::Vector2f Application::worldToScreen(int tileX, int tileY) const {
    return {static_cast<float>(tileX - cameraX_) * kTileSize,
            kMapTop + static_cast<float>(tileY - cameraY_) * kTileSize};
}

void Application::render() {
    window_.clear(sf::Color(10, 10, 14));

    if (mode_ == GameMode::ClassSelection) {
        renderClassSelection();
        window_.display();
        return;
    }

    if (mode_ == GameMode::GameOver) {
        renderGameOver();
        window_.display();
        return;
    }

    if (mode_ == GameMode::AbilityChoice) {
        renderAbilityChoice();
        window_.display();
        return;
    }

    if (mode_ == GameMode::AttributeAllocation) {
        renderAttributeAllocation();
        window_.display();
        return;
    }

    updateCamera();

    // Only the camera-visible range, not the whole map -- both a real
    // performance win now that the map (60x32, Prompt 18) is bigger
    // than the ~40x22-tile viewport, and it naturally avoids needing a
    // separate "is this tile on screen" check per tile. +1 on each
    // upper bound covers the partially-visible tile at the viewport's
    // trailing edge.
    const int viewStartX = cameraX_;
    const int viewEndX = std::min(map_.width(), cameraX_ + static_cast<int>(kMapWidth / kTileSize));
    const int viewStartY = cameraY_;
    const int viewEndY = std::min(map_.height(), cameraY_ + static_cast<int>(kMapHeight / kTileSize));

    for (int y = viewStartY; y < viewEndY; ++y) {
        for (int x = viewStartX; x < viewEndX; ++x) {
            const Visibility vis = exploredMap_.at(x, y);
            if (vis == Visibility::Hidden) {
                continue;
            }

            const TileType tileType = map_.tileAt(x, y).type;
            sf::Color baseColor;
            if (tileType == TileType::Wall) {
                baseColor = sf::Color(45, 45, 52);
            } else if (tileType == TileType::Door) {
                baseColor = sf::Color(180, 40, 40); // red -- the floor-transition door
            } else {
                baseColor = sf::Color(90, 90, 100);
            }

            sf::RectangleShape tileShape({kTileSize - 1.f, kTileSize - 1.f});
            tileShape.setPosition(worldToScreen(x, y));
            tileShape.setFillColor(vis == Visibility::Visible ? baseColor : dim(baseColor));
            window_.draw(tileShape);
        }
    }

    renderGroundItems();
    for (auto& m : monsters_) {
        if (m->stats().hp <= 0) {
            continue;
        }
        if (exploredMap_.at(m->position().x, m->position().y) != Visibility::Visible) {
            continue; // only draw what the player can currently see -- see Prompt 7 notes
        }

        const sf::Vector2f screenPos = worldToScreen(m->position().x, m->position().y);
        if (screenPos.x < 0.f || screenPos.x >= kMapWidth ||
            screenPos.y < kMapTop || screenPos.y >= kMapTop + kMapHeight) continue;

        // Elite/Nightmare border (Prompt 22): a slightly larger square
        // drawn first, in the tier's color, so the monster's own type-
        // colored tile (drawn on top, its normal size) reads as sitting
        // inside a visible outline. Base tier returns nullopt -- nothing
        // extra drawn, looks exactly as it always has.
        if (const std::optional<sf::Color> borderColor = tierBorderColor(m->tier());
            borderColor.has_value()) {
            constexpr float kBorderThickness = 3.f;
            sf::RectangleShape border(
                {kTileSize - 1.f + kBorderThickness * 2.f, kTileSize - 1.f + kBorderThickness * 2.f});
            border.setPosition({screenPos.x - kBorderThickness, screenPos.y - kBorderThickness});
            border.setFillColor(*borderColor);
            window_.draw(border);
        }

        sf::RectangleShape monsterShape({kTileSize - 1.f, kTileSize - 1.f});
        monsterShape.setPosition(screenPos);
        monsterShape.setFillColor(monsterColor(m->type()));
        window_.draw(monsterShape);

        const float hpFraction =
            static_cast<float>(m->stats().hp) / static_cast<float>(m->stats().maxHp);
        sf::RectangleShape hpBack({kTileSize - 1.f, 4.f});
        hpBack.setPosition({screenPos.x, screenPos.y - 6.f});
        hpBack.setFillColor(sf::Color(40, 20, 20));
        window_.draw(hpBack);

        sf::RectangleShape hpFront({(kTileSize - 1.f) * hpFraction, 4.f});
        hpFront.setPosition({screenPos.x, screenPos.y - 6.f});
        hpFront.setFillColor(sf::Color(220, 60, 60));
        window_.draw(hpFront);
    }

    sf::RectangleShape playerShape({kTileSize - 1.f, kTileSize - 1.f});
    playerShape.setPosition(worldToScreen(player_.position().x, player_.position().y));
    playerShape.setFillColor(sf::Color(240, 200, 60));
    window_.draw(playerShape);

    renderTargetingOverlay();

    constexpr float kBarWidth = 200.f;
    constexpr float kBarHeight = 14.f;

    const float hpFraction =
        static_cast<float>(player_.stats().hp) / static_cast<float>(player_.stats().maxHp);
    sf::RectangleShape hpBack({kBarWidth, kBarHeight});
    hpBack.setPosition({10.f, 10.f});
    hpBack.setFillColor(sf::Color(40, 20, 20));
    window_.draw(hpBack);
    sf::RectangleShape hpFront({kBarWidth * hpFraction, kBarHeight});
    hpFront.setPosition({10.f, 10.f});
    hpFront.setFillColor(sf::Color(200, 50, 50));
    window_.draw(hpFront);
    {
        std::ostringstream oss;
        oss << player_.stats().hp << '/' << player_.stats().maxHp;
        drawText(oss.str(), 10.f + kBarWidth + 8.f, 10.f, 13, sf::Color::White);
    }

    const float manaFraction = player_.stats().maxMana > 0
                                    ? static_cast<float>(player_.stats().mana) /
                                          static_cast<float>(player_.stats().maxMana)
                                    : 0.f;
    sf::RectangleShape manaBack({kBarWidth, kBarHeight});
    manaBack.setPosition({10.f, 28.f});
    manaBack.setFillColor(sf::Color(20, 20, 40));
    window_.draw(manaBack);
    sf::RectangleShape manaFront({kBarWidth * manaFraction, kBarHeight});
    manaFront.setPosition({10.f, 28.f});
    manaFront.setFillColor(sf::Color(50, 90, 220));
    window_.draw(manaFront);
    {
        std::ostringstream oss;
        oss << player_.stats().mana << '/' << player_.stats().maxMana;
        drawText(oss.str(), 10.f + kBarWidth + 8.f, 28.f, 13, sf::Color::White);
    }

    // Level/XP -- Prompt 20. "MAX" instead of a fraction once level 10
    // is reached, since grantXp() zeroes xp() there and "X/0" would
    // read as a bug, not a deliberate cap.
    {
        std::ostringstream oss;
        oss << "Level " << player_.level();
        if (player_.level() < 10) {
            oss << "  (" << player_.xp() << '/' << xpForNextLevel(player_.level()) << " XP)";
        } else {
            oss << "  (MAX)";
        }
        drawText(oss.str(), 10.f, 46.f, 13, sf::Color(200, 200, 160));
    }

    // Floor -- the multi-floor dungeon progression. Small and
    // unobtrusive, same styling as the Level line above it, just one
    // more fact about where the character currently stands.
    {
        std::ostringstream oss;
        oss << "Floor " << currentFloor_ << " / " << kFinalFloor;
        drawText(oss.str(), 290.f, 46.f, 13, sf::Color(180, 180, 200));
    }

    renderTargetingPanel();

    // On-screen combat log -- bottom-left, oldest message at top of the
    // block so new lines settle at the bottom, matching how a chat/log
    // window conventionally reads. Same backing-panel reasoning as the
    // talent list above.
    {
        constexpr float kLogX = 10.f;
        constexpr float kLogLineHeight = 16.f;
        const float logBottomY =
            static_cast<float>(kWindowHeight) - 10.f - kLogLineHeight;
        float logY = logBottomY - static_cast<float>(logMessages_.size() - 1) * kLogLineHeight;
        if (logMessages_.empty()) {
            logY = logBottomY;
        }

        if (!logMessages_.empty()) {
            sf::RectangleShape panel(
                {600.f, static_cast<float>(logMessages_.size()) * kLogLineHeight + 6.f});
            panel.setPosition({kLogX - 4.f, logY - 4.f});
            panel.setFillColor(sf::Color(0, 0, 0, 160));
            window_.draw(panel);
        }

        for (const std::string& message : logMessages_) {
            drawText(message, kLogX, logY, 13, sf::Color(210, 210, 210));
            logY += kLogLineHeight;
        }
    }

    // A prominent, top-center boss bar -- distinct from the small
    // floating per-monster bars -- only shown when the boss is alive AND
    // currently visible, same consistency rule as everything else
    // (Prompt 7's "only render what's currently visible").
    if (boss_ != nullptr && boss_->stats().hp > 0 &&
        exploredMap_.at(boss_->position().x, boss_->position().y) == Visibility::Visible) {
        constexpr float kBossBarWidth = 500.f;
        constexpr float kBossBarHeight = 20.f;
        const float bossX = 370.f;

        const float bossHpFraction =
            static_cast<float>(boss_->stats().hp) / static_cast<float>(boss_->stats().maxHp);
        sf::RectangleShape bossBack({kBossBarWidth, kBossBarHeight});
        bossBack.setPosition({bossX, 27.f});
        bossBack.setFillColor(sf::Color(35, 30, 10));
        window_.draw(bossBack);
        sf::RectangleShape bossFront({kBossBarWidth * bossHpFraction, kBossBarHeight});
        bossFront.setPosition({bossX, 27.f});
        bossFront.setFillColor(sf::Color(255, 215, 0));
        window_.draw(bossFront);
        drawText(boss_->name(), bossX, 5.f, 14, sf::Color(255, 215, 0));
    }

    if (inventoryOpen_) renderInventory();
    if (runesOpen_) renderRunes();
    window_.display();
}

void Application::renderClassSelection() {
    drawText("Choose your class", 60.f, 60.f, 28, sf::Color(230, 230, 230));

    drawText("1. Warrior", 60.f, 130.f, 20, sf::Color(230, 120, 90));
    drawText("Pure Strength. A free spammable basic attack, hp as the", 80.f, 158.f, 14,
             sf::Color(190, 190, 190));
    drawText("resource that matters, and the hardest single hit of any", 80.f, 176.f, 14,
             sf::Color(190, 190, 190));
    drawText("base class.", 80.f, 194.f, 14, sf::Color(190, 190, 190));

    drawText("2. Mage", 60.f, 230.f, 20, sf::Color(170, 120, 230));
    drawText("Pure Intelligence. The largest mana pool of any class and", 80.f, 258.f, 14,
             sf::Color(190, 190, 190));
    drawText("the lowest hp -- a true glass cannon. Mind Shatter can", 80.f, 276.f, 14,
             sf::Color(190, 190, 190));
    drawText("stun an enemy, turning a status effect only monsters", 80.f, 294.f, 14,
             sf::Color(190, 190, 190));
    drawText("could inflict before now back around on them.", 80.f, 312.f, 14,
             sf::Color(190, 190, 190));

    drawText("3. Thief", 60.f, 348.f, 20, sf::Color(120, 230, 140));
    drawText("Pure Dexterity. Low hp, but the highest possible dodge", 80.f, 376.f, 14,
             sf::Color(190, 190, 190));
    drawText("chance -- survives by not getting hit at all. Vault Kick", 80.f, 394.f, 14,
             sf::Color(190, 190, 190));
    drawText("lets you strike an adjacent enemy and leap back out of", 80.f, 412.f, 14,
             sf::Color(190, 190, 190));
    drawText("melee range in the same motion.", 80.f, 430.f, 14, sf::Color(190, 190, 190));

    drawText("Press 1, 2 or 3 to begin.", 60.f, 478.f, 16, sf::Color(150, 150, 150));
}

void Application::renderGameOver() {
    if (wonGame_) {
        drawText("Victory!", 60.f, 60.f, 32, sf::Color(255, 215, 0));
        drawText("You have slain the " + defeatedBossName_ + ".", 60.f, 120.f, 18,
                  sf::Color(210, 210, 210));
    } else {
        drawText("You Died", 60.f, 60.f, 32, sf::Color(200, 50, 50));
        drawText("The dungeon claims another.", 60.f, 120.f, 18, sf::Color(210, 210, 210));
    }
    drawText("Press Enter to return to class selection.", 60.f, 180.f, 16,
              sf::Color(150, 150, 150));
}

void Application::renderAbilityChoice() {
    const PlayerClass poolClass = hybridPoolClass(playerClass_);
    const char* poolClassName = poolClass == PlayerClass::Mage ? "Mage" : "Warrior";

    if (pendingHybridChoiceIsSpecIn_) {
        drawText("A new path opens...", 60.f, 60.f, 28, sf::Color(230, 230, 230));
        drawText("You've grown strong enough to begin drawing on a second discipline.",
                 60.f, 104.f, 15, sf::Color(190, 190, 190));
        std::ostringstream oss;
        oss << "Spec into the hybrid path? Pick a " << poolClassName
            << " ability now, and one more every level from here on.";
        drawText(oss.str(), 60.f, 124.f, 15, sf::Color(190, 190, 190));
    } else {
        drawText("Choose your next ability", 60.f, 60.f, 28, sf::Color(230, 230, 230));
        std::ostringstream oss;
        oss << "Pick one more " << poolClassName << " ability to add to your kit.";
        drawText(oss.str(), 60.f, 104.f, 15, sf::Color(190, 190, 190));
    }

    float y = 170.f;
    for (std::size_t i = 0; i < pendingHybridChoices_.size(); ++i) {
        const Talent& talent = pendingHybridChoices_[i];
        std::ostringstream label;
        label << (i + 1) << ". " << talent.name;
        drawText(label.str(), 60.f, y, 18, sf::Color(120, 200, 230));
        drawText(talent.description, 80.f, y + 24.f, 14, sf::Color(180, 180, 180));
        y += 60.f;
    }

    if (pendingHybridChoiceIsSpecIn_) {
        drawText("0. No thanks -- stay on your current path", 60.f, y + 10.f, 16,
                 sf::Color(150, 150, 150));
    }
}

void Application::renderAttributeAllocation() {
    drawText("Level up!", 60.f, 60.f, 28, sf::Color(230, 230, 230));

    std::ostringstream subtitle;
    subtitle << "You have " << player_.unspentAttributePoints()
              << (player_.unspentAttributePoints() == 1 ? " point" : " points")
              << " to spend. Choose one:";
    drawText(subtitle.str(), 60.f, 104.f, 16, sf::Color(190, 190, 190));

    const Stats& stats = player_.stats();

    std::ostringstream strLine;
    strLine << "1. Strength (currently " << stats.strength << ")";
    drawText(strLine.str(), 60.f, 160.f, 18, sf::Color(230, 140, 100));
    drawText("+1 Max HP. Scales Strength-based abilities.", 80.f, 184.f, 14,
             sf::Color(180, 180, 180));

    std::ostringstream dexLine;
    dexLine << "2. Dexterity (currently " << stats.dexterity << ")";
    drawText(dexLine.str(), 60.f, 224.f, 18, sf::Color(120, 220, 140));
    drawText("+0.5% Dodge (cap 25%), +0.5% Crit. Scales Dexterity-based abilities.", 80.f,
             248.f, 14, sf::Color(180, 180, 180));

    std::ostringstream intLine;
    intLine << "3. Intelligence (currently " << stats.intelligence << ")";
    drawText(intLine.str(), 60.f, 288.f, 18, sf::Color(140, 170, 230));
    drawText("+1 Max Mana. Scales Intelligence-based abilities.", 80.f, 312.f, 14,
             sf::Color(180, 180, 180));
}

} // namespace engine
