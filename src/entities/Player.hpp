#pragma once

#include <utility>

#include "entities/Actor.hpp"

namespace engine {

// The player-controlled Actor. Always has ai() == nullptr -- decisions
// come from input (a later prompt's InputHandler), never from a strategy
// object.
//
// As of Prompt 15, the talent kit is passed in rather than hardcoded --
// previously this constructor built TalentSet(spellbladeTalents())
// itself, coupling Player to one specific class. Which kit a given
// Player actually knows is now PlayerClassFactory's job
// (talentSetForClass), the same "factory decides, the class itself
// stays generic" shape MonsterFactory already established for monsters.
// There's still no leveling/unlock system in this vertical slice, so a
// class's full kit is always known from the start -- gating talent
// access behind character progression would be scope this project
// hasn't asked for.
//
// As of Prompt 20: level_/xp_ track character progression -- level_
// starts at 1, capped at 10; xp_ is progress toward the *next* level
// specifically (resets on level-up), not a cumulative lifetime total,
// so "45/60 XP" reads directly as "progress within this level" for the
// HUD rather than needing a separate cumulative-vs-incremental
// conversion. The actual leveling curve and level-up effects live in
// PlayerLeveling.hpp (logic), not here -- Player stays plain data plus
// simple accessors, the same separation this project has kept between
// data and behavior everywhere else (Stats/AttributeFormulas, Talent/
// TalentEffects).
//
// As of Prompt 24: hybridSpecced_ tracks the Fighter/Sorcerer hybrid
// path (see HybridSpec.hpp) -- an in-run choice, not a class change.
// The character's playerClass_ (tracked on Application, not here) and
// Stats stay exactly what they always were; this only affects which
// talents can be picked as the character continues leveling. Deliberately
// no separate "which abilities have been picked from the pool" list --
// that's always derivable by checking which of the opposing class's
// talent names already appear in this Player's own knownTalents(), so
// there's nothing here that could drift out of sync with the real
// talent list.
class Player : public Actor {
public:
    Player(Position position, Stats stats, TalentSet talents)
        : Actor("Player", '@', position, std::move(stats), nullptr, std::move(talents)) {}

    int& level() { return level_; }
    int level() const { return level_; }

    int& xp() { return xp_; }
    int xp() const { return xp_; }

    bool& hybridSpecced() { return hybridSpecced_; }
    bool hybridSpecced() const { return hybridSpecced_; }

    // How many attribute points this character has earned (2 per
    // level, see PlayerLeveling.hpp) but not yet spent. Decremented as
    // each point is allocated to Strength/Dexterity/Intelligence via
    // the AttributeAllocation screen -- see
    // Application::offerAttributeAllocationIfPending().
    int& unspentAttributePoints() { return unspentAttributePoints_; }
    int unspentAttributePoints() const { return unspentAttributePoints_; }

private:
    int level_ = 1;
    int xp_ = 0;
    bool hybridSpecced_ = false;
    int unspentAttributePoints_ = 0;
};

} // namespace engine
