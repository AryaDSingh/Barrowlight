#pragma once

#include <optional>

#include "entities/PlayerClass.hpp"
#include "entities/Stats.hpp"
#include "entities/TalentSet.hpp"

namespace engine {

// The starting Stats for `cls` -- hp/mana pools and Strength/Dexterity/
// Intelligence, each chosen to match the class's identity (see
// ARCHITECTURE_DECISIONS.md, "Multi-class system and Marauder") rather
// than being arbitrary numbers. Hand-picked per class (see
// PlayerClassFactory.cpp), not derived from the AttributeFormulas
// system -- starting HP/Mana are a genuinely separate decision from the
// Strength/Dexterity/Intelligence spread (see
// ARCHITECTURE_DECISIONS.md, "The attribute-system redesign").
Stats statsForClass(PlayerClass cls);

// The starting TalentSet for `cls` -- spellbladeTalents() or
// warriorTalents(), wrapped. Mirrors MonsterFactory::createMonster()'s
// "one factory function, a switch over an enum, data plugged in per
// case" shape exactly, just for player classes instead of monster
// types.
TalentSet talentSetForClass(PlayerClass cls);

// Prompt 23: the talent `cls` unlocks at exactly `level`, if any --
// dispatches to warriorTalentUnlockedAtLevel()/
// mageTalentUnlockedAtLevel()/thiefTalentUnlockedAtLevel(), the
// same per-class-function-behind-one-dispatcher shape
// talentSetForClass() already uses. Spellblade always returns nullopt
// here -- it's reserved for its own separate unlock mechanism (see
// MetaProgress.hpp), not level-gated talents the way the three base
// classes are, and by the time it's playable at all its full 9-talent
// kit is already known from the start (see talentSetForClass()).
std::optional<Talent> talentUnlockedAtLevel(PlayerClass cls, int level);

} // namespace engine
