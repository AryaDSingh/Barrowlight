#pragma once

#include <memory>
#include <string>
#include <utility>

#include "entities/Actor.hpp"

namespace engine {

// A non-player Actor. Always constructed with an AIBehavior -- there is
// no default here on purpose, unlike Player. What makes different monster
// types feel different is which Stats, which AIBehavior (parameterized
// with its own numbers), and -- as of Prompt 10 -- which talents get
// plugged in here, not a subclass per monster type. Most monster types
// don't need talents at all (a plain melee/ranged attack doesn't need
// cooldown tracking); Shaman and Bomber do, reusing the same TalentSet
// the player uses for exactly the same reason: it's already generic,
// nothing about it is player-specific.
class Monster : public Actor {
public:
    Monster(std::string name, char glyph, Position position, Stats stats,
            std::unique_ptr<AIBehavior> ai, TalentSet talents = TalentSet{})
        : Actor(std::move(name), glyph, position, stats, std::move(ai), std::move(talents)) {}
};

} // namespace engine
