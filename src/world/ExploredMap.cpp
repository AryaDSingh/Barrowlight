#include "world/ExploredMap.hpp"

namespace engine {

ExploredMap::ExploredMap(const Map& map)
    : width_(map.width()),
      height_(map.height()),
      visibility_(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_),
                   Visibility::Hidden) {}

void ExploredMap::update(const std::vector<Position>& visible) {
    // Demote last turn's Visible tiles to Remembered *before* marking the
    // new visible set -- otherwise a tile that's visible on both turns
    // would flicker through Remembered instead of staying Visible.
    for (Visibility& v : visibility_) {
        if (v == Visibility::Visible) {
            v = Visibility::Remembered;
        }
    }

    for (const Position& pos : visible) {
        if (pos.x < 0 || pos.y < 0 || pos.x >= width_ || pos.y >= height_) {
            continue;
        }
        visibility_[static_cast<std::size_t>(pos.y) * static_cast<std::size_t>(width_) +
                     static_cast<std::size_t>(pos.x)] = Visibility::Visible;
    }
}

void ExploredMap::restoreAll(std::vector<Visibility> values) {
    visibility_ = std::move(values);
}

Visibility ExploredMap::at(int x, int y) const {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) {
        return Visibility::Hidden;
    }
    return visibility_[static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
                        static_cast<std::size_t>(x)];
}

} // namespace engine
