#include "entities/MonsterFactory.hpp"

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

} // namespace

std::unique_ptr<Monster> createMonster(MonsterType type, Position position) {
    switch (type) {
        case MonsterType::Goblin: {
            // Baseline all around -- the roster's plain, undifferentiated
            // mob; every attribute bonus is exactly 0, so its power stays
            // the original tuned value unmodified.
            MonsterAttackProfile profile;
            profile.power = 5;
            return std::make_unique<Monster>(
                type, "Goblin", 'g', position, makeStats(20, /*str=*/10, /*dex=*/10, /*int=*/10),
                std::make_unique<Chaser>(profile));
        }
        case MonsterType::Spider: {
            // Dexterity-leaning -- 18% dodge, fitting its nimble,
            // hard-to-pin-down identity. power == 4 - 1 (physicalDamageBonus(8)
            // == -1) == 3, matching the original tuned value.
            MonsterAttackProfile profile;
            profile.power = 4;
            profile.onHitEffect = StatusEffectInstance{StatusEffectType::Poison, 3, 2};
            profile.onHitChance = 1.f;
            return std::make_unique<Monster>(
                type, "Spider", 's', position, makeStats(14, /*str=*/8, /*dex=*/16, /*int=*/10),
                std::make_unique<Chaser>(profile));
        }
        case MonsterType::Ogre: {
            // Strength-heavy/Dexterity-light -- hits hard (physicalDamageBonus(18)
            // == 4, so power == 3 + 4 == 7, matching the original tuned
            // value), 0% dodge (a lumbering brute, never evasive).
            MonsterAttackProfile profile;
            profile.power = 3;
            profile.onHitEffect = StatusEffectInstance{StatusEffectType::Stun, 1, 0};
            profile.onHitChance = 0.35f;
            return std::make_unique<Monster>(
                type, "Ogre", 'O', position, makeStats(35, /*str=*/18, /*dex=*/6, /*int=*/10),
                std::make_unique<Chaser>(profile));
        }
        case MonsterType::Archer: {
            // The roster's most evasive Dexterity lean -- 24% dodge.
            // power == 5 + (-1) == 4, matching the original tuned value.
            MonsterAttackProfile profile;
            profile.power = 5;
            return std::make_unique<Monster>(
                type, "Archer", 'a', position, makeStats(15, /*str=*/8, /*dex=*/18, /*int=*/10),
                std::make_unique<Kiter>(profile, /*attackRange=*/6, /*tooCloseRange=*/2));
        }
        case MonsterType::Shaman: {
            // Intelligence-leaning, matching its already-magic-coded
            // support kit -- never deals direct damage (so the
            // Intelligence bonus never actually applies to anything),
            // but genuinely squishy: 0% dodge on top of its already-low
            // 12 hp, a real priority-kill target, not just nominally one.
            std::vector<Talent> abilities;
            Talent empower;
            empower.name = "Empower";
            empower.description = "Grants an ally bonus damage for a few turns.";
            empower.cooldownTurns = 4;
            abilities.push_back(empower);
            return std::make_unique<Monster>(
                type, "Shaman", 'h', position, makeStats(12, /*str=*/6, /*dex=*/10, /*int=*/18),
                std::make_unique<Support>(/*buffMagnitude=*/4, /*buffDuration=*/4,
                                           /*buffRadius=*/4),
                TalentSet(abilities));
        }
        case MonsterType::Bomber: {
            // Intelligence-leaning -- its blast is Magic-typed (set
            // directly in AoEBomber's decision-building code, not here;
            // see ARCHITECTURE_DECISIONS.md). blastPower == 7 + 3
            // (magicDamageBonus(16) == 3) == 10, matching the original
            // tuned value. The Talent object's own damageType is set to
            // Magic too, purely for a future reader's sake -- the actual
            // damage path never reads it (see AIDecision::damageType's
            // comment).
            std::vector<Talent> abilities;
            Talent blast;
            blast.name = "Blast";
            blast.description = "An area burst, on a real cooldown.";
            blast.cooldownTurns = 5;
            blast.damageType = DamageType::Magic;
            abilities.push_back(blast);
            return std::make_unique<Monster>(
                type, "Bomber", 'b', position, makeStats(18, /*str=*/8, /*dex=*/10, /*int=*/16),
                std::make_unique<AoEBomber>(/*blastPower=*/7, /*blastRange=*/4,
                                             /*tooCloseRange=*/2),
                TalentSet(abilities));
        }
        case MonsterType::GoblinWarlord: {
            // Powerful on both axes it uses (melee Strength, blast
            // Intelligence), Dexterity-light (0% dodge -- a boss dodging
            // hits would read as frustrating, not tense, in a climactic
            // fight). meleeProfile.power == 6 + 3 (physicalDamageBonus(16)
            // == 3) == 9; blastPower == 12 + 2 (magicDamageBonus(14) == 2)
            // == 14 -- both matching their original tuned values.
            // enrageBonus is untouched -- it's a flat Empowered status
            // bonus layered on top of whatever a hit already does,
            // orthogonal to the attribute formulas, same as how
            // Empowered already stacked with base damage before this
            // prompt.
            MonsterAttackProfile meleeProfile;
            meleeProfile.power = 6;

            std::vector<Talent> abilities;
            Talent fury;
            fury.name = "Warlord's Fury";
            fury.description = "A desperate magical blast, unlocked once wounded.";
            fury.cooldownTurns = 4;
            fury.damageType = DamageType::Magic;
            abilities.push_back(fury);

            return std::make_unique<Monster>(
                type, "Goblin Warlord", 'W', position,
                makeStats(90, /*str=*/16, /*dex=*/8, /*int=*/14),
                std::make_unique<BossBehavior>(meleeProfile, /*blastPower=*/12,
                                                /*blastRange=*/5, /*tooCloseRange=*/2,
                                                /*enrageBonus=*/6),
                TalentSet(abilities));
        }
    }
    return nullptr; // unreachable -- all enum values handled above
}

} // namespace engine
