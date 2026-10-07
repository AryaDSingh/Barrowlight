#pragma once
namespace engine {
inline constexpr int kRunMaxLevel=20;
inline constexpr int kRunFinalFloor=20;
// The Drowned Cathedral: a side dungeon of six floors, opened by slaying the
// Goblin Warlord. Its floors are numbered 21-26 so every floor has one id,
// but they are as dangerous as floors 7-12 (floorDepth). Its last floor
// holds the Sleeper Below; its stairs lead back to town.
inline constexpr int kCathedralFirst=21, kCathedralLast=26;
inline bool cathedralFloor(int floor) { return floor>=kCathedralFirst && floor<=kCathedralLast; }
// The Ashen Foundry: a side dungeon of five floors, opened by the Foreman's
// key that Grik the Packleader carries. As dangerous as Ruins 6-10; the
// Forgemaster waits on its last floor, and its stairs lead back to town.
inline constexpr int kFoundryFirst=27, kFoundryLast=31;
inline bool foundryFloor(int floor) { return floor>=kFoundryFirst && floor<=kFoundryLast; }
// Thornwood Hollow: a side dungeon of five floors, opened by the map the
// Ossuary Warden carries. As dangerous as Crypt floors 14-18; the Hollow
// Mother waits on its last floor.
inline constexpr int kThornFirst=32, kThornLast=36, kMaxFloorId=36;
inline bool thornFloor(int floor) { return floor>=kThornFirst && floor<=kThornLast; }
inline bool sideDungeonFloor(int floor) { return cathedralFloor(floor) || foundryFloor(floor) || thornFloor(floor); }
// How deep a floor is for difficulty, loot and rewards.
inline int floorDepth(int floor) { return cathedralFloor(floor) ? floor-kCathedralFirst+7 : foundryFloor(floor) ? floor-kFoundryFirst+6 : thornFloor(floor) ? floor-kThornFirst+14 : floor; }
// Two pools, like ToME's class and generic points: ability points (four at
// level 1, then one a level) for the class trees, and utility points (two
// at level 1, then one a level) for the utility trees.
inline int earnedAbilityPoints(int level) { return level+3; }
inline int earnedUtilityPoints(int level) { return level+1; }
// Tree points open class trees: one to start, then one at levels 5, 10 and 15.
// Utility trees open freely, as many as you have tree points earned in all.
inline int earnedTreePoints(int level) { return 1+(level>=5)+(level>=10)+(level>=15); }
inline bool grantsTreePoint(int level) { return level==5 || level==10 || level==15; }
inline int utilityTreeSlots(int level) { return earnedTreePoints(level); }
}
