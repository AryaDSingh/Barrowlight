#pragma once

#include "entities/AIBehavior.hpp"

namespace engine {

// Moves toward its target one A*-computed step at a time, but only when
// the target is within its own line of sight -- reusing
// computeFieldOfView(), the same function that lights the player's view
// (Prompt 6), so a Chaser can't "see" a target through walls. Stateless:
// nothing is stored between turns beyond its own sight radius, everything
// else needed is passed into decideMove() each call.
class Chaser : public AIBehavior {
public:
    explicit Chaser(int sightRadius = kDefaultSightRadius);

    std::optional<Position> decideMove(const Actor& self, const Map& map,
                                         Position targetPosition) override;

private:
    static constexpr int kDefaultSightRadius = 8;
    int sightRadius_;
};

} // namespace engine
