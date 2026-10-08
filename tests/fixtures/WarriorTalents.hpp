#pragma once

// Test fixture: the Warrior kit from before the talent trees. A Strength kit: melee strikes, a self-buff and a blood-cost strike.
// The game no longer grants these; they are fixed, well-understood data for
// the isolated combat tests (see ClassKits.hpp). Each `power` is the
// intended total minus the fixture class's attribute bonus, so the tests can
// compute exact expected damage.

#include <optional>
#include <vector>

#include "entities/Talent.hpp"

namespace engine {

std::vector<Talent> warriorTalents();

// The talent this kit unlocked at `level` (2, 4 or 7), if any.
std::optional<Talent> warriorTalentUnlockedAtLevel(int level);

} // namespace engine
