#pragma once

namespace engine {

// Strategy-pattern interface for monster decision-making. A Player holds
// no AIBehavior (nullptr, see Player.hpp); a Monster is always given a
// concrete strategy object -- what makes 8 enemy types feel distinct is
// which Stats and which AIBehavior get plugged into an otherwise identical
// Monster, not 8 separate classes (see ARCHITECTURE_DECISIONS.md).
//
// This is intentionally an empty interface for now (just a virtual
// destructor, so it's polymorphic and safely destructible through a base
// pointer). It has no decideAction()-style pure virtual method yet because
// there's no Action type or Map for one to operate on -- adding one now
// would mean guessing at that design before Prompt 5/7 actually need it.
// Concrete strategies (Chaser, Kiter, ...) arrive in Prompt 7, at which
// point this stops being instantiable directly and becomes a true
// abstract base.
class AIBehavior {
public:
    virtual ~AIBehavior() = default;
};

} // namespace engine
