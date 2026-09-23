#pragma once

namespace engine {

// Which attribute a source of damage (a Talent or a MonsterAttackProfile)
// scales with. Own tiny header for the same reason MonsterType got one
// in Prompt 12: both Talent.hpp and MonsterAttackProfile.hpp need it,
// and neither should have to include the other just to get an enum.
enum class DamageType {
    Physical, // scales with the attacker's Strength
    Magic,    // scales with the attacker's Intelligence
};

} // namespace engine
