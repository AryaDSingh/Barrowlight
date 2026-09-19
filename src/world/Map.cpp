#include "world/Map.hpp"

#include <stdexcept>

namespace engine {

Map::Map(int width, int height)
    : width_(width),
      height_(height),
      tiles_(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {}

bool Map::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}

const Tile& Map::tileAt(int x, int y) const {
    return tiles_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                   static_cast<std::size_t>(x)];
}

void Map::setTile(int x, int y, Tile tile) {
    tiles_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
           static_cast<std::size_t>(x)] = tile;
}

bool Map::isWalkable(int x, int y) const {
    return inBounds(x, y) && tileAt(x, y).walkable;
}

Map parseAsciiMap(const std::vector<std::string>& rows, Position& playerStart) {
    if (rows.empty()) {
        throw std::invalid_argument("parseAsciiMap: rows must not be empty");
    }

    const int height = static_cast<int>(rows.size());
    const int width = static_cast<int>(rows.front().size());

    Map map(width, height);
    bool foundPlayerStart = false;

    for (int y = 0; y < height; ++y) {
        const std::string& row = rows[static_cast<std::size_t>(y)];
        if (static_cast<int>(row.size()) != width) {
            throw std::invalid_argument(
                "parseAsciiMap: all rows must be the same length");
        }

        for (int x = 0; x < width; ++x) {
            const char c = row[static_cast<std::size_t>(x)];
            switch (c) {
                case '#':
                    map.setTile(x, y, Tile{TileType::Wall, false, false});
                    break;
                case '.':
                    map.setTile(x, y, Tile{TileType::Floor, true, true});
                    break;
                case '@':
                    map.setTile(x, y, Tile{TileType::Floor, true, true});
                    playerStart = Position{x, y};
                    foundPlayerStart = true;
                    break;
                default:
                    throw std::invalid_argument(
                        std::string("parseAsciiMap: unrecognized character '") +
                        c + "'");
            }
        }
    }

    if (!foundPlayerStart) {
        throw std::invalid_argument("parseAsciiMap: no '@' found for player start");
    }

    return map;
}

} // namespace engine
