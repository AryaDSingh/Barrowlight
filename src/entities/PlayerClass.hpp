#pragma once

namespace engine {

// The origin a character starts from. An origin sets starting attributes,
// gear, sprite and the pool of trees the first tree point can open; it locks
// nothing else (every later gate is colour points, see HiddenTrees.hpp).
// Its own tiny header so SaveGameState can remember it without pulling in
// the factory. Saved as an int: append only.
enum class PlayerClass {
    Spellblade, // a Str+Int hybrid from the earliest version; not offered, kept for the tests
    Warrior,    // pure Strength
    Thief,      // pure Dexterity
    Mage,       // pure Intelligence
};

} // namespace engine
