#include "core/UiKit.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <random>
#include <sstream>
#include <tuple>

namespace engine::ui {

namespace {

constexpr const char* kTitleFontPath = "assets/fonts/Cinzel.ttf";
constexpr const char* kBodyFontPath = "assets/fonts/AlegreyaSans-Regular.ttf";
constexpr const char* kBoldFontPath = "assets/fonts/AlegreyaSans-Bold.ttf";
constexpr const char* kFallbackFontPath = "assets/fonts/DejaVuSansMono.ttf";
constexpr const char* kIconDir = "assets/icons/";

void loadFont(sf::Font& font, const char* path) {
    if (font.openFromFile(path)) return;
    std::cout << "Warning: failed to load font " << path << " -- using the fallback font.\n";
    if (!font.openFromFile(kFallbackFontPath))
        std::cout << "Warning: fallback font missing too -- text will not render.\n";
}

// Tileable value noise for the stone: a few octaves of a wrapped random
// lattice, plus sparse pits. Generated once, so it costs nothing per frame
// and needs no image asset.
sf::Image makeStone() {
    constexpr unsigned kSize = 128;
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> unit(0.f, 1.f);
    std::vector<float> value(kSize * kSize, 0.f);
    float amplitude = 1.f, total = 0.f;
    for (unsigned cells : {4u, 8u, 16u, 32u}) {
        std::vector<float> lattice(cells * cells);
        for (auto& v : lattice) v = unit(rng);
        const auto at = [&](unsigned x, unsigned y) { return lattice[(y % cells) * cells + (x % cells)]; };
        for (unsigned y = 0; y < kSize; ++y)
            for (unsigned x = 0; x < kSize; ++x) {
                const float fx = static_cast<float>(x) * cells / kSize, fy = static_cast<float>(y) * cells / kSize;
                const auto ix = static_cast<unsigned>(fx), iy = static_cast<unsigned>(fy);
                float tx = fx - ix, ty = fy - iy;
                tx = tx * tx * (3 - 2 * tx);
                ty = ty * ty * (3 - 2 * ty);
                const float top = at(ix, iy) * (1 - tx) + at(ix + 1, iy) * tx;
                const float bottom = at(ix, iy + 1) * (1 - tx) + at(ix + 1, iy + 1) * tx;
                value[y * kSize + x] += (top * (1 - ty) + bottom * ty) * amplitude;
            }
        total += amplitude;
        amplitude *= 0.55f;
    }
    sf::Image image(sf::Vector2u{kSize, kSize}, sf::Color::Black);
    for (unsigned y = 0; y < kSize; ++y)
        for (unsigned x = 0; x < kSize; ++x) {
            float v = value[y * kSize + x] / total;          // 0..1, mostly 0.3..0.7
            if (unit(rng) < 0.012f) v -= 0.25f;               // pits
            const float base = 28.f + v * 38.f;
            const auto c = [](float f) { return static_cast<std::uint8_t>(std::clamp(f, 0.f, 255.f)); };
            image.setPixel({x, y}, sf::Color(c(base + 3), c(base), c(base - 2)));
        }
    return image;
}

// Panels and slots are built from many thin coloured strips. Each strip is
// appended to a pending vertex batch instead of being drawn on its own, and
// the batch goes out in a single draw call: before anything that isn't a
// plain quad (text, textures, circles) and at the end of every Kit method.
// Debug builds in particular pay heavily per draw call.
std::vector<sf::Vertex> gQuads;
sf::RenderTarget* gQuadTarget = nullptr;

void flushQuads(sf::RenderTarget& target) {
    if (!gQuads.empty() && gQuadTarget) gQuadTarget->draw(gQuads.data(), gQuads.size(), sf::PrimitiveType::Triangles);
    gQuads.clear();
    gQuadTarget = &target;
}

void rect(sf::RenderTarget& target, sf::FloatRect r, sf::Color fill) {
    if (gQuadTarget != &target) flushQuads(target);
    const float l = r.position.x, t = r.position.y, rr = l + r.size.x, b = t + r.size.y;
    for (const sf::Vector2f v : {sf::Vector2f{l, t}, {rr, t}, {rr, b}, {l, t}, {rr, b}, {l, b}})
        gQuads.push_back(sf::Vertex{v, fill});
}

void outline(sf::RenderTarget& target, sf::FloatRect r, sf::Color color, float thickness) {
    const float l = r.position.x, t = r.position.y, w = r.size.x, h = r.size.y;
    rect(target, {{l, t}, {w, thickness}}, color);
    rect(target, {{l, t + h - thickness}, {w, thickness}}, color);
    rect(target, {{l, t + thickness}, {thickness, h - 2 * thickness}}, color);
    rect(target, {{l + w - thickness, t + thickness}, {thickness, h - 2 * thickness}}, color);
}

sf::FloatRect shrink(sf::FloatRect r, float by) {
    return {{r.position.x + by, r.position.y + by}, {r.size.x - 2 * by, r.size.y - 2 * by}};
}

void gem(sf::RenderTarget& target, sf::Vector2f center) {
    flushQuads(target);
    sf::CircleShape ring(8.f);
    ring.setOrigin({8.f, 8.f});
    ring.setPosition(center);
    ring.setFillColor(sf::Color(52, 38, 22));
    ring.setOutlineThickness(2.f);
    ring.setOutlineColor(sf::Color(196, 156, 82));
    target.draw(ring);
    sf::CircleShape stone(4.f);
    stone.setOrigin({4.f, 4.f});
    stone.setPosition(center);
    stone.setFillColor(sf::Color(150, 22, 26));
    target.draw(stone);
    sf::CircleShape glint(1.5f);
    glint.setOrigin({1.5f, 1.5f});
    glint.setPosition({center.x - 1.5f, center.y - 1.5f});
    glint.setFillColor(sf::Color(255, 150, 140));
    target.draw(glint);
}

} // namespace

Kit::Kit() {
    loadFont(title_, kTitleFontPath);
    loadFont(body_, kBodyFontPath);
    loadFont(bold_, kBoldFontPath);
    if (!stone_.loadFromImage(makeStone())) std::cout << "Warning: could not build the stone texture.\n";
    stone_.setRepeated(true);
}

const sf::Font& Kit::font(Font f) const {
    return f == Font::Title ? title_ : f == Font::Bold ? bold_ : body_;
}

Kit::ShapedText& Kit::shaped(const std::string& str, unsigned size, Font f) const {
    std::string key;
    key.reserve(str.size() + 8);
    key += static_cast<char>('0' + static_cast<int>(f));
    key += std::to_string(size);
    key += '|';
    key += str;
    if (const auto it = textCache_.find(key); it != textCache_.end()) return it->second;
    if (textCache_.size() > 4000) textCache_.clear();
    sf::Text t(font(f), sf::String::fromUtf8(str.begin(), str.end()), size);
    const float width = t.findCharacterPos(t.getString().getSize()).x;
    return textCache_.emplace(std::move(key), ShapedText{std::move(t), width}).first->second;
}

float Kit::text(sf::RenderTarget& target, const std::string& str, sf::Vector2f position, unsigned size,
                sf::Color color, Font f, bool shadow) const {
    flushQuads(target);
    if (str.empty()) return 0.f;
    auto& entry = shaped(str, size, f);
    sf::Text& t = entry.text;
    const sf::Vector2f p{std::round(position.x), std::round(position.y)};
    if (shadow) {
        t.setFillColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(color.a * 0.85f)));
        t.setPosition({p.x + 1.f, p.y + 1.f});
        target.draw(t);
    }
    t.setFillColor(color);
    t.setPosition(p);
    target.draw(t);
    return entry.width;
}

