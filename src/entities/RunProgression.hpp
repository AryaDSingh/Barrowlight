#pragma once
namespace engine {
inline constexpr int kRunMaxLevel=20;
inline constexpr int kRunFinalFloor=20;
// Ability points: four at level 1, one per level after, and one more on every even level.
inline int earnedAbilityPoints(int level) { return level+3+level/2; }
inline bool grantsExtraAbilityPoint(int level) { return level%2==0; }
inline int earnedTreePoints(int level) { return 1+(level>=5?(level-3)/2:0); }
inline bool grantsTreePoint(int level) { return level>=5 && level%2==1; }
}
