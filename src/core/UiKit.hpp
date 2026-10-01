#pragma once

#include <SFML/Graphics.hpp>

#include <map>
#include <unordered_map>
#include <string>
#include <vector>

namespace engine::ui {

// The game's visual language, in one place: dark carved-stone panels with
// bronze bevels (after Diablo II's inventory), recessed item/icon slots,
// ToME-style gem-cornered dialogs, framed resource bars and word-wrapped
// tooltips. Everything here is drawing only -- callers own layout and
// input, so hit-testing stays next to the code that reacts to clicks.

enum class Font { Body, Bold, Title };

// Palette shared by every screen.
inline const sf::Color kText(222, 212, 192);       // parchment
inline const sf::Color kMuted(150, 140, 122);
inline const sf::Color kGold(232, 196, 112);       // headings, names
inline const sf::Color kBronze(140, 108, 62);      // frames
inline const sf::Color kGood(130, 214, 120);
inline const sf::Color kBad(232, 104, 88);
inline const sf::Color kMagic(120, 160, 255);
inline const sf::Color kRare(255, 214, 96);
inline const sf::Color kInfo(132, 206, 222);       // cyan accents (ToME's info blue)

// One line of a tooltip or text block; long lines wrap to the box width.
struct Line {
    std::string text;
    sf::Color color = kText;
    unsigned size = 15;
    Font font = Font::Body;
};

class Kit {
public:
    Kit();

    const sf::Font& font(Font f) const;

    // Text with a 1px drop shadow, the way ToME keeps text legible over
    // busy art. Returns the drawn width.
    float text(sf::RenderTarget& target, const std::string& str, sf::Vector2f position, unsigned size,
               sf::Color color, Font f = Font::Body, bool shadow = true) const;
    float textWidth(const std::string& str, unsigned size, Font f = Font::Body) const;
    void textCentered(sf::RenderTarget& target, const std::string& str, sf::FloatRect box, unsigned size,
                      sf::Color color, Font f = Font::Body) const;
    std::vector<std::string> wrap(const std::string& str, float width, unsigned size, Font f = Font::Body) const;
    // Draws wrapped text, advancing y; lines past `bottom` are dropped.
    void paragraph(sf::RenderTarget& target, const std::string& str, float x, float& y, float width, unsigned size,
                   sf::Color color, Font f = Font::Body, float bottom = 100000.f) const;

    // Tiled dark stone fill. `shade` darkens/tints it (White = as generated).
    void stone(sf::RenderTarget& target, sf::FloatRect rect, sf::Color shade = sf::Color::White) const;
    // Bronze bevel around a rect; `ornate` adds ToME's gem corners.
    void frame(sf::RenderTarget& target, sf::FloatRect rect, bool ornate = false) const;
    void panel(sf::RenderTarget& target, sf::FloatRect rect, bool ornate = false,
               sf::Color shade = sf::Color::White) const;
    // A recessed slot: near-black well, shadowed top-left, lit bottom-right.
    // `glow` colors the rim (selection, drop target, rarity); Transparent = plain.
    void inset(sf::RenderTarget& target, sf::FloatRect rect, sf::Color glow = sf::Color::Transparent) const;
    void button(sf::RenderTarget& target, sf::FloatRect rect, const std::string& label, bool hover,
                bool enabled = true, unsigned size = 15) const;
    // A framed bar. `label` is centered on it.
    void bar(sf::RenderTarget& target, sf::FloatRect rect, float fraction, sf::Color fill,
             const std::string& label = "", unsigned size = 13) const;
    // Ornate vertical rod with knobs, used between columns.
    void divider(sf::RenderTarget& target, float x, float top, float bottom) const;
    void heading(sf::RenderTarget& target, const std::string& str, sf::Vector2f position, unsigned size = 22) const;

    // An icon from assets/icons/<name>.png (white silhouettes) tinted with
    // `tint`, fitted into `rect`. Returns false if the icon is missing.
    bool icon(sf::RenderTarget& target, const std::string& name, sf::FloatRect rect,
              sf::Color tint = kText) const;

    // Measures, places and draws a framed tooltip next to `anchor` (to its
    // right, flipping left/up to stay inside `bounds`). Returns its rect.
    sf::FloatRect tooltip(sf::RenderTarget& target, const std::vector<Line>& lines, sf::Vector2f anchor,
                          float width = 330.f, sf::FloatRect bounds = {{0, 0}, {1280, 720}}) const;

private:
    // SFML shapes text (HarfBuzz) whenever an sf::Text is built, which costs
    // hundreds of microseconds. Every distinct string is shaped once and the
    // sf::Text reused; only its position and colour change per draw. Both
    // caches are simply cleared when they grow large (log lines, changing
    // numbers), which is cheap compared with reshaping every frame.
    struct ShapedText { sf::Text text; float width; };
    ShapedText& shaped(const std::string& str, unsigned size, Font f) const;
    mutable std::unordered_map<std::string, ShapedText> textCache_;
    mutable std::unordered_map<std::string, std::vector<std::string>> wrapCache_;

    sf::Font body_, bold_, title_;
    sf::Texture stone_;
    mutable std::map<std::string, sf::Texture> icons_;
    mutable std::map<std::string, bool> missingIcons_;
};

} // namespace engine::ui
