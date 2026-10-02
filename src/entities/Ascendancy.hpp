#pragma once

#include <array>
#include <string>

#include "entities/PlayerClass.hpp"

namespace engine {

// Ascendancy (ASCENDANCY_DESIGN.md): a permanent identity earned through
// the two trials. Each class has one ascendancy for now; the first trial
// grants it with one point, the second trial a second point. Each point
// buys one of six nodes, which have no ranks. Nodes are ordinary talents
// in talentCatalog() whose treeId is the ascendancy's id.
struct AscendancyDefinition {
    const char* id;
    const char* name;
    PlayerClass cls;
    const char* tagline;
    const char* icon;
    std::array<const char*, 6> nodes;
};

inline constexpr std::array<AscendancyDefinition, 3> kAscendancies{{
    {"juggernaut", "Juggernaut", PlayerClass::Warrior, "An unstoppable frontline fighter who shrugs off control.", "hammer-drop",
     {"juggernaut.unstoppable", "juggernaut.earthshaker", "juggernaut.rampage", "juggernaut.last_stand", "juggernaut.iron_skin",
      "juggernaut.crushing_blows"}},
    {"elementalist", "Elementalist", PlayerClass::Mage, "A master of ailments and elemental combinations.", "fireball",
     {"elementalist.fury", "elementalist.ward", "elementalist.conduit", "elementalist.lingering", "elementalist.overload",
      "elementalist.attunement"}},
    {"trickster", "Trickster", PlayerClass::Thief, "An evasive opportunist who turns openings into damage.", "backstab",
     {"trickster.smoke_veil", "trickster.fan_of_knives", "trickster.opportunist", "trickster.slippery", "trickster.quick_hands",
      "trickster.killer_instinct"}},
}};

inline const AscendancyDefinition* findAscendancy(const std::string& id) {
    for (const auto& a : kAscendancies) if (id == a.id) return &a;
    return nullptr;
}
inline const AscendancyDefinition* ascendancyFor(PlayerClass cls) {
    for (const auto& a : kAscendancies) if (a.cls == cls) return &a;
    return nullptr;
}
inline bool isAscendancyTree(const std::string& treeId) { return findAscendancy(treeId) != nullptr; }

// The two trials. Their sigils drop from the Goblin Warlord and the Lich;
// the second trial also needs the first one cleared. Bit n-1 of the
// player's trialKeys/trialsCleared is trial n.
inline constexpr int kTrialCount = 2;
inline const char* trialName(int trial) { return trial == 1 ? "Trial of Stone" : "Trial of the Fallen"; }
inline const char* trialSigil(int trial) { return trial == 1 ? "Stone Sigil" : "Bone Sigil"; }
inline const char* trialGuardian(int trial) { return trial == 1 ? "The Stone Warden" : "The Fallen Saint"; }
// The floor whose difficulty the trial arena uses.
inline int trialDifficultyFloor(int trial) { return trial == 1 ? 8 : 16; }

} // namespace engine
