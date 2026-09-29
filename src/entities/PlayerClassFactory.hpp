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

// Legacy adapters for pre-tree combat fixtures. Live creation uses basicAttack()
// and startingTreeAllowed() from TalentCatalog; it never grants these fixed kits.
TalentSet talentSetForClass(PlayerClass cls);
std::optional<Talent> talentUnlockedAtLevel(PlayerClass cls, int level);

} // namespace engine
