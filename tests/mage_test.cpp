// Standalone sanity check for the Mage's talent kit (MageTalents) and
// PlayerClassFactory -- hand-computed expected results, no SFML, no
// window, no Application.
//
// Rewritten for the attribute-system redesign -- see warrior_test.cpp's
// own header comment for why every damage check now verifies "normal
// or crit" rather than a single exact value.

#include <iostream>
#include <memory>
#include <string>

#include "ai/NullAIBehavior.hpp"
#include "entities/MageTalents.hpp"
#include "entities/Monster.hpp"
#include "entities/PlayerClassFactory.hpp"
#include "entities/TalentEffects.hpp"

using namespace engine;

namespace {
bool g_allOk = true;

void check(bool condition, const std::string& description) {
    g_allOk &= condition;
    std::cout << (condition ? "[ok] " : "[FAIL] ") << description << '\n';
}

bool matchesNormalOrCrit(int actualDamage, int normalDamage) {
    const int critDamage = static_cast<int>(static_cast<float>(normalDamage) * 1.5f);
    return actualDamage == normalDamage || actualDamage == critDamage;
}
} // namespace

int main() {
    // --- PlayerClassFactory: statsForClass(Mage).
    const Stats mageStats = statsForClass(PlayerClass::Mage);
    check(mageStats.strength == 2, "Mage strength == 2 (a true dump stat)");
    check(mageStats.dexterity == 2, "Mage dexterity == 2 (a true dump stat)");
    check(mageStats.intelligence == 6, "Mage intelligence == 6 (the dominant stat)");
    check(mageStats.maxHp == 20, "Mage maxHp == 20 -- the lowest of the three base classes");
    check(mageStats.maxMana == 20,
          "Mage maxMana == 20 (hand-picked, not derived from Intelligence) -- the largest "
          "pool of any class");
    check(mageStats.mana == mageStats.maxMana, "Mage starts at full mana");
    check(mageStats.hp == mageStats.maxHp, "Mage starts at full hp");

    // --- PlayerClassFactory: talentSetForClass(Mage).
    const TalentSet mageTalentSet = talentSetForClass(PlayerClass::Mage);
    check(mageTalentSet.knownTalents().size() == 4,
          "talentSetForClass(Mage) carries exactly 4 talents");

    // --- mageTalents(): the data table itself.
    const std::vector<Talent> talents = mageTalents();
    check(talents.size() == 4, "mageTalents() returns exactly 4 talents");

    // An attacker with the Mage's own real configured stats.
    Monster attacker(MonsterType::Goblin, "MageAttacker", '@', Position{0, 0}, mageStats,
                      std::make_unique<NullAIBehavior>());

    // Index 0: Arcane Bolt -- ranged, base 2 + intelligence bonus
    // (6/5 * Filler-tier 1.0, truncated to 1) == 3.
    constexpr std::size_t kArcaneBolt = 0;
    check(talents[kArcaneBolt].targeting == TargetingMode::RangedEnemyInSight,
          "Arcane Bolt is ranged -- no adjacency needed");
    check(talents[kArcaneBolt].scalingStat == ScalingStat::Intelligence,
          "Arcane Bolt scales from Intelligence -- this class's whole identity");

    Stats boltTargetStats;
    boltTargetStats.hp = 20;
    boltTargetStats.maxHp = 20; // dexterity defaults to 0 now -- guaranteed to land
    Monster boltTarget(MonsterType::Goblin, "BoltTarget", 'q', Position{0, 0}, boltTargetStats,
                        std::make_unique<NullAIBehavior>());
    const bool boltHit = applyTalentDamage(talents[kArcaneBolt], attacker, boltTarget);
    check(boltHit, "Arcane Bolt lands against a 0%-dodge target (guaranteed, not flaky)");
    check(matchesNormalOrCrit(20 - boltTarget.stats().hp, 3),
          "Arcane Bolt deals base 2 + intelligence bonus 1 == 3 damage (or 4 on a crit)");

    // Index 1: Arcane Storm -- AreaAroundTarget, base 6 + intelligence
    // bonus (6/5 * Power-tier 2.0, truncated to 2) == 8 per enemy.
    constexpr std::size_t kArcaneStorm = 1;
    check(talents[kArcaneStorm].shape == EffectShape::AreaAroundTarget,
          "Arcane Storm is AreaAroundTarget-shaped -- hits a cluster, not one target");
    check(talents[kArcaneStorm].areaRadius == 2, "Arcane Storm's radius is 2, matching Fireball's own");

    Stats stormTargetStats;
    stormTargetStats.hp = 20;
    stormTargetStats.maxHp = 20;
    Monster stormTarget(MonsterType::Goblin, "StormTarget", 's', Position{0, 0}, stormTargetStats,
                         std::make_unique<NullAIBehavior>());
    applyTalentDamage(talents[kArcaneStorm], attacker, stormTarget);
    check(matchesNormalOrCrit(20 - stormTarget.stats().hp, 8),
          "Arcane Storm deals base 6 + intelligence bonus 2 == 8 damage per enemy hit (or "
          "12 on a crit)");

    // Index 2: Arcane Focus -- SelfBuff, applies Empowered.
    constexpr std::size_t kArcaneFocus = 2;
    check(talents[kArcaneFocus].effectKind == TalentEffectKind::SelfBuff,
          "Arcane Focus is flagged effectKind::SelfBuff");
    check(talents[kArcaneFocus].targeting == TargetingMode::Self &&
              talents[kArcaneFocus].shape == EffectShape::SingleTarget,
          "Arcane Focus is Self-targeted, SingleTarget shape (same caster-resolution as "
          "Renewal/Rallying Cry/Steady Aim)");
    check(talents[kArcaneFocus].selfBuffEffect.has_value() &&
              talents[kArcaneFocus].selfBuffEffect->type == StatusEffectType::Empowered,
          "Arcane Focus's selfBuffEffect applies Empowered -- the same status effect "
          "every other class's self-buff and the boss's own enrage all already use");

    // Index 3: Mind Shatter -- the signature move. Modest damage, base 3
    // + intelligence bonus (6/5 * Power-tier 2.0, truncated to 2) == 5,
    // plus a real chance to Stun.
    constexpr std::size_t kMindShatter = 3;
    check(talents[kMindShatter].onHitEffect.has_value() &&
              talents[kMindShatter].onHitEffect->type == StatusEffectType::Stun,
          "Mind Shatter's onHitEffect applies Stun -- the first player talent to affect "
          "its target with a status effect rather than just the caster");
    check(talents[kMindShatter].onHitChance > 0.599f && talents[kMindShatter].onHitChance < 0.601f,
          "Mind Shatter's onHitChance is 0.6 -- real, not overwhelming, counterplay exists "
          "via the target's own dodge chance already gating whether the hit lands at all");

    Stats shatterTargetStats;
    shatterTargetStats.hp = 20;
    shatterTargetStats.maxHp = 20;
    Monster shatterTarget(MonsterType::Goblin, "ShatterTarget", 'm', Position{0, 0},
                           shatterTargetStats, std::make_unique<NullAIBehavior>());
    applyTalentDamage(talents[kMindShatter], attacker, shatterTarget);
    check(matchesNormalOrCrit(20 - shatterTarget.stats().hp, 5),
          "Mind Shatter deals base 3 + intelligence bonus 2 == 5 damage (or 7 on a crit)");

    // Deterministic coverage of the on-hit-effect *application* mechanism
    // itself (not the dice roll -- rollChance()'s actual draw is
    // deliberately untested in isolation, same precedent as rollDodge
    // and Chaser's original onHitChance roll). Forcing onHitChance to
    // 1.0 on a local copy eliminates randomness from this specific
    // check while still not unit-testing the RNG.
    Talent guaranteedShatter = talents[kMindShatter];
    guaranteedShatter.onHitChance = 1.f;
    Stats stunTargetStats;
    stunTargetStats.hp = 20;
    stunTargetStats.maxHp = 20;
    Monster stunTarget(MonsterType::Goblin, "StunTarget", 'x', Position{0, 0}, stunTargetStats,
                        std::make_unique<NullAIBehavior>());
    check(!stunTarget.statusEffects().has(StatusEffectType::Stun),
          "stun target starts without Stun (sanity check before applying it)");
    applyTalentDamage(guaranteedShatter, attacker, stunTarget);
    check(stunTarget.statusEffects().has(StatusEffectType::Stun),
          "a guaranteed (100% chance) Mind Shatter applies Stun to the target");

    std::cout << "\n"
              << (g_allOk ? "All Mage checks passed." : "Some Mage checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
