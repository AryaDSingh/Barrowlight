#pragma once

// Test fixture: the Mage kit from before the talent trees. An Intelligence kit: bolts, an area storm, a self-buff, and Mind Shatter, which stuns its target.
// The game no longer grants these; they are fixed, well-understood data for
// the isolated combat tests (see ClassKits.hpp). Each `power` is the
// intended total minus the fixture class's attribute bonus, so the tests can
// compute exact expected damage.

#include <optional>
#include <vector>

#include "entities/Talent.hpp"

namespace engine {

std::vector<Talent> mageTalents();

// The talent this kit unlocked at `level` (2, 4 or 7), if any.
std::optional<Talent> mageTalentUnlockedAtLevel(int level);

} // namespace engine
