#include "entities/MonsterFactory.hpp"

#include <cmath>
#include <string>

#include "ai/AoEBomber.hpp"
#include "ai/BossBehavior.hpp"
#include "ai/Chaser.hpp"
#include "ai/Kiter.hpp"
#include "ai/Support.hpp"
#include "entities/AttributeFormulas.hpp"
#include "entities/MonsterAttackProfile.hpp"
#include "entities/TalentSet.hpp"

namespace engine {

namespace {

// Each monster type gets genuinely different Strength/Dexterity/
// Intelligence, matching each type's identity: Ogre is Strength-heavy
// (hits hard), Spider and Archer lean Dexterity (evasive, "hard to pin
// down"), Shaman and Bomber lean Intelligence (already magic-coded
// kits).
//
// Rebalanced for the attribute-system redesign (Prompt 26+): monster
// attributes are NOT held to the same "genuinely low, hand-picked"
// philosophy player starting stats follow. Players start low
// specifically because they grow through play -- monsters are static,
// so their Str/Dex/Int values are chosen purely to reproduce the exact
// dodge percentages and damage totals already tuned in Prompt 21, not
// to look like a plausible "level 1" character. Spider/Archer's high
// Dexterity values below are a deliberate example of this: 36 and 48
// respectively look large next to a player's starting 2-6, but they
// exist purely to reproduce this roster's existing 18%/24% dodge
// identity exactly (36 * 0.5% == 18%, 48 * 0.5% == 24%) under the new
// no-baseline dodge formula, not to imply monsters have "leveled up."
// A side effect, not a separate design pass: Spider/Archer's same high
// Dexterity now also gives them a real crit chance (23%/29%) under the
// new global crit system, which happens to reinforce their existing
// "nimble, precise" identity rather than working against it.
Stats makeStats(int hp, int strength, int dexterity, int intelligence) {
    Stats stats;
    stats.hp = hp;
    stats.maxHp = hp;
    stats.strength = strength;
    stats.dexterity = dexterity;
    stats.intelligence = intelligence;
    return stats;
}

// Scales a monster's hp for its tier (Prompt 22), rounding to the
// nearest int.
int scaledHp(int baseHp, MonsterTier tier) {
    return static_cast<int>(std::lround(baseHp * hpMultiplierForTier(tier)));
}

// Scales a monster's *total* damage (base power + attribute bonus) for
// its tier, then returns the new base `power` needed to still hit that
// scaled total once the (tier-invariant) attribute bonus is added back
// in. Deliberately not "multiply power alone" -- for a monster like the
// Ogre, most of its damage comes from the Strength-bonus formula, not
// the flat power field, so scaling power alone would barely move its
// actual output. Same "base + bonus = target total" recalibration
// discipline every attribute-driven number in this project has used
// since Prompt 14.
int scaledPower(int baseTotal, int attributeBonus, MonsterTier tier) {
    const int scaledTotal =
        static_cast<int>(std::lround(baseTotal * damageMultiplierForTier(tier)));
    return scaledTotal - attributeBonus;
}

std::string tieredName(const char* baseName, MonsterTier tier) {
    return std::string(namePrefixForTier(tier)) + baseName;
}

} // namespace

std::unique_ptr<Monster> createMonster(MonsterType type, Position position, MonsterTier tier) {
    std::unique_ptr<Monster> monster;

    switch (type) {
        case MonsterType::Goblin: {
            // Baseline all around -- the roster's plain,
            // undifferentiated mob. Modest Strength (6) gives a small,
            // real bonus (int(6/5) == 1) rather than the old system's
            // "exactly 0 at baseline" -- there's no baseline left to
            // sit exactly on anymore, so "plain" is now expressed as
            // "unspecialized," not "contributes nothing." Base total
            // stays 4 (Prompt 21's rebalanced value): power 3 + bonus 1
            // == 4.
            constexpr int kStrength = 6;
            constexpr int kDexterity = 0; // 0% dodge, matching the original tuned identity exactly
            MonsterAttackProfile profile;
            profile.power = scaledPower(
                /*baseTotal=*/4, abilityDamageBonus(ScalingStat::Strength, kStrength, 0), tier);
            monster = std::make_unique<Monster>(
                type, tieredName("Goblin", tier), 'g', position,
                makeStats(scaledHp(20, tier), kStrength, kDexterity, /*int=*/2),
                std::make_unique<Chaser>(profile));
            break;
        }
        case MonsterType::Spider: {
            // Dexterity-leaning -- Dexterity 36 reproduces the exact
            // original 18% dodge (36 * 0.5% == 18%) under the new
            // formula; as a side effect, also gives a real 23% crit
            // chance now (5% base + 18%), which happens to reinforce
            // rather than fight its nimble identity. Base total stays 2
            // (Prompt 21): power 2 + bonus 0 (4/5 truncates to 0) == 2.
            constexpr int kStrength = 4;
            constexpr int kDexterity = 36;
            MonsterAttackProfile profile;
            profile.power = scaledPower(
                /*baseTotal=*/2, abilityDamageBonus(ScalingStat::Strength, kStrength, 0), tier);
            profile.onHitEffect = StatusEffectInstance{StatusEffectType::Poison, 3, 2};
            profile.onHitChance = 1.f;
            monster = std::make_unique<Monster>(
                type, tieredName("Spider", tier), 's', position,
                makeStats(scaledHp(14, tier), kStrength, kDexterity, /*int=*/2),
                std::make_unique<Chaser>(profile));
            break;
        }
        case MonsterType::Ogre: {
            // Strength-heavy/Dexterity-light -- hits hard, 0% dodge (a
            // lumbering brute, never evasive, matching the original
            // tuned identity exactly). Base total stays 5 (Prompt 21):
            // power 2 + bonus 3 (15/5 == 3) == 5 -- most of its damage
            // still comes from the Strength bonus, not the flat power
            // field, same as the original design intent.
            constexpr int kStrength = 15;
            constexpr int kDexterity = 0;
            MonsterAttackProfile profile;
            profile.power = scaledPower(
                /*baseTotal=*/5, abilityDamageBonus(ScalingStat::Strength, kStrength, 0), tier);
            profile.onHitEffect = StatusEffectInstance{StatusEffectType::Stun, 1, 0};
            profile.onHitChance = 0.35f;
            monster = std::make_unique<Monster>(
                type, tieredName("Ogre", tier), 'O', position,
                makeStats(scaledHp(35, tier), kStrength, kDexterity, /*int=*/2),
                std::make_unique<Chaser>(profile));
            break;
        }
        case MonsterType::Archer: {
            // The roster's most evasive Dexterity lean -- Dexterity 48
            // reproduces the exact original 24% dodge (48 * 0.5% ==
            // 24%); also now a real 29% crit chance (5% + 24%), the
            // highest in the roster, matching "the most evasive" with
            // "the most precise" naturally. Base total stays 3 (Prompt
            // 21): power 3 + bonus 0 (4/5 truncates to 0) == 3.
            constexpr int kStrength = 4;
            constexpr int kDexterity = 48;
            MonsterAttackProfile profile;
            profile.power = scaledPower(
                /*baseTotal=*/3, abilityDamageBonus(ScalingStat::Strength, kStrength, 0), tier);
            monster = std::make_unique<Monster>(
                type, tieredName("Archer", tier), 'a', position,
                makeStats(scaledHp(15, tier), kStrength, kDexterity, /*int=*/2),
                std::make_unique<Kiter>(profile, /*attackRange=*/6, /*tooCloseRange=*/2));
            break;
        }
        case MonsterType::Shaman: {
            // Intelligence-leaning, matching its already-magic-coded
            // support kit -- never deals direct damage at all, so there
            // is no damage total to scale; only hp scales with tier.
            // Dexterity 0 -- dodge/crit are moot for a support unit's
            // own identity, but it can still be attacked, so this isn't
            // a "doesn't matter" field, just deliberately unspecialized
            // rather than tuned. buffMagnitude is deliberately left
            // unscaled by tier too -- a simplification, not an
            // oversight (see ARCHITECTURE_DECISIONS.md, "Elite/
            // Nightmare tiers").
            std::vector<Talent> abilities;
            Talent empower;
            empower.name = "Empower";
            empower.description = "Grants an ally bonus damage for a few turns.";
            empower.cooldownTurns = 4;
            abilities.push_back(empower);
            monster = std::make_unique<Monster>(
                type, tieredName("Shaman", tier), 'h', position,
                makeStats(scaledHp(12, tier), /*str=*/1, /*dex=*/0, /*int=*/16),
                std::make_unique<Support>(/*buffMagnitude=*/4, /*buffDuration=*/4,
                                           /*buffRadius=*/4),
                TalentSet(abilities));
            break;
        }
        case MonsterType::Bomber: {
            // Intelligence-leaning -- its blast is Magic-typed (set
            // directly in AoEBomber's decision-building code, not here;
            // see ARCHITECTURE_DECISIONS.md). Base total stays 7
            // (Prompt 21): power 5 + bonus 2 (14/5 == 2) == 7. The
            // Talent object's own scalingStat is set to Intelligence
            // too, purely for a future reader's sake -- the actual
            // damage path never reads it (see AIDecision::scalingStat's
            // comment).
            constexpr int kIntelligence = 14;
            std::vector<Talent> abilities;
            Talent blast;
            blast.name = "Blast";
            blast.description = "An area burst, on a real cooldown.";
            blast.cooldownTurns = 5;
            blast.scalingStat = ScalingStat::Intelligence;
            abilities.push_back(blast);
            monster = std::make_unique<Monster>(
                type, tieredName("Bomber", tier), 'b', position,
                makeStats(scaledHp(18, tier), /*str=*/2, /*dex=*/0, kIntelligence),
                std::make_unique<AoEBomber>(
                    /*blastPower=*/scaledPower(
                        /*baseTotal=*/7, abilityDamageBonus(ScalingStat::Intelligence, kIntelligence, 0),
                        tier),
                    /*blastRange=*/4, /*tooCloseRange=*/2),
                TalentSet(abilities));
            break;
        }
        case MonsterType::GoblinWarlord: {
            // Powerful on both axes it uses (melee Strength, blast
            // Intelligence), Dexterity-light (0% dodge -- a boss
            // dodging hits would read as frustrating, not tense, in a
            // climactic fight, matching the original tuned identity
            // exactly). Deliberately ignores `tier` entirely (Prompt
            // 22) -- it's already the game's separately-tuned hardest
            // fight; scaling it further at high character levels risks
            // making the climactic encounter absurd rather than harder.
            // meleeProfile.power == 4 + 2 (14/5 == 2) == 6; blastPower
            // == 8 + 2 (10/5 == 2) == 10 -- both the Prompt 21
            // rebalanced totals, unchanged. enrageBonus is untouched --
            // it's a flat Empowered status bonus layered on top of
            // whatever a hit already does, orthogonal to the attribute
            // formulas.
            constexpr int kStrength = 14;
            constexpr int kIntelligence = 10;
            MonsterAttackProfile meleeProfile;
            meleeProfile.power = 4;

            std::vector<Talent> abilities;
            Talent fury;
            fury.name = "Warlord's Fury";
            fury.description = "A desperate magical blast, unlocked once wounded.";
            fury.cooldownTurns = 4;
            fury.scalingStat = ScalingStat::Intelligence;
            abilities.push_back(fury);

            monster = std::make_unique<Monster>(
                type, "Goblin Warlord", 'W', position,
                makeStats(90, kStrength, /*dex=*/0, kIntelligence),
                std::make_unique<BossBehavior>(meleeProfile, /*blastPower=*/8,
                                                /*blastRange=*/5, /*tooCloseRange=*/2,
                                                /*enrageBonus=*/6),
                TalentSet(abilities));
            break;
        }
    }

    if (monster != nullptr) {
        // Prompt 20: every monster carries its own XP reward from the
        // moment it's created, via Actor::setXpReward() -- callers
        // (Application) never need to know xpRewardForType() exists at
        // all, they just read monster->xpReward() when it dies.
        monster->setXpReward(xpRewardForType(type, tier));
        // Prompt 22: likewise for tier -- set here once, read later by
        // Application's rendering for the Elite/Nightmare border.
        monster->setTier(tier);
    }
    return monster;
}

int xpRewardForType(MonsterType type, MonsterTier tier) {
    int baseReward = 0;
    switch (type) {
        case MonsterType::Goblin:
            baseReward = 10; // the roster's baseline -- every other value below is relative to this
            break;
        case MonsterType::Spider:
            baseReward = 12;
            break;
        case MonsterType::Ogre:
            baseReward = 20; // the toughest regular Chaser -- more hp, hits harder
            break;
        case MonsterType::Archer:
            baseReward = 12;
            break;
        case MonsterType::Shaman:
            baseReward = 15; // disruptive (buffs allies) even though it never attacks directly
            break;
        case MonsterType::Bomber:
            baseReward = 15;
            break;
        case MonsterType::GoblinWarlord:
            // The set-piece finale -- a genuinely large reward on its
            // own terms, deliberately large enough that it alone can
            // cross several level thresholds at once (see
            // PlayerLeveling.hpp's grantXp(), which loops specifically
            // to handle this). Always Base tier regardless of the
            // `tier` argument -- the boss doesn't scale with tier (see
            // createMonster()'s GoblinWarlord case), so its reward
            // shouldn't either.
            return 200;
    }
    return static_cast<int>(std::lround(baseReward * xpMultiplierForTier(tier)));
}

} // namespace engine
