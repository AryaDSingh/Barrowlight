#pragma once

#include "entities/AIBehavior.hpp"

namespace engine {

// An AIBehavior that never decides to move. A legitimate "dormant" or
// "guard" behavior in its own right, and also a convenient placeholder
// for tests/setup that need a valid concrete AIBehavior without caring
// about AI specifics (see entity_smoke_test.cpp, turn_scheduler_test.cpp
// -- both needed *some* concrete AIBehavior once it became a true
// abstract base in this prompt, and neither cares which one).
class NullAIBehavior : public AIBehavior {
public:
    std::optional<Position> decideMove(const Actor&, const Map&, Position) override {
        return std::nullopt;
    }
};

} // namespace engine
