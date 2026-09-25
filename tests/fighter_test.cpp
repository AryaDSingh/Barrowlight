// Standalone sanity check for the Warrior's talent kit
// (FighterTalents -- file/function names not yet renamed to match,
// a known, deliberate simplification for this pass; see
// ARCHITECTURE_DECISIONS.md) and PlayerClassFactory -- hand-computed
// expected results, no SFML, no window, no Application.
//
// Rewritten for the attribute-system redesign: renamed class, new
// hand-picked stats, and the new tiered damage formula. Every damage
// check now also has to account for global crit (a flat 5% base
// chance, always active, plus a small Dexterity-derived bonus) -- a
// single exact `==` assertion would be genuinely flaky, since roughly
// 1 in 17 real runs would land an unexpected crit on any given hit.
// Each check instead verifies the result is *one of* the two possible
// values (normal or critical), which stays deterministic while still
// meaningfully validating the damage math.

#include <iostream>
#include <memory>
#include <string>

#include "ai/NullAIBehavior.hpp"
#include "entities/FighterTalents.hpp"
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

// A damage result is valid if it matches either the normal hit or the
// (1.5x, truncated) critical hit -- see this file's own header comment
// for why an exact `==` would be flaky now that crit is global.
bool matchesNormalOrCrit(int actualDamage, int normalDamage) {
    const int critDamage = static_cast<int>(static_cast<float>(normalDamage) * 1.5f);
    return actualDamage == normalDamage || actualDamage == critDamage;
}
} // namespace

