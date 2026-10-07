#pragma once

#include <algorithm>
#include <array>
#include <optional>
#include <cctype>
#include <string>
#include <vector>
#include <SFML/Graphics/Color.hpp>
#include "core/UiKit.hpp"

namespace engine {

// Keywords: the game's recurring mechanics, highlighted wherever a
// description names them and explained on hover (like Path of Exile's
// keywords). Each lists the word forms that count as it.
struct Keyword {
    const char* name;
    std::vector<const char*> forms; // lower case
    sf::Color color;
    const char* text;
};

inline const std::vector<Keyword>& keywords() {
    static const std::vector<Keyword> all{
        {"Heat", {"heat"}, sf::Color(255, 150, 70),
         "Builds near furnaces and fire, and from some abilities. Away from heat it fades by 1 a turn; water quenches it. At 10 or more it burns you for 2 a turn."},
        {"Burn", {"burn", "burns", "burning", "burned"}, sf::Color(244, 124, 52),
         "Fire damage each turn. It reveals you if you are hidden, and water puts it out."},
        {"Poison", {"poison", "poisons", "poisoned"}, sf::Color(140, 190, 80), "Damage each turn. It reveals you if you are hidden."},
        {"Bleed", {"bleed", "bleeds", "bleeding"}, sf::Color(210, 50, 60), "Damage each turn, and the living leave a trail of blood."},
        {"Chill", {"chill", "chills", "chilled"}, sf::Color(150, 210, 250), "Slowed by the cold: it acts less often."},
        {"Shock", {"shock", "shocks", "shocked"}, sf::Color(120, 170, 255), "Charged: some Lightning talents spend it for an extra effect."},
        {"Stun", {"stun", "stuns", "stunned"}, sf::Color(232, 196, 112),
         "Loses its actions until it ends, then can't be stunned again for a moment. Bosses resist repeated stuns."},
        {"Guard", {"guard", "guarded"}, sf::Color(214, 178, 110), "Blocks that much damage from each direct hit. Damage over time gets through."},
        {"Shaken", {"shaken", "shakes"}, sf::Color(200, 170, 120), "Deals less damage."},
        {"Steadfast", {"steadfast"}, sf::Color(214, 178, 110), "Can't be moved or stunned, and direct hits deal less."},
        {"Slowed", {"slowed", "slows"}, sf::Color(150, 210, 250), "Acts less often."},
        {"Hasted", {"hasted", "hastened", "haste"}, sf::Color(130, 214, 150), "Acts more often."},
        {"Marked", {"mark", "marks", "marked"}, sf::Color(240, 136, 52), "The next direct hit on it deals 25% more, then the mark is spent."},
        {"Opening", {"opening"}, sf::Color(130, 214, 150), "A brief chance after waiting or moving with a talent; some bow and armour talents reward it."},
        {"Concealed", {"concealed", "concealment", "conceal"}, sf::Color(120, 150, 120),
         "Hidden: each foe that could see you rolls to spot you, using distance and Dexterity. Most attacks and taking damage reveal you."},
        {"Blinded", {"blind", "blinds", "blinded"}, sf::Color(130, 100, 170), "Sees only what is beside it, and loses track of anything further."},
        {"Pinned", {"pin", "pins", "pinned"}, sf::Color(170, 196, 90), "Can't move, though it can still fight."},
        {"Sundered", {"sunder", "sunders", "sundered"}, sf::Color(196, 200, 214), "Every hit it takes deals more."},
        {"Doom", {"doom"}, sf::Color(160, 170, 150), "A countdown: when it runs out, it deals its damage at once. Guard and dodge don't stop it."},
        {"Wither", {"wither", "withered", "withers"}, sf::Color(160, 170, 150), "Hits on it heal whoever strikes it."},
        {"Plague", {"plague"}, sf::Color(140, 170, 70), "Damage each turn; when it dies, the plague spreads to everything beside it."},
        {"Grappled", {"grapple", "grapples", "grappled"}, sf::Color(130, 214, 150), "Held: it can't walk away, and is dragged along when its captor steps."},
    };
    return all;
}

// The keyword a word counts as, ignoring case and the punctuation around it.
inline const Keyword* keywordFor(std::string word) {
    while (!word.empty() && !std::isalpha(static_cast<unsigned char>(word.back()))) word.pop_back();
    std::size_t start = 0;
    while (start < word.size() && !std::isalpha(static_cast<unsigned char>(word[start]))) ++start;
    word = word.substr(start);
    std::transform(word.begin(), word.end(), word.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    for (const auto& k : keywords())
        for (const char* form : k.forms) if (word == form) return &k;
    return nullptr;
}

// Every keyword a text names, each once, in the order they first appear.
inline std::vector<const Keyword*> keywordsIn(const std::string& text) {
    std::vector<const Keyword*> found;
    std::size_t i = 0;
    while (i < text.size()) {
        const auto end = text.find(' ', i);
        const auto word = text.substr(i, end == std::string::npos ? std::string::npos : end - i);
        if (const auto* k = keywordFor(word); k && std::find(found.begin(), found.end(), k) == found.end()) found.push_back(k);
        if (end == std::string::npos) break;
        i = end + 1;
    }
    return found;
}

// A tooltip's closing lines: what each keyword it names means.
inline void appendKeywordLines(std::vector<ui::Line>& lines, const std::string& text) {
    const auto found = keywordsIn(text);
    if (found.empty()) return;
    lines.push_back({""});
    for (const auto* k : found) {
        lines.push_back({k->name, k->color, 13, ui::Font::Bold});
        lines.push_back({k->text, ui::kMuted, 13});
    }
}
inline std::optional<sf::Color> keywordColour(const std::string& word) {
    if (const auto* k = keywordFor(word)) return k->color;
    return std::nullopt;
}

} // namespace engine
