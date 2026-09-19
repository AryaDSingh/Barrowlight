#pragma once

#include <string>
#include <vector>

#include "core/Position.hpp"
#include "world/Tile.hpp"

namespace engine {

// A fixed-size grid of Tiles. Deliberately knows nothing about SFML,
// entities, or FOV -- see ARCHITECTURE_DECISIONS.md. A procedurally
// generated level (Prompt 8) will build one of these the same way this
// prompt's hardcoded test level does: by filling in tiles, nothing more.
class Map {
public:
    // Default-constructs an empty 0x0 map. Exists so Map can be a plain
    // member (e.g. on Application) and assigned its real content once
    // that content is ready, without contorting constructor-initializer
    // ordering to make everything happen in one expression.
    Map() = default;

    Map(int width, int height);

    int width() const { return width_; }
    int height() const { return height_; }

    bool inBounds(int x, int y) const;

    const Tile& tileAt(int x, int y) const;
    void setTile(int x, int y, Tile tile);

    // False for out-of-bounds coordinates too, so callers (movement code)
    // don't need a separate inBounds() check before every walk attempt.
    bool isWalkable(int x, int y) const;

private:
    int width_ = 0;
    int height_ = 0;
    std::vector<Tile> tiles_; // row-major: index = y * width_ + x
};

// Parses '#' as a wall, '.' as floor, and '@' as a floor tile whose
// position gets written to playerStart (throws if no '@' is found, or if
// rows aren't all the same length). This exists so a test level -- or any
// other ASCII-art level later -- is defined as readable text, not a long
// list of individual setTile() calls.
Map parseAsciiMap(const std::vector<std::string>& rows, Position& playerStart);

} // namespace engine
