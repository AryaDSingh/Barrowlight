#pragma once

#include "entities/PlayerClass.hpp"
#include "entities/Stats.hpp"
#include "entities/TalentSet.hpp"

namespace engine {

// The starting Stats for `cls` -- hp/mana pools and Strength/Dexterity/
// Intelligence, each chosen to match the class's identity (see
// ARCHITECTURE_DECISIONS.md, "Multi-class system and Marauder") rather
// than being arbitrary numbers. maxMana is computed from the real
// AttributeFormulas::manaBonusFromIntelligence formula, same as it was
// for the Spellblade alone before this prompt, not hardcoded.
Stats statsForClass(PlayerClass cls);

// The starting TalentSet for `cls` -- spellbladeTalents() or
// marauderTalents(), wrapped. Mirrors
// MonsterFactory::createMonster()'s "one factory function, a switch
// over an enum, data plugged in per case" shape exactly, just for
// player classes instead of monster types.
TalentSet talentSetForClass(PlayerClass cls);

} // namespace engine
