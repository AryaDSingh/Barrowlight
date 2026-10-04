#pragma once

namespace engine {
// How hard the dungeon fights back, in one place. Percentages of the base
// numbers: playtest and tune these rather than individual monsters.
inline constexpr int kMonsterLifePercent = 150;      // every enemy's life
inline constexpr int kMonsterDamagePercent = 135;    // every enemy blow
inline constexpr int kEncounterBudgetPercent = 120;  // how many packs a floor holds
} // namespace engine
