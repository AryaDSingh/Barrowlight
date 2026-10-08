#pragma once

// Test fixture: the Spellblade kit from before the talent trees. Nine talents in two trees (Blade, Flame): melee strikes, an execute, bolts, a fireball, Blink and Renewal.
// The game no longer grants these; they are fixed, well-understood data for
// the isolated combat tests (see ClassKits.hpp). Each `power` is the
// intended total minus the fixture class's attribute bonus, so the tests can
// compute exact expected damage.

#include <optional>
#include <vector>

#include "entities/Talent.hpp"

namespace engine {

std::vector<Talent> spellbladeTalents();

} // namespace engine
