#pragma once

namespace engine {

// Which playable class the person is currently controlling. Own tiny
// header, same reasoning as MonsterType.hpp (Prompt 12): PlayerClassFactory.hpp
// needs it, and so does anything that has to remember "which class is
// this" without pulling in the whole factory (e.g. SaveGameState).
enum class PlayerClass {
    Spellblade, // Str+Int hybrid melee/caster -- the original class, Prompt 9
    Marauder,   // pure Strength, hp-as-resource berserker -- Prompt 15
};

} // namespace engine
