#pragma once

#include <SFML/Graphics.hpp>

#include <map>
#include <unordered_map>
#include <string>

namespace engine {

// One frame cut out of a sprite sheet under assets/sprites/. `sheet` is
// relative to that folder; `rect` is in the sheet's own pixels.
struct SpriteFrame {
    const char* sheet;
    sf::IntRect rect;
};

// Owns every sprite-sheet texture, keyed by sheet path. Lives in core/
// alongside SoundManager for the same reason: only Application touches
// sf:: types, so entities/world stay SFML-free and their tests keep
// building without it.
//
// A sheet that fails to load is remembered as missing and draw() returns
// false for it -- the caller falls back to the old flat-colored square,
// so a missing PNG degrades the visuals instead of crashing the game
// (same policy as the font and SoundManager's sounds).
class SpriteAtlas {
public:
    // Draws `frame` scaled to fit a `size` x `size` cell at `topLeft`,
    // bottom-aligned and horizontally centered so taller-than-wide frames
    // (doors, 48px minotaurs) still stand on the tile's floor line.
    // `tint` multiplies the sprite's colors (White = unchanged).
    // `flip` mirrors it left-to-right (sheets face right).
    bool draw(sf::RenderTarget& target, const SpriteFrame& frame, sf::Vector2f topLeft, float size,
              sf::Color tint = sf::Color::White, bool flip = false);

    // Batching: the sheet's texture (nullptr if missing), and the quad
    // draw() would emit for a frame, appended to a triangle list instead.
    // Many sprites from one sheet then go out in a single draw call.
    const sf::Texture* texture(const char* sheet);
    static void append(sf::VertexArray& triangles, const SpriteFrame& frame, sf::Vector2f topLeft, float size,
                       sf::Color tint = sf::Color::White);

private:

    std::map<std::string, sf::Texture> textures_;
    // Sheet names are string literals, so their address identifies them:
    // a pointer lookup avoids building a std::string for every tile drawn.
    std::unordered_map<const char*, const sf::Texture*> byPointer_;
    std::map<std::string, bool> missing_;
};

} // namespace engine
