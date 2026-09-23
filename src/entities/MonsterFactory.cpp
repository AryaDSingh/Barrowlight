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

// Each monster type now gets genuinely different Strength/Dexterity/
// Intelligence (Prompt 14) instead of Stats' undifferentiated defaults
// (every monster previously had strength=10/dexterity=10, identical
// regardless of type) -- matching each type's identity: Ogre is
// Strength-heavy/Dexterity-light (hits hard, rarely dodges), Spider and
// Archer lean Dexterity (evasive, "hard to pin down"), Shaman and
// Bomber lean Intelligence (already magic-coded kits). See
// ARCHITECTURE_DECISIONS.md, "Attribute-driven combat."
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
            // Baseline all around -- the roster's plain, undifferentiated
            // mob; every attribute bonus is exactly 0 (physicalDamageBonus(10)
            // == 0), so its scaled total and its scaled power are always
            // the same number. Base total is 4 (Prompt 21's rebalanced
            // value).
            constexpr int kStrength = 10;
            MonsterAttackProfile profile;
            profile.power =
                scaledPower(/*baseTotal=*/4, physicalDamageBonus(kStrength), tier);
            monster = std::make_unique<Monster>(
                type, tieredName("Goblin", tier), 'g', position,
                makeStats(scaledHp(20, tier), kStrength, /*dex=*/10, /*int=*/10),
                std::make_unique<Chaser>(profile));
            break;
        }
        case MonsterType::Spider: {
            // Dexterity-leaning -- 18% dodge, fitting its nimble,
            // hard-to-pin-down identity. Base total is 2 (Prompt 21).
            constexpr int kStrength = 8;
            MonsterAttackProfile profile;
            profile.power =
                scaledPower(/*baseTotal=*/2, physicalDamageBonus(kStrength), tier);
            profile.onHitEffect = StatusEffectInstance{StatusEffectType::Poison, 3, 2};
            profile.onHitChance = 1.f;
            monster = std::make_unique<Monster>(
                type, tieredName("Spider", tier), 's', position,
                makeStats(scaledHp(14, tier), kStrength, /*dex=*/16, /*int=*/10),
                std::make_unique<Chaser>(profile));
            break;
        }
        case MonsterType::Ogre: {
            // Strength-heavy/Dexterity-light -- hits hard, 0% dodge (a
            // lumbering brute, never evasive). Base total is 5 (Prompt
            // 21) -- most of it from physicalDamageBonus(18) == 4, not
            // the flat power field, which is exactly why scaledPower()
            // recalibrates from the *total* rather than just scaling
            // power directly.
            constexpr int kStrength = 18;
            MonsterAttackProfile profile;
            profile.power =
                scaledPower(/*baseTotal=*/5, physicalDamageBonus(kStrength), tier);
            profile.onHitEffect = StatusEffectInstance{StatusEffectType::Stun, 1, 0};
            profile.onHitChance = 0.35f;
            monster = std::make_unique<Monster>(
                type, tieredName("Ogre", tier), 'O', position,
                makeStats(scaledHp(35, tier), kStrength, /*dex=*/6, /*int=*/10),
                std::make_unique<Chaser>(profile));
            break;
        }
        case MonsterType::Archer: {
            // The roster's most evasive Dexterity lean -- 24% dodge.
            // Base total is 3 (Prompt 21).
            constexpr int kStrength = 8;
            MonsterAttackProfile profile;
            profile.power =
                scaledPower(/*baseTotal=*/3, physicalDamageBonus(kStrength), tier);
            monster = std::make_unique<Monster>(
                type, tieredName("Archer", tier), 'a', position,
                makeStats(scaledHp(15, tier), kStrength, /*dex=*/18, /*int=*/10),
                std::make_unique<Kiter>(profile, /*attackRange=*/6, /*tooCloseRange=*/2));
            break;
        }
        case MonsterType::Shaman: {
            // Intelligence-leaning, matching its already-magic-coded
            // support kit -- never deals direct damage at all, so there
            // is no damage total to scale; only hp scales with tier.
            // buffMagnitude is deliberately left unscaled too -- a
            // simplification, not an oversight (see
            // ARCHITECTURE_DECISIONS.md, "Elite/Nightmare tiers").
            std::vector<Talent> abilities;
            Talent empower;
            empower.name = "Empower";
            empower.description = "Grants an ally bonus damage for a few turns.";
            empower.cooldownTurns = 4;
            abilities.push_back(empower);
            monster = std::make_unique<Monster>(
                type, tieredName("Shaman", tier), 'h', position,
                makeStats(scaledHp(12, tier), /*str=*/6, /*dex=*/10, /*int=*/18),
                std::make_unique<Support>(/*buffMagnitude=*/4, /*buffDuration=*/4,
                                           /*buffRadius=*/4),
                TalentSet(abilities));
            break;
        }
        case MonsterType::Bomber: {
            // Intelligence-leaning -- its blast is Magic-typed (set
            // directly in AoEBomber's decision-building code, not here;
            // see ARCHITECTURE_DECISIONS.md). Base total is 7 (Prompt
            // 21). The Talent object's own damageType is set to Magic
            // too, purely for a future reader's sake -- the actual
            // damage path never reads it (see AIDecision::damageType's
            // comment).
            constexpr int kIntelligence = 16;
            std::vector<Talent> abilities;
            Talent blast;
            blast.name = "Blast";
            blast.description = "An area burst, on a real cooldown.";
            blast.cooldownTurns = 5;
            blast.damageType = DamageType::Magic;
            abilities.push_back(blast);
            monster = std::make_unique<Monster>(
                type, tieredName("Bomber", tier), 'b', position,
                makeStats(scaledHp(18, tier), /*str=*/8, /*dex=*/10, kIntelligence),
                std::make_unique<AoEBomber>(
                    /*blastPower=*/scaledPower(/*baseTotal=*/7, magicDamageBonus(kIntelligence),
                                                tier),
                    /*blastRange=*/4, /*tooCloseRange=*/2),
                TalentSet(abilities));
            break;
        }
        case MonsterType::GoblinWarlord: {
            // Powerful on both axes it uses (melee Strength, blast
            // Intelligence), Dexterity-light (0% dodge -- a boss
            // dodging hits would read as frustrating, not tense, in a
            // climactic fight). Deliberately ignores `tier` entirely
            // (Prompt 22) -- it's already the game's separately-tuned
            // hardest fight; scaling it further at high character
            // levels risks making the climactic encounter absurd rather
            // than harder. meleeProfile.power == 3 + 3
            // (physicalDamageBonus(16) == 3) == 6; blastPower == 8 + 2
            // (magicDamageBonus(14) == 2) == 10 -- both the Prompt 21
            // rebalanced totals. enrageBonus is untouched -- it's a flat
            // Empowered status bonus layered on top of whatever a hit
            // already does, orthogonal to the attribute formulas.
            MonsterAttackProfile meleeProfile;
            meleeProfile.power = 3;

            std::vector<Talent> abilities;
            Talent fury;
            fury.name = "Warlord's Fury";
            fury.description = "A desperate magical blast, unlocked once wounded.";
            fury.cooldownTurns = 4;
            fury.damageType = DamageType::Magic;
            abilities.push_back(fury);

            monster = std::make_unique<Monster>(
                type, "Goblin Warlord", 'W', position,
                makeStats(90, /*str=*/16, /*dex=*/8, /*int=*/14),
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