float Kit::textWidth(const std::string& str, unsigned size, Font f) const {
    if (str.empty()) return 0.f;
    return shaped(str, size, f).width;
}

void Kit::textCentered(sf::RenderTarget& target, const std::string& str, sf::FloatRect box, unsigned size,
                       sf::Color color, Font f) const {
    const float w = textWidth(str, size, f);
    const float lineHeight = font(f).getLineSpacing(size);
    text(target, str, {box.position.x + (box.size.x - w) / 2.f, box.position.y + (box.size.y - lineHeight) / 2.f},
         size, color, f);
}

std::vector<std::string> Kit::wrap(const std::string& str, float width, unsigned size, Font f) const {
    std::string key = std::to_string(static_cast<int>(width)) + ':' + std::to_string(size) + ':' +
                      std::to_string(static_cast<int>(f)) + '|' + str;
    if (const auto it = wrapCache_.find(key); it != wrapCache_.end()) return it->second;
    if (wrapCache_.size() > 1000) wrapCache_.clear();
    std::vector<std::string> lines;
    std::istringstream paragraphs(str);
    std::string paragraph;
    while (std::getline(paragraphs, paragraph)) {
        std::istringstream words(paragraph);
        std::string word, line;
        while (words >> word) {
            const std::string candidate = line.empty() ? word : line + ' ' + word;
            if (!line.empty() && textWidth(candidate, size, f) > width) {
                lines.push_back(line);
                line = word;
            } else {
                line = candidate;
            }
        }
        lines.push_back(line);
    }
    wrapCache_.emplace(std::move(key), lines);
    return lines;
}

