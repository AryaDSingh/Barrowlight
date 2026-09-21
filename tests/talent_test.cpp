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

    check(talents.size() == 9, "spellbladeTalents() returns exactly 9 talents "
                                "(the original 8 plus Renewal, a player-requested addition)");

    int bladeCount = 0;
    int flameCount = 0;
    for (const Talent& t : talents) {
        if (t.tree == TalentTree::Blade) {
            ++bladeCount;
        } else if (t.tree == TalentTree::Flame) {
            ++flameCount;
        }
    }
    check(bladeCount == 4 && flameCount == 5, "4 Blade talents, 5 Flame talents (Renewal joined "
                                               "the Flame tree, matching its Magic scaling)");

    TalentSet talentSet(talents);

    // Attacker stats mirror the Spellblade's real configuration
    // (Application's constructor): strength 14, intelligence 18. Using
    // the actual configured values, not arbitrary test numbers, means
    // these checks double as a direct verification that the Prompt 14
    // recalibration (base power reduced by exactly the new attribute
    // bonus) reproduces the exact damage numbers tuned back in Prompt 9
    // -- if either the formula or the recalibrated base power were
    // wrong, these wouldn't land on the same values they did before.
    Stats attackerStats;
    attackerStats.strength = 14;
    attackerStats.dexterity = 10;
    attackerStats.intelligence = 18;
    Monster attacker(MonsterType::Goblin, "Attacker", '@', Position{0, 0}, attackerStats,
                      std::make_unique<NullAIBehavior>());

    // Index 0: Quick Strike -- power 6, cooldown 1.
    constexpr std::size_t kQuickStrike = 0;
    check(talentSet.isReady(kQuickStrike), "Quick Strike starts ready");

    Stats dummyStats;
    dummyStats.hp = 20;
    dummyStats.maxHp = 20; // dexterity left at the 10 baseline -- 0% dodge, so this is
                            // guaranteed to land, not a flaky test depending on an RNG roll
    Monster dummy(MonsterType::Goblin, "Dummy", 'd', Position{0, 0}, dummyStats,
                  std::make_unique<NullAIBehavior>());

    const bool quickStrikeHit = applyTalentDamage(talents[kQuickStrike], attacker, dummy);
    check(quickStrikeHit, "Quick Strike lands against a 0%-dodge target (guaranteed, not flaky)");
    check(dummy.stats().hp == 14,
          "Quick Strike deals base 4 + strength bonus 2 == 6 damage (20 -> 14), "
          "matching the original Prompt 9 tuned value exactly");

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
    // The attribute bonus is folded in *before* the conditional
    // multiplier applies (see TalentEffects.cpp): (8 base + 2 strength)
    // * 3 == 30, not 8*3 + 2 == 26 -- an execute is meant to amplify the
    // attacker's full output, not just the talent's flat base number.
    constexpr std::size_t kExecution = 3;

    Stats healthyStats;
    healthyStats.hp = 20;
    healthyStats.maxHp = 20; // 100% hp -- above the threshold
    Monster healthyTarget(MonsterType::Goblin, "Healthy", 'h', Position{0, 0}, healthyStats,
                           std::make_unique<NullAIBehavior>());
    applyTalentDamage(talents[kExecution], attacker, healthyTarget);
    check(healthyTarget.stats().hp == 10,
          "Execution deals base 8 + strength bonus 2 == 10 damage above the hp threshold "
          "(20 -> 10), matching the original Prompt 9 tuned value exactly");

    Stats lowHpStats;
    lowHpStats.hp = 5;
    lowHpStats.maxHp = 20; // 25% hp -- at/below the 30% threshold
    Monster lowHpTarget(MonsterType::Goblin, "Weakened", 'w', Position{0, 0}, lowHpStats,
                         std::make_unique<NullAIBehavior>());
    applyTalentDamage(talents[kExecution], attacker, lowHpTarget);
    check(lowHpTarget.stats().hp == 5 - 30,
          "Execution deals tripled (8+2)*3 == 30 damage at/below the hp threshold (5 -> -25), "
          "the attribute bonus folded in before the multiplier, matching the original "
          "Prompt 9 tuned value exactly");

    // Ember Bolt (index 4, Magic-typed, base 3) -- verifies the
    // Intelligence bonus specifically, and that it genuinely scales with
    // the attacker rather than being a fixed pass-through: the same
    // attacker (intelligence 18) deals 3+4==7, matching the original
    // tuned value, while a baseline-intelligence attacker deals only the
    // base 3, no bonus at all.
    constexpr std::size_t kEmberBolt = 4;
    Stats emberTargetStats;
    emberTargetStats.hp = 20;
    emberTargetStats.maxHp = 20;
    Monster emberTarget(MonsterType::Goblin, "EmberTarget", 'e', Position{0, 0}, emberTargetStats,
                         std::make_unique<NullAIBehavior>());
    applyTalentDamage(talents[kEmberBolt], attacker, emberTarget);
    check(emberTarget.stats().hp == 13,
          "Ember Bolt deals base 3 + intelligence bonus 4 == 7 damage (20 -> 13), matching "
          "the original Prompt 9 tuned value exactly");

    Stats baselineAttackerStats; // every attribute left at the 10 baseline -- every bonus is 0
    Monster baselineAttacker(MonsterType::Goblin, "BaselineAttacker", '@', Position{0, 0},
                              baselineAttackerStats, std::make_unique<NullAIBehavior>());
    Stats emberTarget2Stats;
    emberTarget2Stats.hp = 20;
    emberTarget2Stats.maxHp = 20;
    Monster emberTarget2(MonsterType::Goblin, "EmberTarget2", 'e', Position{0, 0},
                          emberTarget2Stats, std::make_unique<NullAIBehavior>());
    applyTalentDamage(talents[kEmberBolt], baselineAttacker, emberTarget2);
    check(emberTarget2.stats().hp == 17,
          "the same talent from a baseline-intelligence attacker deals only its base 3 "
          "damage (20 -> 17), no bonus -- confirms the bonus genuinely scales with the "
          "attacker's own stats rather than being a fixed pass-through");

    // Index 8: Renewal -- heals instead of damaging. base 8 + intelligence
    // bonus 4 (same attacker as above, intelligence 18) == 12 hp restored.
    constexpr std::size_t kRenewal = 8;
    check(talents[kRenewal].effectKind == TalentEffectKind::Heal,
          "Renewal is flagged effectKind::Heal (so tryUseTalent routes it to "
                                     "applyTalentHeal, not applyTalentDamage)");
    check(talents[kRenewal].targeting == TargetingMode::Self &&
              talents[kRenewal].shape == EffectShape::SingleTarget,
          "Renewal is Self-targeted, SingleTarget shape (resolves to the caster directly, "
          "not AreaAroundSelf's \"nearby enemies\" search)");

    Stats woundedStats;
    woundedStats.hp = 10;
    woundedStats.maxHp = 30;
    Monster wounded(MonsterType::Goblin, "Wounded", 'w', Position{0, 0}, woundedStats,
                     std::make_unique<NullAIBehavior>());
    applyTalentHeal(talents[kRenewal], attacker, wounded);
    check(wounded.stats().hp == 22,
          "Renewal restores base 8 + intelligence bonus 4 == 12 hp (10 -> 22)");

    Stats nearFullStats;
    nearFullStats.hp = 25;
    nearFullStats.maxHp = 30; // only 5 hp of headroom -- less than the 12 Renewal would give
    Monster nearFull(MonsterType::Goblin, "NearFull", 'n', Position{0, 0}, nearFullStats,
                      std::make_unique<NullAIBehavior>());
    applyTalentHeal(talents[kRenewal], attacker, nearFull);
    check(nearFull.stats().hp == 30,
          "Renewal caps at maxHp rather than overhealing past it (25 -> 30, not 37)");

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
