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
// rather than expressed through subclassing: a new monster variant is "a
// Monster with different Stats + a different AIBehavior", not a new
// subclass.
class Actor : public Entity {
public:
    Actor(std::string name, char glyph, Position position, Stats stats,
          std::unique_ptr<AIBehavior> ai = nullptr, TalentSet talents = TalentSet{})
        : Entity(std::move(name), glyph, position),
          stats_(stats),
          ai_(std::move(ai)),
          talents_(std::move(talents)) {}

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

    // How much XP defeating this Actor grants the player.
    // 0 by default -- meaningless for the Player (nothing grants XP for
    // "defeating" yourself), genuinely set per monster type by
    // MonsterFactory::createMonster(). Lives on Actor rather than
    // Monster specifically so Application::checkAndHandleDeath, which
    // only ever sees a generic Actor&, can read it without needing a
    // downcast -- the same reasoning Stats/statusEffects already follow.
    int xpReward() const { return xpReward_; }
    void setXpReward(int reward) { xpReward_ = reward; }

private:
    Stats stats_;
    std::unique_ptr<AIBehavior> ai_;
    Inventory inventory_;
    TalentSet talents_;
    StatusEffects statusEffects_;
    int xpReward_ = 0;
};

} // namespace engine
