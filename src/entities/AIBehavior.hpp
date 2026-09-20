#pragma once

#include <optional>

#include "core/Position.hpp"

namespace engine {

class Actor;
class Map;

// Strategy-pattern interface for monster decision-making. A Player holds
// no AIBehavior (nullptr, see Player.hpp); a Monster is always given a
// concrete strategy object -- what makes 8 enemy types feel distinct is
// which Stats and which AIBehavior get plugged into an otherwise identical
// Monster, not 8 separate classes (see ARCHITECTURE_DECISIONS.md).
//
// Now a true abstract base (Prompt 7): decideMove() is pure virtual, so
// AIBehavior itself can no longer be instantiated directly. See
// NullAIBehavior for a trivial concrete "never moves" stand-in used by
// tests that need a valid AIBehavior but don't care about AI specifics.
//
// `self` and `map` are only forward-declared here, not #included --
// deliberately, to avoid a circular include (Actor.hpp already includes
// this header to declare its ai_ member).
class AIBehavior {
public:
    virtual ~AIBehavior() = default;

    // Decides where `self` wants to move this turn. The parameter is
    // named `targetPosition`, not "playerPosition" -- nothing here
    // hard-codes the target as being specifically the player, so future
    // behaviors (monster-vs-monster targeting, guarding a fixed point)
    // aren't fighting the interface's naming.
    //
    // Returns the tile to move into, or std::nullopt to not move -- e.g.
    // target out of range/sight, or already adjacent (there's no attack
    // action yet to choose instead, so "adjacent" currently just means
    // "stay put").
    virtual std::optional<Position> decideMove(const Actor& self, const Map& map,
                                                 Position targetPosition) = 0;
};

} // namespace engine

