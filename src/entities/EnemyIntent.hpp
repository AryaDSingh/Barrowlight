#pragma once

#include <cstdlib>
#include "core/Position.hpp"

namespace engine {
enum class IntentKind { StunStrike, MagicStrike, HeavyStrike, Summon };
// Pointer-free committed attack. Only deliberate player actions spend the
// reaction window; scheduler speed and skipped stunned turns cannot spend it.
struct EnemyIntent {
    Position origin{}, target{};
    int radius = 0; // Manhattan radius: single tile (0), guard cleave (1), blast diamond (2)
    int playerActionsRemaining = 0;
    int attackPower = 0;
    IntentKind kind = IntentKind::StunStrike;

    bool contains(Position p) const {
        return std::abs(p.x-target.x)+std::abs(p.y-target.y)<=radius;
    }
};
} // namespace engine
