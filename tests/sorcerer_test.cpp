// Standalone sanity check for the Sorcerer's talent kit
// (SorcererTalents) and PlayerClassFactory -- hand-computed expected
// results, no SFML, no window, no Application.

#include <iostream>
#include <memory>
#include <string>

#include "ai/NullAIBehavior.hpp"
#include "entities/Monster.hpp"
#include "entities/PlayerClassFactory.hpp"
#include "entities/SorcererTalents.hpp"
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
    // --- PlayerClassFactory: statsForClass(Sorcerer).
    const Stats sorcererStats = statsForClass(PlayerClass::Sorcerer);
    check(sorcererStats.strength == 6,
          "Sorcerer strength == 6 (a true dump stat -- unlike the Thief, nothing here "
          "needs a moderate secondary attribute for damage)");
    check(sorcererStats.dexterity == 8, "Sorcerer dexterity == 8 (below baseline -- 0% dodge)");
    check(sorcererStats.intelligence == 24,
          "Sorcerer intelligence == 24 (the dominant stat, covering both damage and mana)");
    check(sorcererStats.maxHp == 22,
          "Sorcerer maxHp == 22 -- the lowest of any class (Spellblade 30, Fighter 45, "
          "Thief 24), the purest glass cannon of the three base classes");
    check(sorcererStats.maxMana == 28,
          "Sorcerer maxMana == 28 base 14 + manaBonusFromIntelligence(24) == 14 -- the "
          "largest pool of any class by a wide margin");
    check(sorcererStats.mana == sorcererStats.maxMana, "Sorcerer starts at full mana");
    check(sorcererStats.hp == sorcererStats.maxHp, "Sorcerer starts at full hp");

    // --- PlayerClassFactory: talentSetForClass(Sorcerer).
    const TalentSet sorcererTalentSet = talentSetForClass(PlayerClass::Sorcerer);
    check(sorcererTalentSet.knownTalents().size() == 4,
          "talentSetForClass(Sorcerer) carries exactly 4 talents");

    // --- sorcererTalents(): the data table itself.
    const std::vector<Talent> talents = sorcererTalents();
    check(talents.size() == 4, "sorcererTalents() returns exactly 4 talents");

    // An attacker with the Sorcerer's own real configured stats.
    Monster attacker(MonsterType::Goblin, "SorcererAttacker", '@', Position{0, 0}, sorcererStats,
                      std::make_unique<NullAIBehavior>());

    // Index 0: Arcane Bolt -- ranged, base 2 + intelligence bonus 7 == 9.
    constexpr std::size_t kArcaneBolt = 0;
    check(talents[kArcaneBolt].targeting == TargetingMode::RangedEnemyInSight,
          "Arcane Bolt is ranged -- no adjacency needed");
    check(talents[kArcaneBolt].damageType == DamageType::Magic,
          "Arcane Bolt is Magic-typed -- Intelligence is this class's whole identity, "
          "not a stat to work around");

    Stats boltTargetStats;
    boltTargetStats.hp = 20;
    boltTargetStats.maxHp = 20; // dexterity left at the 10 baseline -- guaranteed to land
    Monster boltTarget(MonsterType::Goblin, "BoltTarget", 'q', Position{0, 0}, boltTargetStats,
                        std::make_unique<NullAIBehavior>());
    const bool boltHit = applyTalentDamage(talents[kArcaneBolt], attacker, boltTarget);
    check(boltHit, "Arcane Bolt lands against a 0%-dodge target (guaranteed, not flaky)");
    check(boltTarget.stats().hp == 11,
          "Arcane Bolt deals base 2 + intelligence bonus 7 == 9 damage (20 -> 11)");

    // Index 1: Arcane Storm -- AreaAroundTarget, base 6 + intelligence bonus 7 == 13 per enemy.
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
    check(stormTarget.stats().hp == 7,
          "Arcane Storm deals base 6 + intelligence bonus 7 == 13 damage per enemy hit (20 -> 7)");

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
    // + intelligence bonus 7 == 10, plus a real chance to Stun.
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
    check(shatterTarget.stats().hp == 10,
          "Mind Shatter deals base 3 + intelligence bonus 7 == 10 damage (20 -> 10)");

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
              << (g_allOk ? "All Sorcerer checks passed." : "Some Sorcerer checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
