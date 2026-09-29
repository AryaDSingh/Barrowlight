#pragma once
namespace engine {
inline constexpr int kRunMaxLevel=20;
inline constexpr int kRunFinalFloor=20;
inline int earnedTreePoints(int level) { return 1+(level>=5?(level-3)/2:0); }
inline bool grantsTreePoint(int level) { return level>=5 && level%2==1; }
}
