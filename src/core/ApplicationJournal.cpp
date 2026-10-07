#include "core/Application.hpp"
#include "entities/Lore.hpp"

#include <algorithm>

namespace engine {

// The journal (J): every piece of lore you've found, in the order you found
// it, with what it opened. What you haven't found is only a count.
namespace {
const sf::FloatRect kJournal{{150, 50}, {980, 610}};
const sf::FloatRect kJournalClose{{560, 604}, {160, 40}};
constexpr int kJournalRows = 3; // entries per column on one page
}

void Application::handleJournalMouse(const sf::Event& event) {
    if (const auto* move = event.getIf<sf::Event::MouseMoved>()) mousePixel_ = move->position;
    if (const auto* wheel = event.getIf<sf::Event::MouseWheelScrolled>())
        journalScroll_ = std::max(0, journalScroll_ + (wheel->delta < 0 ? 1 : -1));
    const auto* click = event.getIf<sf::Event::MouseButtonPressed>();
    if (!click || click->button != sf::Mouse::Button::Left) return;
    const auto p = sf::Vector2f(click->position);
    if (kJournalClose.contains(p) || !kJournal.contains(p)) journalOpen_ = false;
}

void Application::renderJournal() {
    if (!journalOpen_) return;
    const auto mouse = mousePixel_ ? std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)) : std::nullopt;
    beginMenu(170);
    ui_.panel(window_, kJournal, true, sf::Color(150, 120, 90));
    const float x = kJournal.position.x, w = kJournal.size.x, top = kJournal.position.y;
    ui_.textCentered(window_, "Journal", {{x, top + 14}, {w, 44}}, 34, ui::kUnique, ui::Font::Title);
    std::vector<const LoreEntry*> found;
    for (const auto& id : player_.lore) if (const auto* e = loreEntry(id)) found.push_back(e);
    const int unknown = static_cast<int>(loreEntries().size() - found.size());
    ui_.textCentered(window_, found.empty() ? std::string("Nothing yet. What the great foes carry, and what lies on the ground (G), is kept here.")
                                            : std::to_string(found.size()) + " found" + (unknown ? ", " + std::to_string(unknown) + " still out there." : ", and nothing left to find."),
                     {{x, top + 58}, {w, 22}}, 15, ui::kMuted);
    // Two columns, a page of entries at a time; the wheel or Up/Down turn it.
    const int perPage = 2 * kJournalRows;
    const int pages = std::max(1, (static_cast<int>(found.size()) + perPage - 1) / perPage);
    journalScroll_ = std::min(journalScroll_, pages - 1);
    const float colW = (w - 72) / 2;
    for (int i = 0; i < perPage; ++i) {
        const int index = journalScroll_ * perPage + i;
        if (index >= static_cast<int>(found.size())) break;
        const auto* e = found[static_cast<std::size_t>(index)];
        const float cx = x + 28 + (i / kJournalRows) * (colW + 16);
        float y = top + 96 + (i % kJournalRows) * 132.f;
        ui_.inset(window_, {{cx, y - 6}, {colW, 122}}, sf::Color::Transparent);
        ui_.text(window_, e->title, {cx + 12, y}, 18, ui::kGold, ui::Font::Title);
        y += 28;
        for (const char* line : e->text) { ui_.paragraph(window_, line, cx + 12, y, colW - 24, 14, ui::kText); y += 2; }
        if (e->opens) ui_.text(window_, std::string("Opened: ") + e->opens, {cx + 12, std::min(y + 2, top + 96 + (i % kJournalRows) * 132.f + 94)}, 13, ui::kInfo, ui::Font::Bold);
    }
    if (pages > 1) ui_.textCentered(window_, "Page " + std::to_string(journalScroll_ + 1) + " of " + std::to_string(pages) + "  (wheel or Up/Down)",
                                    {{x, top + 500}, {w, 20}}, 13, ui::kMuted);
    ui_.button(window_, kJournalClose, "Close (J)", mouse && kJournalClose.contains(*mouse), true, 16);
}

} // namespace engine