void Kit::paragraph(sf::RenderTarget& target, const std::string& str, float x, float& y, float width, unsigned size,
                    sf::Color color, Font f, float bottom) const {
    const float lineHeight = std::round(font(f).getLineSpacing(size));
    for (const auto& line : wrap(str, width, size, f)) {
        if (y + lineHeight <= bottom) text(target, line, {x, y}, size, color, f);
        y += lineHeight;
    }
}

void Kit::stone(sf::RenderTarget& target, sf::FloatRect r, sf::Color shade) const {
    flushQuads(target);
    sf::RectangleShape shape(r.size);
    shape.setPosition(r.position);
    shape.setTexture(&stone_);
    // Anchor the texture to screen space so neighboring panels line up.
    shape.setTextureRect(sf::IntRect({static_cast<int>(r.position.x), static_cast<int>(r.position.y)},
                                     {static_cast<int>(r.size.x), static_cast<int>(r.size.y)}));
    shape.setFillColor(shade);
    target.draw(shape);
}

void Kit::frame(sf::RenderTarget& target, sf::FloatRect r, bool ornate) const {
    outline(target, r, sf::Color(8, 6, 4), 1.f);
    outline(target, shrink(r, 1.f), kBronze, 2.f);
    // Light catches the top-left of the bevel, shadow the bottom-right.
    rect(target, {{r.position.x + 1, r.position.y + 1}, {r.size.x - 2, 1}}, sf::Color(214, 178, 112));
    rect(target, {{r.position.x + 1, r.position.y + 1}, {1, r.size.y - 2}}, sf::Color(214, 178, 112));
    rect(target, {{r.position.x + 1, r.position.y + r.size.y - 2}, {r.size.x - 2, 1}}, sf::Color(70, 50, 26));
    rect(target, {{r.position.x + r.size.x - 2, r.position.y + 1}, {1, r.size.y - 2}}, sf::Color(70, 50, 26));
    outline(target, shrink(r, 3.f), sf::Color(10, 8, 6), 1.f);
    if (ornate) {
        const float l = r.position.x + 2, t = r.position.y + 2;
        const float rr = r.position.x + r.size.x - 2, b = r.position.y + r.size.y - 2;
        for (const sf::Vector2f c : {sf::Vector2f{l, t}, {rr, t}, {l, b}, {rr, b}}) gem(target, c);
    }
    flushQuads(target);
}

void Kit::panel(sf::RenderTarget& target, sf::FloatRect r, bool ornate, sf::Color shade) const {
    stone(target, r, shade);
    frame(target, r, ornate);
    flushQuads(target);
}

void Kit::glass(sf::RenderTarget& target, sf::FloatRect r, bool ornate, std::uint8_t alpha) const {
    rect(target, r, sf::Color(9, 8, 11, alpha));
    outline(target, r, sf::Color(140, 108, 62, 200), 1.f);
    // A faint inner line keeps the edge crisp over bright ground.
    outline(target, {{r.position.x + 3, r.position.y + 3}, {r.size.x - 6, r.size.y - 6}}, sf::Color(255, 230, 180, 18), 1.f);
    flushQuads(target);
    if (ornate) frame(target, r, true);
}

