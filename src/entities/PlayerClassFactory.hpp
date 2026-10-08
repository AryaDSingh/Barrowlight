#pragma once

#include <optional>

#include "entities/PlayerClass.hpp"
#include "entities/Stats.hpp"
#include "entities/TalentSet.hpp"

namespace engine {

// The starting Stats for `cls` -- hp/mana pools and Strength/Dexterity/
// Intelligence, each chosen to match the origin's identity. Hand-picked
// per origin, not derived from the attributes: starting HP/Mana are a
// separate decision from the Strength/Dexterity/Intelligence spread.
Stats statsForClass(PlayerClass cls);

} // namespace engine
