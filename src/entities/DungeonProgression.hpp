#pragma once
#include <algorithm>
#include <array>

namespace engine {
using DungeonLevels = std::array<int,2>;
inline int dungeonIndex(int floor) { return floor>10?1:0; }
inline int dungeonMinimum(int dungeon) { return dungeon?10:1; }
inline int dungeonMaximum(int dungeon) { return dungeon?20:10; }
inline const char* dungeonName(int dungeon) { return dungeon?"Deep Crypts":"The Ruins"; }
inline bool validDungeonLevels(const DungeonLevels& levels) {
    for(int i=0;i<2;++i) if(levels[i] && (levels[i]<dungeonMinimum(i) || levels[i]>dungeonMaximum(i))) return false;
    return true;
}
// Global depth spans Ruins 1-10 and Deep Crypts 11-20.
inline int dungeonDepthBonus(int floor) { return std::clamp(floor,1,20)-1; }
}