void Kit::inset(sf::RenderTarget& target, sf::FloatRect r, sf::Color glow) const {
    rect(target, r, sf::Color(12, 11, 12));
    // Raised rim: light top-left edge of the surrounding stone...
    outline(target, r, sf::Color(92, 86, 78), 1.f);
    // ...then a deep shadow inside the top and left edges.
    rect(target, {{r.position.x + 1, r.position.y + 1}, {r.size.x - 2, 3}}, sf::Color(0, 0, 0, 200));
    rect(target, {{r.position.x + 1, r.position.y + 1}, {3, r.size.y - 2}}, sf::Color(0, 0, 0, 200));
    rect(target, {{r.position.x + 1, r.position.y + r.size.y - 2}, {r.size.x - 2, 1}}, sf::Color(58, 54, 50));
    rect(target, {{r.position.x + r.size.x - 2, r.position.y + 1}, {1, r.size.y - 2}}, sf::Color(58, 54, 50));
    if (glow.a) {
        outline(target, r, glow, 2.f);
    }
    flushQuads(target);
}

void Kit::button(sf::RenderTarget& target, sf::FloatRect r, const std::string& label, bool hover, bool enabled,
                 unsigned size) const {
    const sf::Color face = !enabled ? sf::Color(46, 42, 38) : hover ? sf::Color(126, 98, 62) : sf::Color(96, 74, 48);
    rect(target, r, face);
    rect(target, {r.position, {r.size.x, r.size.y / 2}}, sf::Color(255, 240, 210, enabled ? 18 : 6)); // sheen
    outline(target, r, sf::Color(10, 8, 6), 1.f);
    rect(target, {{r.position.x + 1, r.position.y + 1}, {r.size.x - 2, 1}},
         enabled ? sf::Color(220, 186, 126) : sf::Color(80, 74, 66));
    rect(target, {{r.position.x + 1, r.position.y + r.size.y - 2}, {r.size.x - 2, 1}}, sf::Color(40, 28, 14));
    textCentered(target, label, r, size, enabled ? sf::Color(250, 238, 214) : kMuted, Font::Bold);
    flushQuads(target);
}

void Kit::bar(sf::RenderTarget& target, sf::FloatRect r, float fraction, sf::Color fill, const std::string& label,
              unsigned size) const {
    fraction = std::clamp(fraction, 0.f, 1.f);
    rect(target, r, sf::Color(10, 9, 9));
    const sf::FloatRect inner = shrink(r, 2.f);
    rect(target, inner, sf::Color(fill.r / 5, fill.g / 5, fill.b / 5));
    const sf::FloatRect filled{inner.position, {inner.size.x * fraction, inner.size.y}};
    rect(target, filled, fill);
    rect(target, {filled.position, {filled.size.x, std::max(1.f, inner.size.y / 3)}}, sf::Color(255, 255, 255, 45));
    rect(target, {{filled.position.x, filled.position.y + inner.size.y - 2}, {filled.size.x, 2}}, sf::Color(0, 0, 0, 70));
    outline(target, r, sf::Color(96, 76, 46), 1.f);
    if (!label.empty()) textCentered(target, label, r, size, sf::Color(250, 244, 232), Font::Bold);
    flushQuads(target);
}

void Kit::divider(sf::RenderTarget& target, float x, float top, float bottom) const {
    rect(target, {{x - 2, top}, {4, bottom - top}}, sf::Color(70, 52, 30));
    rect(target, {{x - 1, top}, {1, bottom - top}}, sf::Color(196, 156, 82));
    flushQuads(target);
    for (const float y : {top, bottom}) {
        sf::CircleShape knob(4.f);
        knob.setOrigin({4.f, 4.f});
        knob.setPosition({x, y});
        knob.setFillColor(sf::Color(150, 22, 26));
        knob.setOutlineThickness(1.5f);
        knob.setOutlineColor(sf::Color(196, 156, 82));
        target.draw(knob);
    }
    flushQuads(target);
}

