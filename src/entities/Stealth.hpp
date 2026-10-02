#pragma once

#include <algorithm>
#include <cmath>
#include "core/Position.hpp"

namespace engine {
inline constexpr int kStealthDetectionRadius = 4;

// Pure probability only: UI may inspect this without advancing any RNG.
// Distance is Euclidean; callers must independently gate checks by line of sight.
inline float stealthDetectionChance(int playerDexterity, int enemyDexterity,
                                    int concealmentRank, Position player, Position enemy) {
    const float dx=static_cast<float>(player.x)-enemy.x;
    const float dy=static_cast<float>(player.y)-enemy.y;
    const float distance=std::sqrt(dx*dx+dy*dy);
    if (distance>kStealthDetectionRadius) return 0.f;
    // Ranks 2-3 help a lot; ranks 4-5 a little more.
    const float rankBonus=.12f*(std::clamp(concealmentRank,1,3)-1)+.05f*std::clamp(concealmentRank-3,0,2);
    const float agilityBonus=.015f*std::clamp(playerDexterity,0,16);
    const float perceptionBonus=.008f*std::clamp(enemyDexterity,0,60);
    const float distanceBonus=.22f*std::max(0.f,distance-1.f);
    return std::clamp(.38f-rankBonus-agilityBonus+perceptionBonus-distanceBonus,.02f,.85f);
}
} // namespace engine
