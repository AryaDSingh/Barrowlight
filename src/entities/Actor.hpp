#pragma once

#include <memory>
#include <string>
#include <utility>

#include "entities/AIBehavior.hpp"
#include "entities/Entity.hpp"
#include "entities/Inventory.hpp"
#include "entities/StatusEffects.hpp"
#include "entities/Stats.hpp"
#include "entities/TalentSet.hpp"

namespace engine {

// Anything that takes a turn. Composed of component-style member objects
// rather than expressed through subclassing (see
// ARCHITECTURE_DECISIONS.md) -- a new monster variant is meant to be "a
// Monster with different Stats + a different AIBehavior", not a new
// subclass.
class Actor : public Entity {
public:
    Actor(std::string name, char glyph, Position position, Stats stats,
          std::unique_ptr<AIBehavior> ai = nullptr)
        : Entity(std::move(name), glyph, position),
          stats_(stats),
          ai_(std::move(ai)) {}

    Stats& stats() { return stats_; }
    const Stats& stats() const { return stats_; }

    // nullptr for the Player -- input drives the player, not a strategy
    // object. Always non-null for a Monster (enforced by Monster's
    // constructor, see Monster.hpp).
    AIBehavior* ai() { return ai_.get(); }
    const AIBehavior* ai() const { return ai_.get(); }

    Inventory& inventory() { return inventory_; }
    const Inventory& inventory() const { return inventory_; }

    TalentSet& talents() { return talents_; }
    const TalentSet& talents() const { return talents_; }

    StatusEffects& statusEffects() { return statusEffects_; }
    const StatusEffects& statusEffects() const { return statusEffects_; }

private:
    Stats stats_;
    std::unique_ptr<AIBehavior> ai_;
    Inventory inventory_;
    TalentSet talents_;
    StatusEffects statusEffects_;
};

} // namespace engine
