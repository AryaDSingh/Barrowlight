// Standalone sanity check for the talent system: Talent (data),
// TalentSet (cooldown bookkeeping), and applyTalentDamage (effect
// application). No SFML, no window, no Application -- just the pieces
// directly, against hand-computed expected results.

#include <iostream>
#include <memory>
#include <string>

#include "ai/NullAIBehavior.hpp"
#include "entities/Monster.hpp"
#include "entities/SpellbladeTalents.hpp"
#include "entities/TalentEffects.hpp"

using namespace engine;

namespace {
bool g_allOk = true;

void check(bool condition, const std::string& description) {
    g_allOk &= condition;
    std::cout << (condition ? "[ok] " : "[FAIL] ") << description << '\n';
}
} // namespace

int main() {
    const std::vector<Talent> talents = spellbladeTalents();

    check(talents.size() == 8, "spellbladeTalents() returns exactly 8 talents");

    int bladeCount = 0;
    int flameCount = 0;
    for (const Talent& t : talents) {
        if (t.tree == TalentTree::Blade) {
            ++bladeCount;
        } else if (t.tree == TalentTree::Flame) {
            ++flameCount;
        }
    }
    check(bladeCount == 4 && flameCount == 4, "4 Blade talents, 4 Flame talents");

    TalentSet talentSet(talents);

    // Index 0: Quick Strike -- power 6, cooldown 1.
    constexpr std::size_t kQuickStrike = 0;
    check(talentSet.isReady(kQuickStrike), "Quick Strike starts ready");

    Stats dummyStats;
    dummyStats.hp = 20;
    dummyStats.maxHp = 20;
    Monster dummy("Dummy", 'd', Position{0, 0}, dummyStats, std::make_unique<NullAIBehavior>());

    applyTalentDamage(talents[kQuickStrike], dummy);
    check(dummy.stats().hp == 14, "Quick Strike deals exactly 6 damage (20 -> 14)");

    talentSet.startCooldown(kQuickStrike);
    check(!talentSet.isReady(kQuickStrike), "Quick Strike goes on cooldown after use");
    check(talentSet.cooldownRemaining(kQuickStrike) == 1,
          "cooldown remaining equals the talent's cooldownTurns (1)");

    talentSet.tickCooldowns();
    check(talentSet.isReady(kQuickStrike), "Quick Strike ready again after 1 tick (cooldown 1)");

    // Index 1: Power Strike -- cooldown 4. Verify it takes exactly 4 ticks.
    constexpr std::size_t kPowerStrike = 1;
    talentSet.startCooldown(kPowerStrike);
    for (int i = 1; i <= 3; ++i) {
        talentSet.tickCooldowns();
        check(!talentSet.isReady(kPowerStrike),
              "Power Strike still on cooldown after tick " + std::to_string(i) + "/4");
    }
    talentSet.tickCooldowns();
    check(talentSet.isReady(kPowerStrike), "Power Strike ready after the 4th tick");

    // Index 3: Execution -- power 10, triples (30) at/below 30% target hp.
    constexpr std::size_t kExecution = 3;

    Stats healthyStats;
    healthyStats.hp = 20;
    healthyStats.maxHp = 20; // 100% hp -- above the threshold
    Monster healthyTarget("Healthy", 'h', Position{0, 0}, healthyStats,
                           std::make_unique<NullAIBehavior>());
    applyTalentDamage(talents[kExecution], healthyTarget);
    check(healthyTarget.stats().hp == 10,
          "Execution deals normal 10 damage above the hp threshold (20 -> 10)");

    Stats lowHpStats;
    lowHpStats.hp = 5;
    lowHpStats.maxHp = 20; // 25% hp -- at/below the 30% threshold
    Monster lowHpTarget("Weakened", 'w', Position{0, 0}, lowHpStats,
                         std::make_unique<NullAIBehavior>());
    applyTalentDamage(talents[kExecution], lowHpTarget);
    check(lowHpTarget.stats().hp == 5 - 30,
          "Execution deals tripled 30 damage at/below the hp threshold (5 -> -25)");

    // Index 2: Reckless Lunge -- costs the caster's own hp as well as
    // mana. applyTalentDamage only touches the target; the hp-cost
    // deduction is the caller's (Application's) job, so this just
    // confirms the data carries the cost, not Application's logic.
    check(talents[2].hpCost == 6, "Reckless Lunge's data specifies an hp cost (caller applies it)");

    std::cout << "\n"
              << (g_allOk ? "All talent system checks passed." : "Some talent checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
