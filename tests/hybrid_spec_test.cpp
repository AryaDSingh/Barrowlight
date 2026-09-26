// Standalone sanity check for HybridSpec (Prompt 24's Fighter/Sorcerer
// hybrid path) -- hand-computed expected results, no SFML, no window,
// no Application.

#include <iostream>
#include <string>

#include "entities/HybridSpec.hpp"
#include "entities/PlayerClassFactory.hpp"

using namespace engine;

namespace {
bool g_allOk = true;

void check(bool condition, const std::string& description) {
    g_allOk &= condition;
    std::cout << (condition ? "[ok] " : "[FAIL] ") << description << '\n';
}
} // namespace

int main() {
    // --- Eligibility: exactly Fighter and Sorcerer.
    check(isHybridEligible(PlayerClass::Warrior), "Fighter is hybrid-eligible");
    check(isHybridEligible(PlayerClass::Mage), "Sorcerer is hybrid-eligible");
    check(!isHybridEligible(PlayerClass::Thief),
          "Thief is not hybrid-eligible -- shares neither of Spellblade's stats");
    check(!isHybridEligible(PlayerClass::Spellblade),
          "Spellblade is not hybrid-eligible -- no opposing class to draw from");

    // --- Pool class: each draws from the other.
    check(hybridPoolClass(PlayerClass::Warrior) == PlayerClass::Mage,
          "Fighter's hybrid pool is Sorcerer's kit");
    check(hybridPoolClass(PlayerClass::Mage) == PlayerClass::Warrior,
          "Sorcerer's hybrid pool is Fighter's kit");

    // --- fullKitForClass: starting talents plus both level-gated
    // unlocks, regardless of what level a character actually is.
    check(fullKitForClass(PlayerClass::Warrior).size() == 6,
          "Fighter's full kit is 6 talents (4 starting + Whirlwind + Undying Rage)");
    check(fullKitForClass(PlayerClass::Mage).size() == 7,
          "Mage's full kit is 7 talents (4 starting + Blink + Meteor + Overload)");

    // --- availableHybridPicks: a fresh Fighter can pick from all 7 of
    // Sorcerer's abilities.
    {
        TalentSet freshFighter = talentSetForClass(PlayerClass::Warrior);
        std::vector<Talent> available = availableHybridPicks(PlayerClass::Warrior, freshFighter);
        check(available.size() == 7,
              "a fresh (unspecced) Fighter has all 7 Sorcerer abilities available to pick");
        check(available[0].name == "Arcane Bolt",
              "available picks are ordered the same way Sorcerer's own kit is ordered");
    }

    // --- Picking shrinks the pool, and doesn't re-offer what's already
    // known -- this is the core mechanic the whole choice screen
    // depends on being correct.
    {
        TalentSet fighter = talentSetForClass(PlayerClass::Warrior);
        fighter.learnTalent(fullKitForClass(PlayerClass::Mage).front()); // simulates picking it
        std::vector<Talent> available = availableHybridPicks(PlayerClass::Warrior, fighter);
        check(available.size() == 6, "picking one ability shrinks the available pool to 6");
        bool stillOffersArcaneBolt = false;
        for (const Talent& t : available) {
            if (t.name == "Arcane Bolt") {
                stillOffersArcaneBolt = true;
            }
        }
        check(!stillOffersArcaneBolt, "an already-picked ability is never offered again");
    }

    // --- Picking all 7 empties the pool entirely.
    {
        TalentSet fighter = talentSetForClass(PlayerClass::Warrior);
        for (const Talent& mageTalent : fullKitForClass(PlayerClass::Mage)) {
            fighter.learnTalent(mageTalent);
        }
        check(availableHybridPicks(PlayerClass::Warrior, fighter).empty(),
              "picking all 7 of the opposing kit empties the available pool");
    }

    // --- A non-eligible class always returns an empty pool, regardless
    // of what it already knows.
    {
        TalentSet thief = talentSetForClass(PlayerClass::Thief);
        check(availableHybridPicks(PlayerClass::Thief, thief).empty(),
              "Thief's available hybrid picks are always empty -- not eligible at all");
    }

    std::cout << "\n"
              << (g_allOk ? "All HybridSpec checks passed." : "Some HybridSpec checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
