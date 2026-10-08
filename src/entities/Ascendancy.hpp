#pragma once

#include <array>
#include <string>
#include <vector>

#include "entities/PlayerClass.hpp"

namespace engine {

// Ascendancy: a permanent identity earned through the trials. Each great
// boss drops a sigil that opens a trial at the obelisk in town; each trial
// won grants a point, and the first lets you choose any ascendancy your
// colours allow (ascendancyNeed in TalentProgression.hpp). One per run.
// Each point buys one of the ascendancy's six nodes, which have no ranks.
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
inline constexpr std::array<AscendancyDefinition, 12> kAscendancies{{
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
    // Born of Thornwood Hollow.
    {"beastwarden", "Beastwarden", kWarrior | kMage | kThief, "DEX", "A warden of the wild, never alone: Thornmaw grows with you and fights where you point.", "wolverine-claws",
     {"beastwarden.thornmaw", "beastwarden.point", "beastwarden.pack_of_two", "beastwarden.running_mate", "beastwarden.guardian", "beastwarden.call_wild"}},
    {"plaguebringer", "Plaguebringer", kWarrior | kMage | kThief, "INT", "A carrier of sickness: one foe infected, and the plague walks from room to room.", "virus",
     {"plaguebringer.patient_zero", "plaguebringer.contagion", "plaguebringer.outbreak", "plaguebringer.miasma", "plaguebringer.wasting", "plaguebringer.pandemic"}},
    // Born of Rimeholt.
    {"wintercaller", "Wintercaller", kWarrior | kMage | kThief, "INT", "A caller of the cold: freeze the water, and shatter what stands on it.", "snowflake-1",
     {"wintercaller.flash_freeze", "wintercaller.shatterpoint", "wintercaller.cold_blood", "wintercaller.glacial_armour", "wintercaller.rime_tide", "wintercaller.absolute_zero"}},
    {"gravelord", "Gravelord", kWarrior | kMage | kThief, "INT", "A lord of the grave: the dead you raise stand until they fall.", "tombstone",
     {"gravelord.standing_legion", "gravelord.tithe", "gravelord.rally", "gravelord.deaths_due", "gravelord.shield", "gravelord.last_rites"}},
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
// The third, the Trial of the Forge, is opened by the Forgemaster's sigil, and
// the fourth, the Trial of the Hollow, by the Hollow Mother's; both stand
// alone. Any trial won lets you take any ascendancy your colours allow.
inline constexpr int kTrialCount = 5, kForgeTrial = 3, kHollowTrial = 4, kWinterTrial = 5;
inline const char* trialName(int trial) { return trial == 5 ? "Trial of Winter" : trial == 4 ? "Trial of the Hollow" : trial == 3 ? "Trial of the Forge" : trial == 1 ? "Trial of Stone" : "Trial of the Fallen"; }
inline const char* trialSigil(int trial) { return trial == 5 ? "Frost Sigil" : trial == 4 ? "Thorn Sigil" : trial == 3 ? "Forge Sigil" : trial == 1 ? "Stone Sigil" : "Bone Sigil"; }
inline const char* trialGuardian(int trial) { return trial == 5 ? "The Frost Regent" : trial == 4 ? "The Thorn Queen" : trial == 3 ? "The Anvil-Born" : trial == 1 ? "The Stone Warden" : "The Fallen Saint"; }
// The floor whose difficulty the trial arena uses.
// (Winter's arena is a Rimeholt floor: as deep as 26.)
inline int trialDifficultyFloor(int trial) { return trial == 5 ? 42 : trial == 4 ? 16 : trial == 3 ? 10 : trial == 1 ? 8 : 16; }

} // namespace engine
