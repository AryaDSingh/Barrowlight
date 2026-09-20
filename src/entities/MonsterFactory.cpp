#include "entities/MonsterFactory.hpp"

#include "ai/AoEBomber.hpp"
#include "ai/BossBehavior.hpp"
#include "ai/Chaser.hpp"
#include "ai/Kiter.hpp"
#include "ai/Support.hpp"
#include "entities/MonsterAttackProfile.hpp"
#include "entities/TalentSet.hpp"

namespace engine {

namespace {
Stats makeStats(int hp) {
    Stats stats;
    stats.hp = hp;
    stats.maxHp = hp;
    return stats;
}
} // namespace

std::unique_ptr<Monster> createMonster(MonsterType type, Position position) {
    switch (type) {
        case MonsterType::Goblin: {
            MonsterAttackProfile profile;
            profile.power = 5;
            return std::make_unique<Monster>(type, "Goblin", 'g', position, makeStats(20),
                                              std::make_unique<Chaser>(profile));
        }
        case MonsterType::Spider: {
            MonsterAttackProfile profile;
            profile.power = 3;
            profile.onHitEffect = StatusEffectInstance{StatusEffectType::Poison, 3, 2};
            profile.onHitChance = 1.f;
            return std::make_unique<Monster>(type, "Spider", 's', position, makeStats(14),
                                              std::make_unique<Chaser>(profile));
        }
        case MonsterType::Ogre: {
            MonsterAttackProfile profile;
            profile.power = 7;
            profile.onHitEffect = StatusEffectInstance{StatusEffectType::Stun, 1, 0};
            profile.onHitChance = 0.35f;
            return std::make_unique<Monster>(type, "Ogre", 'O', position, makeStats(35),
                                              std::make_unique<Chaser>(profile));
        }
        case MonsterType::Archer: {
            MonsterAttackProfile profile;
            profile.power = 4;
            return std::make_unique<Monster>(
                type, "Archer", 'a', position, makeStats(15),
                std::make_unique<Kiter>(profile, /*attackRange=*/6, /*tooCloseRange=*/2));
        }
        case MonsterType::Shaman: {
            std::vector<Talent> abilities;
            Talent empower;
            empower.name = "Empower";
            empower.description = "Grants an ally bonus damage for a few turns.";
            empower.cooldownTurns = 4;
            abilities.push_back(empower);
            return std::make_unique<Monster>(
                type, "Shaman", 'h', position, makeStats(12),
                std::make_unique<Support>(/*buffMagnitude=*/4, /*buffDuration=*/4,
                                           /*buffRadius=*/4),
                TalentSet(abilities));
        }
        case MonsterType::Bomber: {
            std::vector<Talent> abilities;
            Talent blast;
            blast.name = "Blast";
            blast.description = "An area burst, on a real cooldown.";
            blast.cooldownTurns = 5;
            abilities.push_back(blast);
            return std::make_unique<Monster>(
                type, "Bomber", 'b', position, makeStats(18),
                std::make_unique<AoEBomber>(/*blastPower=*/10, /*blastRange=*/4,
                                             /*tooCloseRange=*/2),
                TalentSet(abilities));
        }
        case MonsterType::GoblinWarlord: {
            MonsterAttackProfile meleeProfile;
            meleeProfile.power = 9;

            std::vector<Talent> abilities;
            Talent fury;
            fury.name = "Warlord's Fury";
            fury.description = "A desperate magical blast, unlocked once wounded.";
            fury.cooldownTurns = 4;
            abilities.push_back(fury);

            return std::make_unique<Monster>(
                type, "Goblin Warlord", 'W', position, makeStats(90),
                std::make_unique<BossBehavior>(meleeProfile, /*blastPower=*/14,
                                                /*blastRange=*/5, /*tooCloseRange=*/2,
                                                /*enrageBonus=*/6),
                TalentSet(abilities));
        }
    }
    return nullptr; // unreachable -- all enum values handled above
}

} // namespace engine