void Kit::heading(sf::RenderTarget& target, const std::string& str, sf::Vector2f position, unsigned size) const {
    const float w = text(target, str, position, size, kGold, Font::Title);
    const float y = position.y + font(Font::Title).getLineSpacing(size) + 2;
    rect(target, {{position.x, y}, {w, 1}}, sf::Color(140, 108, 62, 180));
    flushQuads(target);
}

bool Kit::icon(sf::RenderTarget& target, const std::string& name, sf::FloatRect r, sf::Color tint) const {
    flushQuads(target);
    if (name.empty() || missingIcons_.count(name)) return false;
    auto it = icons_.find(name);
    if (it == icons_.end()) {
        sf::Texture texture;
        if (!texture.loadFromFile(std::string(kIconDir) + name + ".png")) {
            std::cout << "Warning: missing icon " << name << ".png\n";
            missingIcons_[name] = true;
            return false;
        }
        // 512px silhouettes drawn at ~24-48px: mipmaps keep them crisp.
        texture.setSmooth(true);
        std::ignore = texture.generateMipmap();
        it = icons_.emplace(name, std::move(texture)).first;
    }
    sf::Sprite sprite(it->second);
    const auto size = it->second.getSize();
    const float scale = std::min(r.size.x / size.x, r.size.y / size.y);
    sprite.setScale({scale, scale});
    sprite.setPosition({r.position.x + (r.size.x - size.x * scale) / 2, r.position.y + (r.size.y - size.y * scale) / 2});
    // A dark copy offset by a pixel gives the silhouette some depth.
    sprite.setColor(sf::Color(0, 0, 0, static_cast<std::uint8_t>(tint.a * 0.7f)));
    sprite.move({1.f, 1.f});
    target.draw(sprite);
    sprite.move({-1.f, -1.f});
    sprite.setColor(tint);
    target.draw(sprite);
    return true;
}

sf::FloatRect Kit::tooltip(sf::RenderTarget& target, const std::vector<Line>& lines, sf::Vector2f anchor, float width,
                           sf::FloatRect bounds) const {
    if (holding_) { held_ = HeldTooltip{lines, anchor, width, bounds}; return {anchor, {width, 0.f}}; }
    constexpr float kPad = 12.f;
    struct Laid { std::string text; const Line* line; float height; };
    std::vector<Laid> laid;
    float height = 0.f;
    for (const auto& line : lines) {
        const float lineHeight = std::round(font(line.font).getLineSpacing(line.size));
        if (line.text.empty()) { height += lineHeight / 2; laid.push_back({"", &line, lineHeight / 2}); continue; }
        for (const auto& wrapped : wrap(line.text, width - 2 * kPad, line.size, line.font)) {
            laid.push_back({wrapped, &line, lineHeight});
            height += lineHeight;
        }
    }
    const sf::Vector2f size{width, height + 2 * kPad};
    sf::Vector2f pos{anchor.x + 18.f, anchor.y + 10.f};
    const float right = bounds.position.x + bounds.size.x, bottom = bounds.position.y + bounds.size.y;
    if (pos.x + size.x > right) pos.x = anchor.x - size.x - 12.f;
    if (pos.y + size.y > bottom) pos.y = bottom - size.y;
    pos.x = std::max(pos.x, bounds.position.x);
    pos.y = std::max(pos.y, bounds.position.y);
    const sf::FloatRect box{pos, size};
    rect(target, {{pos.x + 4, pos.y + 4}, size}, sf::Color(0, 0, 0, 110)); // drop shadow
    stone(target, box, sf::Color(150, 150, 165));
    rect(target, box, sf::Color(8, 8, 14, 150));
    frame(target, box);
    float y = pos.y + kPad;
    for (const auto& l : laid) {
        if (!l.text.empty()) text(target, l.text, {pos.x + kPad, y}, l.line->size, l.line->color, l.line->font);
        y += l.height;
    }
    flushQuads(target);
    return box;
}

} // namespace engine::ui
