// Standalone sanity check for the Marauder's talent kit
// (MarauderTalents) and PlayerClassFactory -- hand-computed expected
// results, no SFML, no window, no Application.

#include <iostream>
#include <memory>
#include <string>

#include "ai/NullAIBehavior.hpp"
#include "entities/MarauderTalents.hpp"
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
    // --- PlayerClassFactory: statsForClass(Marauder).
    const Stats marauderStats = statsForClass(PlayerClass::Marauder);
    check(marauderStats.strength == 20, "Marauder strength == 20 (pure Strength identity)");
    check(marauderStats.dexterity == 8, "Marauder dexterity == 8 (below baseline -- 0% dodge)");
    check(marauderStats.intelligence == 4,
          "Marauder intelligence == 4 (well below baseline -- a genuine dump stat)");
    check(marauderStats.maxHp == 45,
          "Marauder maxHp == 45 (hand-tuned tanky identity, higher than the Spellblade's 30)");
    check(marauderStats.maxMana == 10,
          "Marauder maxMana == 10 base 10 + manaBonusFromIntelligence(4) == 0 (floored, "
          "not negative) -- a small pool, not the Spellblade's mana-juggling");
    check(marauderStats.mana == marauderStats.maxMana, "Marauder starts at full mana");
    check(marauderStats.hp == marauderStats.maxHp, "Marauder starts at full hp");

    // --- PlayerClassFactory: talentSetForClass(Marauder) round-trips
    // the same talents marauderTalents() returns directly.
    const TalentSet marauderTalentSet = talentSetForClass(PlayerClass::Marauder);
    check(marauderTalentSet.knownTalents().size() == 4,
          "talentSetForClass(Marauder) carries exactly 4 talents");

    // --- marauderTalents(): the data table itself.
    const std::vector<Talent> talents = marauderTalents();
    check(talents.size() == 4, "marauderTalents() returns exactly 4 talents");

    // An attacker with the Marauder's own real configured stats --
    // verifying the recalibration (base power + strength bonus ==
    // the intended total) the same way talent_test.cpp does for the
    // Spellblade.
    Monster attacker(MonsterType::Goblin, "MarauderAttacker", '@', Position{0, 0}, marauderStats,
                      std::make_unique<NullAIBehavior>());

    // Index 0: Slam -- zero mana cost, base 4 + strength bonus 5 == 9.
    constexpr std::size_t kSlam = 0;
    check(talents[kSlam].manaCost == 0, "Slam costs zero mana -- the Marauder's free filler");
    check(talents[kSlam].cooldownTurns == 1, "Slam has only a 1-turn cooldown -- spammable");

    Stats slamTargetStats;
    slamTargetStats.hp = 20;
    slamTargetStats.maxHp = 20; // dexterity left at the 10 baseline -- guaranteed to land
    Monster slamTarget(MonsterType::Goblin, "SlamTarget", 's', Position{0, 0}, slamTargetStats,
                        std::make_unique<NullAIBehavior>());
    const bool slamHit = applyTalentDamage(talents[kSlam], attacker, slamTarget);
    check(slamHit, "Slam lands against a 0%-dodge target (guaranteed, not flaky)");
    check(slamTarget.stats().hp == 11,
          "Slam deals base 4 + strength bonus 5 == 9 damage (20 -> 11)");

    // Index 1: Cleave -- AreaAroundSelf, base 5 + strength bonus 5 == 10
    // per enemy. Only the shape/targeting/cost are checked here (the
    // "hits everyone nearby" resolution itself is Application's job,
    // already covered by the AoE-radius tests elsewhere); the damage
    // math is the same applyTalentDamage path already proven above.
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
    check(cleaveTarget.stats().hp == 10,
          "Cleave deals base 5 + strength bonus 5 == 10 damage per enemy hit (20 -> 10)");

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
          "boss's own enrage uses (Prompt 11), reused rather than reinvented");

    Stats casterStats;
    casterStats.hp = 45;
    casterStats.maxHp = 45;
    casterStats.strength = 20; // the Marauder's own real strength -- without this, the
                                // Empowered+Slam check below would silently use the Stats
                                // default (10) instead of testing the intended scenario
    Monster caster(MonsterType::Goblin, "Caster", 'c', Position{0, 0}, casterStats,
                    std::make_unique<NullAIBehavior>());
    check(!caster.statusEffects().has(StatusEffectType::Empowered),
          "caster starts without Empowered (sanity check before applying it)");
    applyTalentSelfBuff(talents[kRallyingCry], caster);
    check(caster.statusEffects().has(StatusEffectType::Empowered),
          "applyTalentSelfBuff grants the caster Empowered");
    check(caster.statusEffects().magnitudeOf(StatusEffectType::Empowered) == 4,
          "Empowered's magnitude matches the talent's configured selfBuffEffect (4)");

    // The exact bug live testing caught: Rallying Cry granting Empowered
    // did nothing to the caster's own subsequent damage, since
    // applyTalentDamage never checked for it (only executeAIDecision,
    // the *monster*-attack path, did). Verify the fix directly: the same
    // caster now empowered, using Slam, deals its normal 9 damage *plus*
    // the 4 from Empowered.
    Stats empoweredSlamTargetStats;
    empoweredSlamTargetStats.hp = 20;
    empoweredSlamTargetStats.maxHp = 20;
    Monster empoweredSlamTarget(MonsterType::Goblin, "EmpoweredSlamTarget", 'e', Position{0, 0},
                                 empoweredSlamTargetStats, std::make_unique<NullAIBehavior>());
    applyTalentDamage(talents[kSlam], caster, empoweredSlamTarget);
    check(empoweredSlamTarget.stats().hp == 7,
          "an Empowered caster's Slam deals base 4 + strength bonus 5 + Empowered 4 == 13 "
          "damage (20 -> 7), not just the un-buffed 9 -- the exact scenario live testing "
          "caught as broken before this fix");

    // Index 3: Berserker's Fury -- costs hp, base 23 + strength bonus 5
    // == 28, the hardest-hitting single-target strike in either kit.
    constexpr std::size_t kBerserkersFury = 3;
    check(talents[kBerserkersFury].hpCost == 8,
          "Berserker's Fury costs 8 hp -- blood magic, not mana (manaCost is 0)");
    check(talents[kBerserkersFury].manaCost == 0,
          "Berserker's Fury costs zero mana -- the cost is entirely hp");

    Stats furyTargetStats;
    furyTargetStats.hp = 30;
    furyTargetStats.maxHp = 30;
    Monster furyTarget(MonsterType::Goblin, "FuryTarget", 'f', Position{0, 0}, furyTargetStats,
                        std::make_unique<NullAIBehavior>());
    applyTalentDamage(talents[kBerserkersFury], attacker, furyTarget);
    check(furyTarget.stats().hp == 2,
          "Berserker's Fury deals base 23 + strength bonus 5 == 28 damage (30 -> 2), the "
          "hardest hit in either class's kit");

    std::cout << "\n"
              << (g_allOk ? "All Marauder checks passed." : "Some Marauder checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
