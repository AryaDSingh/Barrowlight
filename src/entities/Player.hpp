#pragma once

#include <utility>

#include "entities/Actor.hpp"

namespace engine {

// The player-controlled Actor. Always has ai() == nullptr -- decisions
// come from input (a later prompt's InputHandler), never from a strategy
// object.
class Player : public Actor {
public:
    Player(Position position, Stats stats)
        : Actor("Player", '@', position, std::move(stats), nullptr) {}
};

} // namespace engine
