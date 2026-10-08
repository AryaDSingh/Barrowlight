// The talent engine's damage maths, checked against the Thief fixture kit
// (tests/fixtures/) with hand-computed expected results: no SFML, no
// window, no Application. Vault Kick's retreat needs the map and lives in
// Application, so here only its data and its ordinary damage are checked.
// See warrior_test.cpp for why each damage check accepts "normal or crit".

#include <iostream>
#include <memory>
#include <string>

#include "ai/NullAIBehavior.hpp"
#include "fixtures/ThiefTalents.hpp"
#include "entities/Monster.hpp"
#include "entities/PlayerClassFactory.hpp"
#include "fixtures/ClassKits.hpp"
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
    // --- PlayerClassFactory: statsForClass(Thief).
    const Stats thiefStats = statsForClass(PlayerClass::Thief);
    check(thiefStats.strength == 2, "Thief strength == 2 (a true dump stat)");
    check(thiefStats.dexterity == 6, "Thief dexterity == 6 (the dominant stat)");
    check(thiefStats.intelligence == 2, "Thief intelligence == 2 (a true dump stat)");
    check(thiefStats.maxHp == 25, "Thief maxHp == 25 (hand-picked, not derived from any attribute)");
    check(thiefStats.maxMana == 15, "Thief maxMana == 15 (hand-picked, not derived from Intelligence)");
    check(thiefStats.mana == thiefStats.maxMana, "Thief starts at full mana");
    check(thiefStats.hp == thiefStats.maxHp, "Thief starts at full hp");

    // --- PlayerClassFactory: talentSetForClass(Thief).
    const TalentSet thiefTalentSet = talentSetForClass(PlayerClass::Thief);
    check(thiefTalentSet.knownTalents().size() == 4,
          "talentSetForClass(Thief) carries exactly 4 talents");

    // --- thiefTalents(): the data table itself.
    const std::vector<Talent> talents = thiefTalents();
    check(talents.size() == 4, "thiefTalents() returns exactly 4 talents");

    // An attacker with the Thief's own real configured stats. Note
    // Strength (2) is low enough that its ability-damage bonus is 0 at
    // every tier up through Power (2/5 truncates to 0 even at the 2.0x
    // multiplier) -- "bows stay Strength-based for damage" per the
    // redesign, but a Thief investing nothing further into Strength
    // gets none of the scaling bonus from it, only the flat base power.
    Monster attacker(MonsterType::Goblin, "ThiefAttacker", '@', Position{0, 0}, thiefStats,
                      std::make_unique<NullAIBehavior>());

    // Index 0: Quick Shot -- ranged, base 6 + strength bonus (2/5 * 1.0,
    // truncated to 0) == 6.
    constexpr std::size_t kQuickShot = 0;
    check(talents[kQuickShot].targeting == TargetingMode::RangedEnemyInSight,
          "Quick Shot is ranged -- no adjacency needed");
    check(talents[kQuickShot].scalingStat == ScalingStat::Strength,
          "Quick Shot scales from Strength -- bows stay Strength-based for damage");

    Stats quickShotTargetStats;
    quickShotTargetStats.hp = 20;
    quickShotTargetStats.maxHp = 20; // dexterity defaults to 0 now -- guaranteed to land
    Monster quickShotTarget(MonsterType::Goblin, "QuickShotTarget", 'q', Position{0, 0},
                             quickShotTargetStats, std::make_unique<NullAIBehavior>());
    const bool quickShotHit = applyTalentDamage(talents[kQuickShot], attacker, quickShotTarget);
    check(quickShotHit, "Quick Shot lands against a 0%-dodge target (guaranteed, not flaky)");
    check(matchesNormalOrCrit(20 - quickShotTarget.stats().hp, 6),
          "Quick Shot deals base 6 + strength bonus 0 == 6 damage (or 9 on a crit)");

    // Index 1: Volley -- AreaAroundTarget, base 6 + strength bonus (2/5
    // * 2.0, truncated to 0) == 6 per enemy.
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
    check(matchesNormalOrCrit(20 - volleyTarget.stats().hp, 6),
          "Volley deals base 6 + strength bonus 0 == 6 damage per enemy hit (or 9 on a crit)");

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
    // retreat destination requires Application::resolveBlinkDestination,
    // which needs the map. Base 4 + strength bonus (2/5 *
    // 2.0, truncated to 0) == 4.
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
    check(matchesNormalOrCrit(20 - vaultTarget.stats().hp, 4),
          "Vault Kick deals base 4 + strength bonus 0 == 4 damage (or 6 on a crit) -- "
          "deliberately modest, since the escape is the point, not the hit");

    // Sanity check that no other talent in this kit accidentally carries
    // a nonzero retreatDistance -- the mechanic should be exclusive to
    // Vault Kick, not a stray default leaking in.
    check(talents[kQuickShot].retreatDistance == 0 && talents[kVolley].retreatDistance == 0 &&
              talents[kSteadyAim].retreatDistance == 0,
          "no other Thief talent carries a nonzero retreatDistance");

    std::cout << "\n" << (g_allOk ? "All Thief checks passed." : "Some Thief checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
