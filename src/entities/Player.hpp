#pragma once

#include <utility>

#include "entities/Actor.hpp"
#include "entities/SpellbladeTalents.hpp"

namespace engine {

// The player-controlled Actor. Always has ai() == nullptr -- decisions
// come from input (a later prompt's InputHandler), never from a strategy
// object.
//
// Knows the full Spellblade talent kit from the start (Prompt 9) --
// there's no leveling/unlock system in this vertical slice, so gating
// talent access behind character progression would be scope this project
// hasn't asked for.
class Player : public Actor {
public:
    Player(Position position, Stats stats)
        : Actor("Player", '@', position, std::move(stats), nullptr,
                TalentSet(spellbladeTalents())) {}
};

} // namespace engine
