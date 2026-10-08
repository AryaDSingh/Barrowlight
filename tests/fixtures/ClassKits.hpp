#pragma once

// Fixed talent kits for the isolated combat tests. Before the talent trees,
// each class started with a hand-built kit and unlocked one more ability at
// set levels. The game no longer uses these (characters are built from the
// talent catalog), but the kits remain good, stable test data for the talent
// engine: known powers, cooldowns and shapes the tests can compute against.

#include <optional>
#include <vector>

#include "entities/PlayerClass.hpp"
#include "entities/TalentSet.hpp"

namespace engine {

TalentSet talentSetForClass(PlayerClass cls);                       // a class's starting kit
std::optional<Talent> talentUnlockedAtLevel(PlayerClass cls, int level); // what it unlocked at `level`, if anything
std::vector<Talent> fullKitForClass(PlayerClass cls);              // the starting kit plus every unlock

} // namespace engine
