#pragma once
#include <algorithm>
#include <array>

namespace engine {
using DungeonLevels = std::array<int,2>;
inline int dungeonIndex(int floor) { return floor>20?2:floor>10?1:0; }
inline int dungeonMinimum(int dungeon) { return dungeon?10:1; }
inline int dungeonMaximum(int dungeon) { return dungeon?20:10; }
inline const char* dungeonName(int dungeon) { return dungeon==2?"Drowned Cathedral":dungeon?"Deep Crypts":"The Ruins"; }
// Where each dungeon starts and how many floors it has (the Ruins, Deep Crypts, the Cathedral).
inline int dungeonFirstFloor(int dungeon) { return dungeon==2?21:dungeon?11:1; }
inline int dungeonLength(int dungeon) { return dungeon==2?6:10; }
inline int floorInDungeon(int floor) { return floor-dungeonFirstFloor(dungeonIndex(floor))+1; }
inline bool validDungeonLevels(const DungeonLevels& levels) {
    for(int i=0;i<2;++i) if(levels[i] && (levels[i]<dungeonMinimum(i) || levels[i]>dungeonMaximum(i))) return false;
    return true;
}
// Global depth spans Ruins 1-10 and Deep Crypts 11-20.
inline int dungeonDepthBonus(int floor) { return std::clamp(floor,1,20)-1; }
}
