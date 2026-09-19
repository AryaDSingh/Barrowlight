#pragma once

#include <memory>
#include <string>
#include <utility>

#include "entities/Actor.hpp"

namespace engine {

// A non-player Actor. Always constructed with an AIBehavior -- there is
// no default here on purpose, unlike Player. What makes different monster
// types feel different is which Stats and which AIBehavior strategy get
// plugged in here, not a subclass per monster type.
class Monster : public Actor {
public:
    Monster(std::string name, char glyph, Position position, Stats stats,
            std::unique_ptr<AIBehavior> ai)
        : Actor(std::move(name), glyph, position, stats, std::move(ai)) {}
};

} // namespace engine
