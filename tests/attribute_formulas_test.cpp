// Standalone sanity check for AttributeFormulas -- the pure, deterministic
// functions only. rollDodge() itself (the one function here that actually
// draws a random number) is deliberately not tested in isolation, same
// precedent as Chaser's onHitChance roll (Prompt 10): didDodge(), the
// pure comparison it's built on, is exhaustively tested instead, and live
// play is what confirms the randomized rate behaves plausibly.

#include <iostream>
#include <string>

#include "entities/AttributeFormulas.hpp"

using namespace engine;

namespace {
bool g_allOk = true;

void check(bool condition, const std::string& description) {
    g_allOk &= condition;
    std::cout << (condition ? "[ok] " : "[FAIL] ") << description << '\n';
}
} // namespace

int main() {
    // --- physicalDamageBonus: +1 per 2 points of strength above the 10
    // baseline, truncated toward zero (C++'s native integer division).
    check(physicalDamageBonus(10) == 0, "physicalDamageBonus(10) == 0 (baseline contributes nothing)");
    check(physicalDamageBonus(14) == 2, "physicalDamageBonus(14) == 2 (the Spellblade's own strength)");
    check(physicalDamageBonus(18) == 4, "physicalDamageBonus(18) == 4");
    check(physicalDamageBonus(20) == 5, "physicalDamageBonus(20) == 5");
    check(physicalDamageBonus(8) == -1, "physicalDamageBonus(8) == -1 (below baseline, genuinely weaker)");
    check(physicalDamageBonus(6) == -2, "physicalDamageBonus(6) == -2 (Ogre's dexterity value, used here on strength for the arithmetic)");
    check(physicalDamageBonus(11) == 0,
          "physicalDamageBonus(11) == 0 (truncation toward zero: 1/2 == 0, not a floor to -1)");

    // --- magicDamageBonus: identical shape, mirrored onto Intelligence.
    check(magicDamageBonus(10) == 0, "magicDamageBonus(10) == 0 (baseline contributes nothing)");
    check(magicDamageBonus(18) == 4, "magicDamageBonus(18) == 4 (the Spellblade's own intelligence)");
    check(magicDamageBonus(16) == 3, "magicDamageBonus(16) == 3 (Bomber's intelligence)");
    check(magicDamageBonus(14) == 2, "magicDamageBonus(14) == 2 (the boss's intelligence)");
    check(magicDamageBonus(6) == -2, "magicDamageBonus(6) == -2 (below baseline)");

    // --- dodgeChance: +3% per point of dexterity above the 10 baseline,
    // floored at 0%, capped at 30%.
    check(dodgeChance(10) == 0.f, "dodgeChance(10) == 0 (baseline)");
    check(dodgeChance(6) == 0.f, "dodgeChance(6) == 0 (below baseline floors at 0, doesn't go negative)");
    check(dodgeChance(8) == 0.f, "dodgeChance(8) == 0 (Archer/Ogre's strength value, used here on dexterity)");
    const float spiderDodge = dodgeChance(16);
    check(spiderDodge > 0.1799f && spiderDodge < 0.1801f,
          "dodgeChance(16) == 0.18 (Spider's own dexterity -- 18%)");
    const float archerDodge = dodgeChance(18);
    check(archerDodge > 0.2399f && archerDodge < 0.2401f,
          "dodgeChance(18) == 0.24 (Archer's own dexterity -- 24%, the roster's most evasive)");
    const float dexTwentyDodge = dodgeChance(20);
    check(dexTwentyDodge > 0.2999f && dexTwentyDodge < 0.3001f,
          "dodgeChance(20) == 0.30 (exactly at the cap, computed via multiplication -- "
          "compared with tolerance, not exact ==, since 10*0.03f isn't guaranteed bit-exact "
          "to the literal 0.30f the way a clamped return value is)");
    check(dodgeChance(30) == 0.30f, "dodgeChance(30) == 0.30 (well past the cap, still clamped)");
    check(dodgeChance(100) == 0.30f, "dodgeChance(100) == 0.30 (extreme value, still clamped -- the cap is real)");

    // --- manaBonusFromIntelligence: +1 per point above the 10 baseline,
    // floored at 0 (unlike the damage bonuses, this can't go negative --
    // a mana pool can't shrink below its base from low intelligence).
    check(manaBonusFromIntelligence(10) == 0, "manaBonusFromIntelligence(10) == 0 (baseline)");
    check(manaBonusFromIntelligence(18) == 8,
          "manaBonusFromIntelligence(18) == 8 (the Spellblade's own intelligence -- "
          "12 base + 8 == 20, matching the original Prompt 9 tuned maxMana exactly)");
    check(manaBonusFromIntelligence(6) == 0,
          "manaBonusFromIntelligence(6) == 0 (below baseline floors at 0, doesn't reduce the pool)");

    // --- didDodge: pure comparison, roll < chance.
    check(didDodge(0.3f, 0.29f), "didDodge(0.3, 0.29) == true (roll just under the chance)");
    check(!didDodge(0.3f, 0.31f), "didDodge(0.3, 0.31) == false (roll just over the chance)");
    check(!didDodge(0.3f, 0.3f), "didDodge(0.3, 0.3) == false (an exact tie does not count as a dodge)");
    check(!didDodge(0.f, 0.f), "didDodge(0, 0) == false (zero chance never dodges, even at roll 0)");
    check(didDodge(1.f, 0.f), "didDodge(1.0, 0.0) == true (a theoretical 100% chance always dodges)");

    std::cout << "\n"
              << (g_allOk ? "All attribute formula checks passed." : "Some checks FAILED.") << '\n';
    return g_allOk ? 0 : 1;
}
