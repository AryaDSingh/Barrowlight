#pragma once

#include <array>
#include <string>
#include <vector>

#include "entities/PlayerClass.hpp"

namespace engine {

// Ascendancy (ASCENDANCY_DESIGN.md): a permanent identity earned through
// the two trials. Winning the first trial lets the player choose one of
// their class's four ascendancies and grants a point; the second trial a
// second point. Each point buys one of six nodes, which have no ranks.
// Nodes are ordinary talents in talentCatalog() whose treeId is the
// ascendancy's id.
// Which classes may take an ascendancy: a bit per PlayerClass.
inline constexpr unsigned classBit(PlayerClass c) { return 1u << static_cast<unsigned>(c); }
inline constexpr unsigned kWarrior = classBit(PlayerClass::Warrior), kMage = classBit(PlayerClass::Mage),
                          kThief = classBit(PlayerClass::Thief);

struct AscendancyDefinition {
    const char* id;
    const char* name;
    unsigned classes; // classBit()s of the classes that can choose it
    const char* attributes;
    const char* tagline;
    const char* icon;
    std::array<const char*, 6> nodes;
};

// Each class chooses among four: its own, the two hybrids it shares with
// the other classes, and the Paragon.
inline constexpr std::array<AscendancyDefinition, 8> kAscendancies{{
    {"juggernaut", "Juggernaut", kWarrior, "STR", "An unstoppable frontline fighter who shrugs off control.", "hammer-drop",
     {"juggernaut.unstoppable", "juggernaut.earthshaker", "juggernaut.rampage", "juggernaut.last_stand", "juggernaut.iron_skin",
      "juggernaut.crushing_blows"}},
    {"elementalist", "Elementalist", kMage, "INT", "A master of ailments and elemental combinations.", "fireball",
     {"elementalist.fury", "elementalist.ward", "elementalist.conduit", "elementalist.lingering", "elementalist.overload",
      "elementalist.attunement"}},
    {"trickster", "Trickster", kThief, "DEX", "An evasive opportunist who turns openings into damage.", "backstab",
     {"trickster.smoke_veil", "trickster.fan_of_knives", "trickster.opportunist", "trickster.slippery", "trickster.quick_hands",
      "trickster.killer_instinct"}},
    {"templar", "Templar", kWarrior | kMage, "STR / INT", "A holy knight who uses magic to protect, and punishes those who strike.", "checked-shield",
     {"templar.consecrate", "templar.aegis", "templar.zeal", "templar.retribution", "templar.devotion", "templar.righteous"}},
    {"shadowcaster", "Shadowcaster", kMage | kThief, "DEX / INT", "A caster who strikes from hiding and slips away again.", "shadow-follower",
     {"shadowcaster.veil", "shadowcaster.hex", "shadowcaster.hidden_casting", "shadowcaster.lingering_shadow", "shadowcaster.shade_step",
      "shadowcaster.spell_thief"}},
    {"duelist", "Duelist", kWarrior | kThief, "STR / DEX", "A one-on-one specialist who wins through timing and counters.", "sword-clash",
     {"duelist.challenge", "duelist.flurry", "duelist.momentum", "duelist.finisher", "duelist.counter", "duelist.en_garde"}},
    {"paragon", "Paragon", kWarrior | kMage | kThief, "STR / DEX / INT", "A master of everything and of nothing in particular: steady, whole, hard to kill.", "aura",
     {"paragon.exalt", "paragon.renewal", "paragon.balance", "paragon.versatility", "paragon.resilience", "paragon.wellspring"}},
    // Born of the Ashen Foundry.
    {"forgeknight", "Forgeknight", kWarrior | kMage | kThief, "STR", "A knight of the forge: heat is your armour, and the fire answers every blow.", "hammer-drop",
     {"forgeknight.heat_engine", "forgeknight.slam", "forgeknight.quench", "forgeknight.burning_plate", "forgeknight.overheat", "forgeknight.anvil"}},
}};

inline const AscendancyDefinition* findAscendancy(const std::string& id) {
    for (const auto& a : kAscendancies) if (id == a.id) return &a;
    return nullptr;
}
inline bool ascendancyAllowed(const AscendancyDefinition& a, PlayerClass cls) { return (a.classes & classBit(cls)) != 0; }
inline std::vector<const AscendancyDefinition*> ascendanciesFor(PlayerClass cls) {
    std::vector<const AscendancyDefinition*> result;
    for (const auto& a : kAscendancies) if (ascendancyAllowed(a, cls)) result.push_back(&a);
    return result;
}
inline bool isAscendancyTree(const std::string& treeId) { return findAscendancy(treeId) != nullptr; }

// The two trials. Their sigils drop from the Goblin Warlord and the Lich;
// the second trial also needs the first one cleared. Bit n-1 of the
// player's trialKeys/trialsCleared is trial n.
// The third, the Trial of the Forge, is opened by the Forgemaster's sigil and
// stands alone. Any trial won lets you take any ascendancy your colours allow.
inline constexpr int kTrialCount = 3, kForgeTrial = 3;
inline const char* trialName(int trial) { return trial == 3 ? "Trial of the Forge" : trial == 1 ? "Trial of Stone" : "Trial of the Fallen"; }
inline const char* trialSigil(int trial) { return trial == 3 ? "Forge Sigil" : trial == 1 ? "Stone Sigil" : "Bone Sigil"; }
inline const char* trialGuardian(int trial) { return trial == 3 ? "The Anvil-Born" : trial == 1 ? "The Stone Warden" : "The Fallen Saint"; }
// The floor whose difficulty the trial arena uses.
inline int trialDifficultyFloor(int trial) { return trial == 3 ? 10 : trial == 1 ? 8 : 16; }

} // namespace engine
