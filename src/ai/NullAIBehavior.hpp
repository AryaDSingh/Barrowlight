#pragma once

#include "entities/AIBehavior.hpp"

namespace engine {

// An AIBehavior that never decides to do anything (always Wait). A
// legitimate "dormant" or "guard" behavior in its own right, and also a
// convenient placeholder for tests/setup that need a valid concrete
// AIBehavior without caring about AI specifics.
class NullAIBehavior : public AIBehavior {
public:
    AIDecision decideAction(const Actor&, const Map&, Actor&,
                             const std::vector<Actor*>&) override {
        return AIDecision{};
    }
};

} // namespace engine
