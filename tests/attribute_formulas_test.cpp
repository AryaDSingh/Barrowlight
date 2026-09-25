// Standalone sanity check for AttributeFormulas -- the pure,
// deterministic functions only. rollDodge()/rollCrit() themselves (the
// functions here that actually draw a random number) are deliberately
// not tested in isolation, same precedent as Chaser's onHitChance roll
// (Prompt 10): didDodge(), the pure comparison they're built on, is
// exhaustively tested instead, and live play is what confirms a
// randomized rate behaves plausibly.
//
// Fully rewritten for the attribute-system redesign -- the original
// Prompt 14 version tested a baseline-10 model (physicalDamageBonus,
// magicDamageBonus, manaBonusFromIntelligence) that no longer exists.
// The new system has no baseline at all: every point counts at full
// value from zero, damage scaling is per-ability (not universal), and
// scales by the ability's own cooldown tier.

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
    // --- tierForCooldown: the four bands.
    check(tierForCooldown(0) == AbilityCooldownTier::Filler, "cooldown 0 is Filler");
    check(tierForCooldown(1) == AbilityCooldownTier::Filler, "cooldown 1 is Filler");
    check(tierForCooldown(2) == AbilityCooldownTier::Core, "cooldown 2 is Core");
    check(tierForCooldown(3) == AbilityCooldownTier::Core, "cooldown 3 is Core");
    check(tierForCooldown(4) == AbilityCooldownTier::Power, "cooldown 4 is Power");
    check(tierForCooldown(5) == AbilityCooldownTier::Power, "cooldown 5 is Power");
    check(tierForCooldown(6) == AbilityCooldownTier::Signature, "cooldown 6 is Signature");
    check(tierForCooldown(10) == AbilityCooldownTier::Signature, "cooldown 10 (well past 6) is still Signature");

    // --- abilityDamageBonus: +1 per 5 points in the scaling stat,
    // multiplied by the ability's own cooldown tier (Filler x1.0, Core
    // x1.5, Power x2.0, Signature x2.5), truncated toward zero.
    check(abilityDamageBonus(ScalingStat::Strength, 0, 0) == 0,
          "abilityDamageBonus with 0 stat investment is 0, regardless of tier");
    check(abilityDamageBonus(ScalingStat::Strength, 5, 0) == 1,
          "abilityDamageBonus(Strength, 5, Filler) == 1 (5/5 * 1.0)");
    check(abilityDamageBonus(ScalingStat::Strength, 10, 1) == 2,
          "abilityDamageBonus(Strength, 10, cooldown 1/Filler) == 2 (10/5 * 1.0)");
    check(abilityDamageBonus(ScalingStat::Strength, 24, 1) == 4,
          "abilityDamageBonus(Strength, 24, Filler) == 4 (24/5 == 4.8, truncated down, not rounded)");
    check(abilityDamageBonus(ScalingStat::Strength, 10, 2) == 3,
          "abilityDamageBonus(Strength, 10, Core) == 3 (10/5 * 1.5 == 3.0)");
    check(abilityDamageBonus(ScalingStat::Strength, 10, 4) == 4,
          "abilityDamageBonus(Strength, 10, Power) == 4 (10/5 * 2.0 == 4.0)");
    check(abilityDamageBonus(ScalingStat::Intelligence, 10, 6) == 5,
          "abilityDamageBonus(Intelligence, 10, Signature) == 5 (10/5 * 2.5 == 5.0)");
    check(abilityDamageBonus(ScalingStat::Intelligence, 24, 7) == 12,
          "abilityDamageBonus(Intelligence, 24, Signature) == 12 (24/5 == 4.8, *2.5 == 12.0) -- "
          "a heavily-invested Signature ability, the top of this formula's current range");

    // --- dodgeChance: +0.5% per point of *current total* dexterity (no
    // baseline subtraction at all -- every point counts, including a
    // class's starting spread), capped at 25%.
    check(dodgeChance(0) == 0.f, "dodgeChance(0) == 0");
    const float dexTwoDodge = dodgeChance(2);
    check(dexTwoDodge > 0.0099f && dexTwoDodge < 0.0101f,
          "dodgeChance(2) == 0.01 (1%) -- Thief's Dex-2 minority stats would compute this way");
    const float dexSixDodge = dodgeChance(6);
    check(dexSixDodge > 0.0299f && dexSixDodge < 0.0301f,
          "dodgeChance(6) == 0.03 (3%) -- Thief's own starting Dexterity");
    const float dexFiftyDodge = dodgeChance(50);
    check(dexFiftyDodge > 0.2499f && dexFiftyDodge < 0.2501f,
          "dodgeChance(50) == 0.25 (exactly at the cap, computed via multiplication -- "
          "compared with tolerance, not exact ==, since 50*0.005f isn't guaranteed bit-exact "
          "to the literal 0.25f the way a clamped return value is)");
    check(dodgeChance(100) == 0.25f, "dodgeChance(100) == 0.25 (well past the cap, still clamped)");

    // --- critChanceBonus: +0.5% per point of current total dexterity,
    // no cap (unlike dodge, crit is allowed to climb as high as
    // investment takes it).
    check(critChanceBonus(0) == 0.f, "critChanceBonus(0) == 0");
    const float critBonusAtSix = critChanceBonus(6);
    check(critBonusAtSix > 0.0299f && critBonusAtSix < 0.0301f,
          "critChanceBonus(6) == 0.03 (3%) -- Thief's own starting Dexterity");
    const float critBonusAtHundred = critChanceBonus(100);
    check(critBonusAtHundred > 0.4999f && critBonusAtHundred < 0.5001f,
          "critChanceBonus(100) == 0.50 (50%) -- deliberately uncapped, unlike dodge");

    // --- critDamageMultiplier: a flat constant.
    check(critDamageMultiplier() > 1.499f && critDamageMultiplier() < 1.501f,
          "critDamageMultiplier() == 1.5");

    // --- statValueForScalingStat: picks out the right field.
    check(statValueForScalingStat(ScalingStat::Strength, 6, 2, 2) == 6,
          "statValueForScalingStat(Strength, ...) returns the strength argument");
    check(statValueForScalingStat(ScalingStat::Dexterity, 6, 2, 2) == 2,
          "statValueForScalingStat(Dexterity, ...) returns the dexterity argument");
    check(statValueForScalingStat(ScalingStat::Intelligence, 2, 2, 6) == 6,
          "statValueForScalingStat(Intelligence, ...) returns the intelligence argument");

    // --- didDodge: pure comparison, roll < chance -- unchanged by the
    // redesign, same semantics as before.
    check(didDodge(0.3f, 0.29f), "didDodge(0.3, 0.29) == true (roll just under the chance)");
    check(!didDodge(0.3f, 0.31f), "didDodge(0.3, 0.31) == false (roll just over the chance)");
    check(!didDodge(0.3f, 0.3f), "didDodge(0.3, 0.3) == false (an exact tie does not count as a dodge)");
    check(!didDodge(0.f, 0.f), "didDodge(0, 0) == false (zero chance never dodges, even at roll 0)");
    check(didDodge(1.f, 0.f), "didDodge(1.0, 0.0) == true (a theoretical 100% chance always dodges)");

    std::cout << "\n"
              << (g_allOk ? "All attribute formula checks passed." : "Some checks FAILED.") << '\n';
    return g_allOk ? 0 : 1;
}
