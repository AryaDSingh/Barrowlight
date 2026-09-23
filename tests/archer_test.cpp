// Standalone sanity check for the Archer's talent kit (ArcherTalents)
// and PlayerClassFactory -- hand-computed expected results, no SFML, no
// window, no Application. The retreat *direction/destination* math for
// Vault Kick lives in Application::tryUseTalent (it needs
// resolveBlinkDestination, an Application method), so it isn't
// reachable from here -- this file checks the data (retreatDistance is
// set correctly, targeting/shape are right) and the ordinary damage
// path Vault Kick shares with every other Damage-kind talent; the
// retreat itself is verified live (see ARCHITECTURE_DECISIONS.md).

#include <iostream>
#include <memory>
#include <string>

#include "ai/NullAIBehavior.hpp"
#include "entities/ArcherTalents.hpp"
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
} // namespace

int main() {
    // --- PlayerClassFactory: statsForClass(Archer).
    const Stats archerStats = statsForClass(PlayerClass::Archer);
    check(archerStats.strength == 14, "Archer strength == 14 (moderate, not a dump stat)");
    check(archerStats.dexterity == 20,
          "Archer dexterity == 20 (hits the 30% dodge cap exactly -- the maximum "
          "possible evasion in the game)");
    check(archerStats.intelligence == 6,
          "Archer intelligence == 6 (below baseline -- a real, if modest, penalty "
          "reinforcing \"not a caster\")");
    check(archerStats.maxHp == 24,
          "Archer maxHp == 24 -- the lowest of any class (Spellblade 30, Marauder 45), "
          "survives via dodge instead of hp");
    check(archerStats.maxMana == 8,
          "Archer maxMana == 8 base 8 + manaBonusFromIntelligence(6) == 0 (floored) -- "
          "the smallest pool of any class");
    check(archerStats.mana == archerStats.maxMana, "Archer starts at full mana");
    check(archerStats.hp == archerStats.maxHp, "Archer starts at full hp");

    // --- PlayerClassFactory: talentSetForClass(Archer).
    const TalentSet archerTalentSet = talentSetForClass(PlayerClass::Archer);
    check(archerTalentSet.knownTalents().size() == 4,
          "talentSetForClass(Archer) carries exactly 4 talents");

    // --- archerTalents(): the data table itself.
    const std::vector<Talent> talents = archerTalents();
    check(talents.size() == 4, "archerTalents() returns exactly 4 talents");

    // An attacker with the Archer's own real configured stats.
    Monster attacker(MonsterType::Goblin, "ArcherAttacker", '@', Position{0, 0}, archerStats,
                      std::make_unique<NullAIBehavior>());

    // Index 0: Quick Shot -- ranged, base 6 + strength bonus 2 == 8.
    constexpr std::size_t kQuickShot = 0;
    check(talents[kQuickShot].targeting == TargetingMode::RangedEnemyInSight,
          "Quick Shot is ranged -- no adjacency needed");
    check(talents[kQuickShot].damageType == DamageType::Physical,
          "Quick Shot is Physical, not Magic -- Intelligence is a penalty for this class, "
          "not a resource to spend");

    Stats quickShotTargetStats;
    quickShotTargetStats.hp = 20;
    quickShotTargetStats.maxHp = 20; // dexterity left at the 10 baseline -- guaranteed to land
    Monster quickShotTarget(MonsterType::Goblin, "QuickShotTarget", 'q', Position{0, 0},
                             quickShotTargetStats, std::make_unique<NullAIBehavior>());
    const bool quickShotHit = applyTalentDamage(talents[kQuickShot], attacker, quickShotTarget);
    check(quickShotHit, "Quick Shot lands against a 0%-dodge target (guaranteed, not flaky)");
    check(quickShotTarget.stats().hp == 12,
          "Quick Shot deals base 6 + strength bonus 2 == 8 damage (20 -> 12)");

    // Index 1: Volley -- AreaAroundTarget, base 6 + strength bonus 2 == 8 per enemy.
    constexpr std::size_t kVolley = 1;
    check(talents[kVolley].shape == EffectShape::AreaAroundTarget,
          "Volley is AreaAroundTarget-shaped -- hits a cluster, not one target");
    check(talents[kVolley].areaRadius == 2, "Volley's radius is 2, matching Fireball's own");

    Stats volleyTargetStats;
    volleyTargetStats.hp = 20;
    volleyTargetStats.maxHp = 20;
    Monster volleyTarget(MonsterType::Goblin, "VolleyTarget", 'v', Position{0, 0},
                          volleyTargetStats, std::make_unique<NullAIBehavior>());
    applyTalentDamage(talents[kVolley], attacker, volleyTarget);
    check(volleyTarget.stats().hp == 12,
          "Volley deals base 6 + strength bonus 2 == 8 damage per enemy hit (20 -> 12)");

    // Index 2: Steady Aim -- SelfBuff, applies Empowered.
    constexpr std::size_t kSteadyAim = 2;
    check(talents[kSteadyAim].effectKind == TalentEffectKind::SelfBuff,
          "Steady Aim is flagged effectKind::SelfBuff");
    check(talents[kSteadyAim].targeting == TargetingMode::Self &&
              talents[kSteadyAim].shape == EffectShape::SingleTarget,
          "Steady Aim is Self-targeted, SingleTarget shape (same caster-resolution as "
          "Renewal and Rallying Cry)");
    check(talents[kSteadyAim].selfBuffEffect.has_value() &&
              talents[kSteadyAim].selfBuffEffect->type == StatusEffectType::Empowered,
          "Steady Aim's selfBuffEffect applies Empowered -- the same status effect "
          "Rallying Cry and the boss's own enrage both already use");

    // Index 3: Vault Kick -- the signature move. Data-level checks only
    // here (targeting, shape, retreatDistance, and the ordinary damage
    // math it shares with every ranged/melee Damage talent); the actual
    // retreat destination requires Application::resolveBlinkDestination
    // and is verified live instead (see ARCHITECTURE_DECISIONS.md,
    // "Archer and the vault mechanic").
    constexpr std::size_t kVaultKick = 3;
    check(talents[kVaultKick].targeting == TargetingMode::AdjacentEnemy,
          "Vault Kick requires melee range -- the whole point is escaping it");
    check(talents[kVaultKick].retreatDistance == 3,
          "Vault Kick's retreatDistance is 3 -- the only talent in either class's kit "
          "that moves the caster after a Damage-kind hit");

    Stats vaultTargetStats;
    vaultTargetStats.hp = 20;
    vaultTargetStats.maxHp = 20;
    Monster vaultTarget(MonsterType::Goblin, "VaultTarget", 'k', Position{0, 0}, vaultTargetStats,
                         std::make_unique<NullAIBehavior>());
    applyTalentDamage(talents[kVaultKick], attacker, vaultTarget);
    check(vaultTarget.stats().hp == 14,
          "Vault Kick deals base 4 + strength bonus 2 == 6 damage (20 -> 14) -- deliberately "
          "modest, since the escape is the point, not the hit");

    // Sanity check that no other talent in this kit accidentally carries
    // a nonzero retreatDistance -- the mechanic should be exclusive to
    // Vault Kick, not a stray default leaking in.
    check(talents[kQuickShot].retreatDistance == 0 && talents[kVolley].retreatDistance == 0 &&
              talents[kSteadyAim].retreatDistance == 0,
          "no other Archer talent carries a nonzero retreatDistance");

    std::cout << "\n" << (g_allOk ? "All Archer checks passed." : "Some Archer checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
