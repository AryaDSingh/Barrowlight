#pragma once
#include <algorithm>
#include <cstdlib>
#include <vector>
#include "world/Map.hpp"

namespace engine {
inline bool fireTileOpen(const Map& map,Position p) {
    return map.inBounds(p.x,p.y) && map.tileAt(p.x,p.y).transparent;
}
inline bool fireCornerBlocked(const Map& map,Position a,Position b) {
    return a.x!=b.x && a.y!=b.y && (!fireTileOpen(map,{a.x,b.y}) || !fireTileOpen(map,{b.x,a.y}));
}
// Choose one canonical direction before rasterizing, then reverse when needed.
// A->B and B->A visit exactly the same cells, including ambiguous slopes.
inline std::vector<Position> fireLine(Position from,Position to) {
    const bool reverse=from.x>to.x || (from.x==to.x && from.y>to.y);
    if (reverse) std::swap(from,to);
    std::vector<Position> result{from};
    const int dx=std::abs(to.x-from.x),dy=-std::abs(to.y-from.y);
    const int sx=from.x<to.x?1:-1,sy=from.y<to.y?1:-1;
    int error=dx+dy;
    while (from.x!=to.x || from.y!=to.y) {
        const int twice=2*error;
        if (twice>=dy) { error+=dy; from.x+=sx; }
        if (twice<=dx) { error+=dx; from.y+=sy; }
        result.push_back(from);
    }
    if (reverse) std::reverse(result.begin(),result.end());
    return result;
}
inline bool hasLineOfFire(const Map& map,Position from,Position to) {
    if (!fireTileOpen(map,from) || !fireTileOpen(map,to)) return false;
    const auto ray=fireLine(from,to);
    for (std::size_t i=1;i<ray.size();++i)
        if (!fireTileOpen(map,ray[i]) || fireCornerBlocked(map,ray[i-1],ray[i])) return false;
    return true;
}
}
