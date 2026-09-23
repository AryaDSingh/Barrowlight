// Standalone sanity check for the Prompt 23 talent-unlock system --
// TalentSet::learnTalent() and PlayerClassFactory::talentUnlockedAtLevel().
// Hand-computed expected results, no SFML, no window, no Application.

#include <iostream>
#include <string>

#include "entities/FighterTalents.hpp"
#include "entities/PlayerClassFactory.hpp"
#include "entities/SorcererTalents.hpp"
#include "entities/ThiefTalents.hpp"

using namespace engine;

namespace {
bool g_allOk = true;

void check(bool condition, const std::string& description) {
    g_allOk &= condition;
    std::cout << (condition ? "[ok] " : "[FAIL] ") << description << '\n';
}
} // namespace

int main() {
    // --- TalentSet::learnTalent(): appends without disturbing existing
    // entries or their cooldown tracking.
    {
        TalentSet talents = talentSetForClass(PlayerClass::Fighter);
        check(talents.knownTalents().size() == 4, "Fighter starts with 4 known talents");

        talents.startCooldown(1); // Cleave, index 1
        check(!talents.isReady(1), "Cleave is on cooldown after starting it (sanity check)");

        talents.learnTalent(Talent{/*name=*/"Test Talent"});
        check(talents.knownTalents().size() == 5, "learnTalent() grows the known-talent count to 5");
        check(talents.knownTalents()[4].name == "Test Talent",
              "the newly learned talent lands at the new index (4), not inserted earlier");
        check(!talents.isReady(1), "Cleave's cooldown (index 1) is undisturbed by the new talent");
        check(talents.isReady(4), "the newly learned talent starts with 0 cooldown -- usable immediately");
    }

    // --- Fighter's unlock levels: 4 and 7 only.
    check(!fighterTalentUnlockedAtLevel(1).has_value(), "Fighter has no unlock at level 1");
    check(!fighterTalentUnlockedAtLevel(3).has_value(), "Fighter has no unlock at level 3");
    check(fighterTalentUnlockedAtLevel(4).has_value() &&
              fighterTalentUnlockedAtLevel(4)->name == "Whirlwind",
          "Fighter unlocks Whirlwind at level 4");
    check(!fighterTalentUnlockedAtLevel(5).has_value(), "Fighter has no unlock at level 5");
    check(fighterTalentUnlockedAtLevel(7).has_value() &&
              fighterTalentUnlockedAtLevel(7)->name == "Undying Rage",
          "Fighter unlocks Undying Rage at level 7");
    check(!fighterTalentUnlockedAtLevel(10).has_value(), "Fighter has no unlock at level 10");

    // --- Sorcerer's unlock levels.
    check(sorcererTalentUnlockedAtLevel(4).has_value() &&
              sorcererTalentUnlockedAtLevel(4)->name == "Meteor",
          "Sorcerer unlocks Meteor at level 4");
    check(sorcererTalentUnlockedAtLevel(7).has_value() &&
              sorcererTalentUnlockedAtLevel(7)->name == "Overload",
          "Sorcerer unlocks Overload at level 7");
    check(!sorcererTalentUnlockedAtLevel(6).has_value(), "Sorcerer has no unlock at level 6");

    // --- Thief's unlock levels, including the reused Execution mechanic.
    check(thiefTalentUnlockedAtLevel(4).has_value() &&
              thiefTalentUnlockedAtLevel(4)->name == "Piercing Shot",
          "Thief unlocks Piercing Shot at level 4");
    check(thiefTalentUnlockedAtLevel(4)->conditionalHpFraction > 0.29f &&
              thiefTalentUnlockedAtLevel(4)->conditionalMultiplier == 3,
          "Piercing Shot reuses Execution's exact conditional-multiplier mechanic (30% hp, 3x)");
    check(thiefTalentUnlockedAtLevel(7).has_value() &&
              thiefTalentUnlockedAtLevel(7)->name == "Adrenaline",
          "Thief unlocks Adrenaline at level 7");

    // --- PlayerClassFactory::talentUnlockedAtLevel() dispatches correctly.
    check(talentUnlockedAtLevel(PlayerClass::Fighter, 4)->name == "Whirlwind",
          "talentUnlockedAtLevel dispatches Fighter correctly");
    check(talentUnlockedAtLevel(PlayerClass::Sorcerer, 4)->name == "Meteor",
          "talentUnlockedAtLevel dispatches Sorcerer correctly");
    check(talentUnlockedAtLevel(PlayerClass::Thief, 4)->name == "Piercing Shot",
          "talentUnlockedAtLevel dispatches Thief correctly");

    // --- Spellblade never unlocks talents this way -- reserved for its
    // own separate mechanism (MetaProgress), not level-gated like the
    // three base classes.
    check(!talentUnlockedAtLevel(PlayerClass::Spellblade, 4).has_value(),
          "Spellblade has no talentUnlockedAtLevel unlock at level 4");
    check(!talentUnlockedAtLevel(PlayerClass::Spellblade, 7).has_value(),
          "Spellblade has no talentUnlockedAtLevel unlock at level 7");

    std::cout << "\n"
              << (g_allOk ? "All talent-unlock checks passed." : "Some talent-unlock checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
