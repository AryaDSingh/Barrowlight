#pragma once
namespace engine {
inline constexpr int kRunMaxLevel=20;
inline constexpr int kRunFinalFloor=20;
// The Drowned Cathedral: a side dungeon of six floors, opened by slaying the
// Goblin Warlord. Its floors are numbered 21-26 so every floor has one id,
// but they are as dangerous as floors 7-12 (floorDepth). Its last floor
// holds the Sleeper Below; its stairs lead back to town.
inline constexpr int kCathedralFirst=21, kCathedralLast=26, kMaxFloorId=26;
inline bool cathedralFloor(int floor) { return floor>=kCathedralFirst && floor<=kCathedralLast; }
// How deep a floor is for difficulty, loot and rewards.
inline int floorDepth(int floor) { return cathedralFloor(floor) ? floor-kCathedralFirst+7 : floor; }
// Ability points: four at level 1, one per level after, and one more on every even level.
inline int earnedAbilityPoints(int level) { return level+3+level/2; }
inline bool grantsExtraAbilityPoint(int level) { return level%2==0; }
inline int earnedTreePoints(int level) { return 1+(level>=5?(level-3)/2:0); }
inline bool grantsTreePoint(int level) { return level>=5 && level%2==1; }
}
