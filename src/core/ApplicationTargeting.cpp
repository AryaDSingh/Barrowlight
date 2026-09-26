#include "core/Application.hpp"

#include <algorithm>
#include <cmath>
#include "core/PlayLayout.hpp"
#include "entities/MonsterInspection.hpp"
#include "entities/TalentEffects.hpp"

namespace engine {
namespace {
constexpr int kMapWidth = playLayout::mapWidth, kMapTop = playLayout::mapTop,
    kMapHeight = playLayout::mapHeight, kTile = playLayout::tileSize;
constexpr float kPanelX = 912.f, kPanelRight = 1264.f;
constexpr float kTalentsTop = 48.f, kRowHeight = 20.f;
constexpr std::size_t kPageSize = 9;
bool same(Position a, Position b) { return a.x == b.x && a.y == b.y; }
}

std::optional<Position> Application::screenToWorld(sf::Vector2i pixel) const {
    // mapPixelToCoords handles resizing using SFML's unchanged logical view.
    const auto p = window_.mapPixelToCoords(pixel);
    if (p.x < 0 || p.x >= kMapWidth || p.y < kMapTop || p.y >= kMapTop + kMapHeight)
        return std::nullopt;
    const Position tile{cameraX_ + static_cast<int>(p.x) / kTile,
        cameraY_ + (static_cast<int>(p.y) - kMapTop) / kTile};
    if (!map_.inBounds(tile.x, tile.y)) return std::nullopt;
    return tile;
}

std::optional<std::size_t> Application::talentAtPixel(sf::Vector2i pixel) const {
    const auto p = window_.mapPixelToCoords(pixel);
    if (p.x < kPanelX || p.x >= kPanelRight || p.y < kTalentsTop ||
        p.y >= kTalentsTop + kRowHeight * kPageSize) return std::nullopt;
    const std::size_t index = talentPage_ * kPageSize +
        static_cast<std::size_t>((p.y - kTalentsTop) / kRowHeight);
    if (index >= player_.talents().knownTalents().size()) return std::nullopt;
    return index;
}

std::vector<Actor*> Application::targetingEnemies() const {
    std::vector<Actor*> result;
    for (const auto& monster : monsters_)
        if (monster->stats().hp > 0) result.push_back(monster.get());
    return result;
}

TalentTarget Application::targetPreview(std::size_t index, Position cursor) {
    if (index >= player_.talents().knownTalents().size()) return {};
    return resolveTalentTarget(map_, exploredMap_, player_, targetingEnemies(),
        player_.talents().effectiveTalent(index), cursor);
}

void Application::cancelTargeting() {
    aimingTalent_.reset();
    inspecting_ = false;
    hoveredTalent_.reset();
}

void Application::requestTalent(std::size_t index) {
    const auto& known = player_.talents().knownTalents();
    if (index >= known.size()) return;
    const auto reason = talentUnavailableReason(player_, index);
    if (!reason.empty()) { log(reason); return; }
    cancelTargeting();
    const Talent talent = player_.talents().effectiveTalent(index);
    if (talent.targeting == TargetingMode::Self && talent.shape == EffectShape::SingleTarget) {
        tryUseTalent(index, player_.position());
        return;
    }
    aimingTalent_ = index;
    targetCursor_ = player_.position();
    if (talent.shape != EffectShape::Movement && talent.shape != EffectShape::AreaAroundSelf)
        cycleTarget();
}

void Application::cycleTarget(int direction) {
    std::vector<Position> candidates;
    for (const auto& monster : monsters_) {
        const Position p = monster->position();
        if (monster->stats().hp <= 0 || exploredMap_.at(p.x, p.y) != Visibility::Visible) continue;
        if (aimingTalent_) {
            const auto result = targetPreview(*aimingTalent_, p);
            // An intercepted enemy isn't independently targetable by this ray.
            if (!result.valid || std::find(result.affected.begin(), result.affected.end(),
                monster.get()) == result.affected.end()) continue;
        }
        candidates.push_back(p);
    }
    std::sort(candidates.begin(), candidates.end(), [&](Position a, Position b) {
        const auto from = player_.position();
        const int da = (a.x-from.x)*(a.x-from.x) + (a.y-from.y)*(a.y-from.y);
        const int db = (b.x-from.x)*(b.x-from.x) + (b.y-from.y)*(b.y-from.y);
        if (da != db) return da < db;
        return a.y != b.y ? a.y < b.y : a.x < b.x;
    });
    if (candidates.empty()) return;
    const auto found = std::find_if(candidates.begin(), candidates.end(),
        [&](Position p) { return same(p, targetCursor_); });
    int index = found == candidates.end() ? (direction > 0 ? -1 : 0) :
        static_cast<int>(found - candidates.begin());
    index = (index + direction + static_cast<int>(candidates.size())) % static_cast<int>(candidates.size());
    targetCursor_ = candidates[static_cast<std::size_t>(index)];
}

void Application::changeTalentPage(int direction) {
    cancelTargeting();
    const auto count = player_.talents().knownTalents().size();
    const int pages = static_cast<int>(std::max<std::size_t>(1, (count + kPageSize - 1) / kPageSize));
    talentPage_ = static_cast<std::size_t>((static_cast<int>(talentPage_) + direction + pages) % pages);
}

bool Application::handleTargetingKey(sf::Keyboard::Key key, bool shift) {
    if (key == sf::Keyboard::Key::PageDown) { changeTalentPage(1); return true; }
    if (key == sf::Keyboard::Key::PageUp) { changeTalentPage(-1); return true; }
    if (key == sf::Keyboard::Key::I) {
        const bool wasInspecting = inspecting_;
        cancelTargeting();
        inspecting_ = !wasInspecting;
        targetCursor_ = player_.position();
        if (inspecting_) cycleTarget();
        return true;
    }
    if (key == sf::Keyboard::Key::Tab) {
        if (!aimingTalent_ && !inspecting_) {
            inspecting_ = true;
            targetCursor_ = player_.position();
        }
        cycleTarget(shift ? -1 : 1);
        return true;
    }
    if (!aimingTalent_ && !inspecting_) return false;
    if (key == sf::Keyboard::Key::Enter) {
        if (aimingTalent_) tryUseTalent(*aimingTalent_, targetCursor_);
        return true;
    }
    Position delta;
    switch (key) {
        case sf::Keyboard::Key::W: case sf::Keyboard::Key::Up: delta.y = -1; break;
        case sf::Keyboard::Key::S: case sf::Keyboard::Key::Down: delta.y = 1; break;
        case sf::Keyboard::Key::A: case sf::Keyboard::Key::Left: delta.x = -1; break;
        case sf::Keyboard::Key::D: case sf::Keyboard::Key::Right: delta.x = 1; break;
        default: return false;
    }
    targetCursor_.x = std::clamp(targetCursor_.x + delta.x, 0, map_.width() - 1);
    targetCursor_.y = std::clamp(targetCursor_.y + delta.y, 0, map_.height() - 1);
    return true;
}

void Application::handleTargetingMouse(const sf::Event& event) {
    updateCamera(); // Events may follow movement before the next rendered frame.
    if (event.is<sf::Event::MouseLeft>()) {
        mousePixel_.reset(); hoveredTalent_.reset();
    }
    if (const auto* move = event.getIf<sf::Event::MouseMoved>()) {
        mousePixel_ = move->position;
        hoveredTalent_ = talentAtPixel(move->position);
        if (aimingTalent_ || inspecting_)
            if (const auto tile = screenToWorld(move->position)) targetCursor_ = *tile;
    }
    if (const auto* click = event.getIf<sf::Event::MouseButtonPressed>()) {
        mousePixel_ = click->position;
        if (click->button == sf::Mouse::Button::Right) { cancelTargeting(); return; }
        if (click->button != sf::Mouse::Button::Left) return;
        if (const auto index = talentAtPixel(click->position)) { requestTalent(*index); return; }
        const auto tile = screenToWorld(click->position);
        if (!tile) return; // Clicking HUD never casts through it.
        targetCursor_ = *tile;
        if (aimingTalent_) tryUseTalent(*aimingTalent_, targetCursor_);
        else inspecting_ = true;
    }
}

void Application::drawWrapped(const std::string& text, float x, float& y,
    std::size_t columns, sf::Color color, float bottom) {
    std::istringstream words(text);
    std::string word, line;
    auto flush = [&] {
        if (y + 16.f <= bottom) drawText(line, x, y, 13, color);
        y += 17.f;
        line.clear();
    };
    while (words >> word) {
        if (!line.empty() && line.size() + word.size() + 1 > columns) flush();
        if (!line.empty()) line += ' ';
        line += word;
    }
    if (!line.empty()) flush();
}

void Application::renderTargetingOverlay() {
    auto inViewport = [&](Position p) {
        return p.x >= cameraX_ && p.x < cameraX_ + kMapWidth / kTile &&
            p.y >= cameraY_ && p.y < cameraY_ + kMapHeight / kTile &&
            exploredMap_.at(p.x, p.y) == Visibility::Visible;
    };
    auto mark = [&](Position p, sf::Color color, bool fill) {
        if (!inViewport(p)) return;
        sf::RectangleShape box({kTile - 2.f, kTile - 2.f});
        const auto pos = worldToScreen(p.x, p.y);
        box.setPosition({pos.x + 1.f, pos.y + 1.f});
        box.setFillColor(fill ? sf::Color(color.r, color.g, color.b, 65) : sf::Color::Transparent);
        box.setOutlineColor(color);
        box.setOutlineThickness(-2.f);
        window_.draw(box);
    };
    std::optional<Position> inspectionTile;
    if (aimingTalent_ || inspecting_) inspectionTile = targetCursor_;
    else if (mousePixel_) inspectionTile = screenToWorld(*mousePixel_);
    if (inspectionTile) mark(*inspectionTile, sf::Color(245, 220, 100), false);
    if (!aimingTalent_) return;
    const auto preview = targetPreview(*aimingTalent_, targetCursor_);
    const sf::Color color = preview.valid ? sf::Color(80, 220, 220) : sf::Color(245, 100, 90);
    for (Position p : preview.area) mark(p, color, true);
    for (Position p : preview.movementPath) mark(p, sf::Color(130, 160, 255), false);
    auto arrow = [&](const std::vector<Position>& path) {
        for (std::size_t i = 1; i < path.size(); ++i) {
            if (!inViewport(path[i - 1]) || !inViewport(path[i])) continue;
            auto a = worldToScreen(path[i - 1].x, path[i - 1].y);
            auto b = worldToScreen(path[i].x, path[i].y);
            a += sf::Vector2f(kTile / 2.f, kTile / 2.f);
            b += sf::Vector2f(kTile / 2.f, kTile / 2.f);
            const sf::Vertex segment[] = {{a, color}, {b, color}};
            window_.draw(segment, 2, sf::PrimitiveType::Lines);
            if (i + 1 == path.size()) {
                const auto difference = b - a;
                const float length = std::sqrt(difference.x * difference.x + difference.y * difference.y);
                if (length <= 0.f) continue;
                const sf::Vector2f direction = difference / length;
                const sf::Vector2f perpendicular{-direction.y, direction.x};
                sf::ConvexShape head(3);
                head.setPoint(0, b);
                head.setPoint(1, b - direction * 10.f + perpendicular * 5.f);
                head.setPoint(2, b - direction * 10.f - perpendicular * 5.f);
                head.setFillColor(color); window_.draw(head);
            }
        }
    };
    arrow(preview.path);
    arrow(preview.movementPath);
    arrow(preview.chainPath);
    if (preview.blockedAt) mark(*preview.blockedAt, sf::Color(255, 90, 75), true);
    if (player_.talents().knownTalents()[*aimingTalent_].shape == EffectShape::Movement)
        mark(preview.destination, sf::Color(130, 255, 150), true);
}

void Application::renderTargetingPanel() {
    sf::RectangleShape panel({384.f, 720.f});
    panel.setPosition({896.f, 0.f}); panel.setFillColor(sf::Color(18, 22, 31));
    window_.draw(panel);
    const auto& talents = player_.talents().knownTalents();
    const std::size_t pages = std::max<std::size_t>(1, (talents.size() + kPageSize - 1) / kPageSize);
    talentPage_ = std::min(talentPage_, pages - 1);
    drawText("TALENTS  " + std::to_string(talentPage_ + 1) + "/" + std::to_string(pages),
        kPanelX, 10.f, 17, sf::Color(110, 220, 220));
    drawText("1-9 / click   PgUp/PgDn: pages", kPanelX, 30.f, 13, sf::Color(175, 185, 205));
    for (std::size_t row = 0; row < kPageSize; ++row) {
        const auto index = talentPage_ * kPageSize + row;
        if (index >= talents.size()) break;
        const int cooldown = player_.talents().cooldownRemaining(index);
        const bool active = aimingTalent_ && *aimingTalent_ == index;
        drawText(std::to_string(row + 1) + ". " + talents[index].name +
            (cooldown ? " [" + std::to_string(cooldown) + "]" : " [Ready]"),
            kPanelX, kTalentsTop + row * kRowHeight, 13,
            active ? sf::Color(255, 220, 100) :
            talentUnavailableReason(player_, index).empty() ? sf::Color(225, 230, 240) : sf::Color(125, 135, 150));
    }

    const auto selected = aimingTalent_ ? aimingTalent_ : hoveredTalent_;
    float y = 239.f;
    const sf::Color normal(210, 220, 235), accent(110, 220, 220);
    if (selected && *selected < talents.size()) {
        const auto talent = player_.talents().effectiveTalent(*selected);
        drawWrapped(talent.name, kPanelX, y, 43, accent, 463.f);
        const auto* rune = player_.talents().attachedRune(*selected);
        if (rune) {
            const auto* definition = findRune(rune->definitionId);
            drawWrapped(std::string(definition->name) + ": " + definition->description, kPanelX, y, 43, accent, 463.f);
        } else drawWrapped(talent.description, kPanelX, y, 43, normal, 463.f);
        drawWrapped("Cost: " + std::to_string(talent.manaCost) + " mana, " +
            std::to_string(talent.hpCost) + " HP | CD: " + std::to_string(talent.cooldownTurns) +
            " turns", kPanelX, y, 43, normal, 463.f);
        const std::string rule = talent.shape == EffectShape::Movement ?
            "Movement: up to " + std::to_string(talent.moveDistance) + " tiles; stops at blockers." :
            talent.projectile ? "Projectile: visible path; first enemy hit." :
            talent.targeting == TargetingMode::AdjacentEnemy ? "Range: adjacent (no diagonals)." :
            talent.targeting == TargetingMode::Self ? "Centered on yourself." : "Range: any visible enemy (direct spell).";
        drawWrapped(rule, kPanelX, y, 43, normal, 463.f);
        if (talent.tags & AreaTag) drawWrapped("Radius: " + std::to_string(talent.areaRadius), kPanelX, y, 43, normal, 463.f);
        const auto unavailable = talentUnavailableReason(player_, *selected);
        if (!unavailable.empty()) drawWrapped(unavailable, kPanelX, y, 43, sf::Color(255, 130, 110), 463.f);
        if (aimingTalent_) {
            const auto preview = targetPreview(*selected, targetCursor_);
            if (talent.effectKind == TalentEffectKind::Damage && !preview.affected.empty()) {
                int low = 0, high = 0;
                bool first = true;
                for (const auto* actor : preview.affected) {
                    auto hitTalent = talent;
                    if (actor == preview.chainedTarget) hitTalent.damagePercent /= 2;
                    const auto damage = estimateTalentDamage(hitTalent, player_, *actor);
                    low = first ? damage.normal : std::min(low, damage.normal);
                    high = std::max(high, damage.critical); first = false;
                }
                drawWrapped("On hit: " + std::to_string(low) + "-" + std::to_string(high) +
                    " damage (includes crit). Can miss. Targets: " +
                    std::to_string(preview.affected.size()), kPanelX, y, 43, accent, 463.f);
            }
            if (!preview.message.empty()) drawWrapped(preview.message, kPanelX, y, 43,
                preview.valid ? accent : sf::Color(255, 130, 110), 463.f);
        }
    } else {
        drawWrapped("Hover a talent for details. Select a talent to aim; self buffs and healing cast immediately.",
            kPanelX, y, 43, normal, 463.f);
    }

    y = 468.f;
    drawWrapped("ENEMY INSPECTION", kPanelX, y, 43, accent, 712.f);
    std::optional<Position> inspectTile;
    if (aimingTalent_ || inspecting_) inspectTile = targetCursor_;
    else if (mousePixel_) inspectTile = screenToWorld(*mousePixel_);
    bool found = false;
    if (inspectTile) {
        for (const auto& monster : monsters_) {
            if (!same(monster->position(), *inspectTile)) continue;
            const auto lines = inspectMonster(*monster, exploredMap_);
            if (lines.empty()) continue;
            found = true;
            for (const auto& line : lines) drawWrapped(line, kPanelX, y, 43, normal, 712.f);
            break;
        }
    }
    if (!found) drawWrapped("Hover a visible enemy, or press I / Tab to inspect with the keyboard. Hidden enemies cannot be inspected.",
        kPanelX, y, 43, normal, 712.f);

    drawText(aimingTalent_ ? "AIM: mouse / arrows  Tab: target  Enter / click: cast  Esc / right-click: cancel" :
        inspecting_ ? "INSPECT: mouse / arrows  Tab: enemy  I / Esc / right-click: close" :
        "WASD: move  1-9: talent  I: inspect  B: bag  V: runes  G: pickup  F5/F9: save/load",
        10.f, 68.f, 12, sf::Color(180, 210, 220));
}
} // namespace engine