int main() {
    // --- PlayerClassFactory: statsForClass(Warrior).
    const Stats warriorStats = statsForClass(PlayerClass::Warrior);
    check(warriorStats.strength == 6, "Warrior strength == 6 (pure Strength identity)");
    check(warriorStats.dexterity == 2, "Warrior dexterity == 2 (a true dump stat)");
    check(warriorStats.intelligence == 2, "Warrior intelligence == 2 (a true dump stat)");
    check(warriorStats.maxHp == 30, "Warrior maxHp == 30 (hand-picked, not derived from Strength)");
    check(warriorStats.maxMana == 10, "Warrior maxMana == 10 (hand-picked, not derived from Intelligence)");
    check(warriorStats.mana == warriorStats.maxMana, "Warrior starts at full mana");
    check(warriorStats.hp == warriorStats.maxHp, "Warrior starts at full hp");

    // --- PlayerClassFactory: talentSetForClass(Warrior) round-trips
    // the same talents fighterTalents() returns directly.
    const TalentSet warriorTalentSet = talentSetForClass(PlayerClass::Warrior);
    check(warriorTalentSet.knownTalents().size() == 4,
          "talentSetForClass(Warrior) carries exactly 4 talents");

    // --- fighterTalents(): the data table itself.
    const std::vector<Talent> talents = fighterTalents();
    check(talents.size() == 4, "fighterTalents() returns exactly 4 talents");

    // An attacker with the Warrior's own real configured stats.
    Monster attacker(MonsterType::Goblin, "WarriorAttacker", '@', Position{0, 0}, warriorStats,
                      std::make_unique<NullAIBehavior>());

    // Index 0: Slam -- zero mana cost, base 4 + strength bonus (6/5 * Filler-tier 1.0,
    // truncated to 1) == 5.
    constexpr std::size_t kSlam = 0;
    check(talents[kSlam].manaCost == 0, "Slam costs zero mana -- the Warrior's free filler");
    check(talents[kSlam].cooldownTurns == 1, "Slam has only a 1-turn cooldown -- spammable");

    Stats slamTargetStats;
    slamTargetStats.hp = 20;
    slamTargetStats.maxHp = 20; // dexterity defaults to 0 now -- guaranteed to land
    Monster slamTarget(MonsterType::Goblin, "SlamTarget", 's', Position{0, 0}, slamTargetStats,
                        std::make_unique<NullAIBehavior>());
    const bool slamHit = applyTalentDamage(talents[kSlam], attacker, slamTarget);
    check(slamHit, "Slam lands against a 0%-dodge target (guaranteed, not flaky)");
    check(matchesNormalOrCrit(20 - slamTarget.stats().hp, 5),
          "Slam deals base 4 + strength bonus 1 == 5 damage (or 7 on a crit)");

    // Index 1: Cleave -- AreaAroundSelf, base 5 + strength bonus (6/5 *
    // Core-tier 1.5, truncated to 1) == 6 per enemy.
    constexpr std::size_t kCleave = 1;
    check(talents[kCleave].shape == EffectShape::AreaAroundSelf,
          "Cleave is AreaAroundSelf-shaped -- hits everything adjacent, not one target");
    check(talents[kCleave].areaRadius == 1, "Cleave's radius is 1 -- adjacent tiles only");

    Stats cleaveTargetStats;
    cleaveTargetStats.hp = 20;
    cleaveTargetStats.maxHp = 20;
    Monster cleaveTarget(MonsterType::Goblin, "CleaveTarget", 'c', Position{0, 0},
                          cleaveTargetStats, std::make_unique<NullAIBehavior>());
    applyTalentDamage(talents[kCleave], attacker, cleaveTarget);
    check(matchesNormalOrCrit(20 - cleaveTarget.stats().hp, 6),
          "Cleave deals base 5 + strength bonus 1 == 6 damage per enemy hit (or 9 on a crit)");

    // Index 2: Rallying Cry -- SelfBuff, applies Empowered to the caster
    // directly via applyTalentSelfBuff, not applyTalentDamage.
    constexpr std::size_t kRallyingCry = 2;
    check(talents[kRallyingCry].effectKind == TalentEffectKind::SelfBuff,
          "Rallying Cry is flagged effectKind::SelfBuff");
    check(talents[kRallyingCry].targeting == TargetingMode::Self &&
              talents[kRallyingCry].shape == EffectShape::SingleTarget,
          "Rallying Cry is Self-targeted, SingleTarget shape (same caster-resolution as "
          "the Spellblade's Renewal)");
    check(talents[kRallyingCry].selfBuffEffect.has_value() &&
              talents[kRallyingCry].selfBuffEffect->type == StatusEffectType::Empowered,
          "Rallying Cry's selfBuffEffect applies Empowered -- the same status effect the "
          "boss's own enrage uses, reused rather than reinvented");

    Stats casterStats = warriorStats; // the Warrior's own real stats, not an arbitrary override
    Monster caster(MonsterType::Goblin, "Caster", 'c', Position{0, 0}, casterStats,
                    std::make_unique<NullAIBehavior>());
    check(!caster.statusEffects().has(StatusEffectType::Empowered),
          "caster starts without Empowered (sanity check before applying it)");
    applyTalentSelfBuff(talents[kRallyingCry], caster);
    check(caster.statusEffects().has(StatusEffectType::Empowered),
          "applyTalentSelfBuff grants the caster Empowered");
    check(caster.statusEffects().magnitudeOf(StatusEffectType::Empowered) == 4,
          "Empowered's magnitude matches the talent's configured selfBuffEffect (4)");

    // The exact bug live testing originally caught (Prompt 15): Rallying
    // Cry granting Empowered did nothing to the caster's own subsequent
    // damage, since applyTalentDamage never checked for it. Verify the
    // fix still holds: the same caster now empowered, using Slam, deals
    // its normal 5 damage *plus* the 4 from Empowered == 9.
    Stats empoweredSlamTargetStats;
    empoweredSlamTargetStats.hp = 20;
    empoweredSlamTargetStats.maxHp = 20;
    Monster empoweredSlamTarget(MonsterType::Goblin, "EmpoweredSlamTarget", 'e', Position{0, 0},
                                 empoweredSlamTargetStats, std::make_unique<NullAIBehavior>());
    applyTalentDamage(talents[kSlam], caster, empoweredSlamTarget);
    check(matchesNormalOrCrit(20 - empoweredSlamTarget.stats().hp, 9),
          "an Empowered caster's Slam deals base 4 + strength bonus 1 + Empowered 4 == 9 "
          "damage (or 13 on a crit), not just the un-buffed 5 -- the fix from the bug live "
          "testing originally caught still holds");

    // Index 3: Berserker's Fury -- costs hp, base 23 + strength bonus
    // (6/5 * Power-tier 2.0, truncated to 2) == 25, the hardest-hitting
    // single-target strike in the kit.
    constexpr std::size_t kBerserkersFury = 3;
    check(talents[kBerserkersFury].hpCost == 8,
          "Berserker's Fury costs 8 hp -- blood magic, not mana (manaCost is 0)");
    check(talents[kBerserkersFury].manaCost == 0,
          "Berserker's Fury costs zero mana -- the cost is entirely hp");

    Stats furyTargetStats;
    furyTargetStats.hp = 40;
    furyTargetStats.maxHp = 40;
    Monster furyTarget(MonsterType::Goblin, "FuryTarget", 'f', Position{0, 0}, furyTargetStats,
                        std::make_unique<NullAIBehavior>());
    applyTalentDamage(talents[kBerserkersFury], attacker, furyTarget);
    check(matchesNormalOrCrit(40 - furyTarget.stats().hp, 25),
          "Berserker's Fury deals base 23 + strength bonus 2 == 25 damage (or 37 on a "
          "crit), the hardest hit in the kit");

    std::cout << "\n"
              << (g_allOk ? "All Warrior checks passed." : "Some Warrior checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
