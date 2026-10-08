#pragma once

// Test fixture: the Thief kit from before the talent trees. A Dexterity kit: shots, a vault kick that retreats, and Piercing Shot with its own critical bonus.
// The game no longer grants these; they are fixed, well-understood data for
// the isolated combat tests (see ClassKits.hpp). Each `power` is the
// intended total minus the fixture class's attribute bonus, so the tests can
// compute exact expected damage.

#include <optional>
#include <vector>

#include "entities/Talent.hpp"

namespace engine {

std::vector<Talent> thiefTalents();

// The talent this kit unlocked at `level` (2, 4 or 7), if any.
std::optional<Talent> thiefTalentUnlockedAtLevel(int level);

} // namespace engine
