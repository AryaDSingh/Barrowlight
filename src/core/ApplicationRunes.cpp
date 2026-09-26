#include "core/Application.hpp"
#include <algorithm>
#include <limits>

namespace engine {
void Application::giveRune(const std::string& definitionId) {
    if (!findRune(definitionId) || nextItemId_ == std::numeric_limits<std::uint64_t>::max()) return;
    player_.talents().runes().push_back({definitionId, nextItemId_++, {}});
    log("Received ", findRune(definitionId)->name, " rune. V: manage runes.");
}

void Application::handleRuneKey(sf::Keyboard::Key key) {
    auto& kit = player_.talents();
    const auto count = kit.knownTalents().size();
    if (key == sf::Keyboard::Key::V) { runesOpen_ = false; return; }
    if (key == sf::Keyboard::Key::F5) { saveGame(); return; }
    const std::array<sf::Keyboard::Key, 4> choices{sf::Keyboard::Key::Num1, sf::Keyboard::Key::Num2,
                                                 sf::Keyboard::Key::Num3, sf::Keyboard::Key::Num4};
    if (runeChoiceAvailable_) for (std::size_t i = 0; i < choices.size(); ++i) {
        if (key != choices[i]) continue;
        if (nextItemId_ == std::numeric_limits<std::uint64_t>::max()) return;
        giveRune(kRunes[i].id);
        runeChoiceAvailable_ = false; // the chest already paid the acquisition turn
        runeSelection_ = kit.runes().size() - 1;
        runeFeedback_ = "Chosen rune is now owned. Select a compatible talent and press Enter.";
        return;
    }
    if (!count) return;
    runeTalentSelection_ = std::min(runeTalentSelection_, count - 1);
    if (key == sf::Keyboard::Key::Up) runeTalentSelection_ = (runeTalentSelection_ + count - 1) % count;
    else if (key == sf::Keyboard::Key::Down) runeTalentSelection_ = (runeTalentSelection_ + 1) % count;
    else if (!kit.runes().empty() && key == sf::Keyboard::Key::Left)
        runeSelection_ = (runeSelection_ + kit.runes().size() - 1) % kit.runes().size();
    else if (!kit.runes().empty() && key == sf::Keyboard::Key::Right)
        runeSelection_ = (runeSelection_ + 1) % kit.runes().size();
    else if (key == sf::Keyboard::Key::U) {
        if (!kit.removeRune(runeTalentSelection_)) { runeFeedback_ = "No rune in this talent's slot."; return; }
        log("Removed support rune; its running cooldown is preserved.");
        finishInventoryTurn();
    } else if (key == sf::Keyboard::Key::Enter && !kit.runes().empty()) {
        runeSelection_ = std::min(runeSelection_, kit.runes().size() - 1);
        const auto& rune = kit.runes()[runeSelection_];
        runeFeedback_ = runeUnavailableReason(kit.knownTalents()[runeTalentSelection_], rune.definitionId);
        if (!runeFeedback_.empty()) return;
        if (!kit.attachRune(runeSelection_, runeTalentSelection_)) {
            runeFeedback_ = "This rune is already attached here."; return;
        }
        log("Attached ", findRune(rune.definitionId)->name, " to ", kit.knownTalents()[runeTalentSelection_].name, ".");
        finishInventoryTurn();
    } else return;
    runeFeedback_.clear();
}

void Application::renderRunes() {
    const sf::Color accent(115,225,215), normal(220,228,240);
    sf::RectangleShape backdrop({1280.f,720.f}); backdrop.setFillColor(sf::Color(8,12,20,250)); window_.draw(backdrop);
    drawText("SKILL RUNES", 40.f, 24.f, 24, accent);
    drawText("Up/Down: talent   Left/Right: owned rune   Enter: attach/swap   U: remove   V/Esc: close", 40.f, 65.f, 15, normal);
    drawText("Changes cost 1 turn. Running cooldowns are preserved, then advance normally with the turn.", 40.f, 90.f, 14, normal);
    auto& kit = player_.talents();
    const auto& talents = kit.knownTalents();
    if (talents.empty()) return;
    runeTalentSelection_ = std::min(runeTalentSelection_, talents.size()-1);
    const auto first = (runeTalentSelection_ / 14) * 14;
    for (std::size_t i=first; i<talents.size() && i<first+14; ++i) {
        const auto* attached = kit.attachedRune(i);
        drawText((i==runeTalentSelection_ ? "> " : "  ") + talents[i].name +
            (attached ? " [" + std::string(findRune(attached->definitionId)->name) + "]" : " [empty]"),
            40.f, 145.f+(i-first)*29.f, 15, i==runeTalentSelection_ ? sf::Color(255,220,100) : normal);
    }
    float y=140.f;
    const auto& base = talents[runeTalentSelection_];
    drawWrapped(base.name + " | CD remaining: " + std::to_string(kit.cooldownRemaining(runeTalentSelection_)), 595.f, y, 69, accent, 530.f);
    if (!kit.runes().empty()) {
        runeSelection_ = std::min(runeSelection_, kit.runes().size()-1);
        const auto& rune = kit.runes()[runeSelection_];
        const auto* definition = findRune(rune.definitionId);
        drawWrapped("Rune " + std::to_string(runeSelection_+1) + "/" + std::to_string(kit.runes().size()) + ": " + definition->name,
                    595.f, y, 69, accent, 530.f);
        std::string location = "In rune bag";
        for (const auto& talent : talents) if (talent.id == rune.talentId) location = "Bound to " + talent.name;
        drawWrapped(location, 595.f, y, 69, normal, 530.f);
        drawWrapped(definition->description, 595.f, y, 69, normal, 530.f);
        const auto reason = runeUnavailableReason(base, rune.definitionId);
        if (!reason.empty()) drawWrapped(reason, 595.f, y, 69, sf::Color(255,140,120), 530.f);
        else {
            const auto now = kit.effectiveTalent(runeTalentSelection_);
            const auto after = resolveEffectiveTalent(base, rune.definitionId);
            const auto row = [&](const std::string& label, int before, int next) {
                drawWrapped(label + ": " + std::to_string(before) + " -> " + std::to_string(next), 595.f, y, 69, normal, 530.f);
            };
            row("Mana cost", now.manaCost, after.manaCost);
            row("Cooldown on next cast", now.cooldownTurns, after.cooldownTurns);
            row("Radius", now.areaRadius, after.areaRadius);
            row("Movement distance", now.moveDistance, after.moveDistance);
            row("Direct damage %", now.damagePercent, after.damagePercent);
            drawWrapped("Damage scaling keeps the base talent's cooldown tier. Displaced runes return to the rune bag.", 595.f, y, 69, normal, 530.f);
        }
    } else drawWrapped("No runes owned yet. Open the first floor's chest for a choice. More runes come from even-floor chests and bosses.", 595.f, y, 69, normal, 530.f);
    if (!runeFeedback_.empty()) drawWrapped(runeFeedback_, 595.f, y, 69, sf::Color(255,210,120), 560.f);
    if (runeChoiceAvailable_) {
        drawText("FIRST CHEST: choose one free rune (1-4)", 40.f, 585.f, 18, accent);
        drawText("1 Chain: projectile bounce at 50%   |   2 Widen: radius +1, mana +50%", 40.f, 618.f, 15, normal);
        drawText("3 Venom: 80% hit + poison 2 x 3   |   4 Swift Passage: move +2, cooldown +2", 40.f, 644.f, 15, normal);
        drawText("Venom excludes existing on-hit effects. Swift needs pure movement; Mage learns Blink at level 2.", 40.f, 676.f, 13, normal);
    }
}
}
