#include "entities/MonsterFactory.hpp"

#include <cmath>
#include <string>

#include "ai/AoEBomber.hpp"
#include "ai/BossBehavior.hpp"
#include "ai/Chaser.hpp"
#include "ai/Kiter.hpp"
#include "ai/LichBehavior.hpp"
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
            empower.id = "shaman.empower";
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
            blast.id = "bomber.blast";
            blast.description = "Committed radius-2 blast. Three player actions to escape; stun or push interrupts.";
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
            // Three phases: melee, advancing blast pressure, then a committed
            // enraged cleave. Boss stats are independent of ordinary rarity tiers.
            constexpr int kStrength = 14;
            constexpr int kIntelligence = 10;
            MonsterAttackProfile meleeProfile;
            meleeProfile.power = 5;

            std::vector<Talent> abilities;
            Talent fury;
            fury.name = "Warlord's Fury";
            fury.id = "warlord.fury";
            fury.description = "Below 60% HP: radius-2 blast with three actions to escape; keeps advancing between casts.";
            fury.cooldownTurns = 4;
            fury.scalingStat = ScalingStat::Intelligence;
            abilities.push_back(fury);

            monster = std::make_unique<Monster>(
                type, "Goblin Warlord", 'W', position,
                makeStats(115, kStrength, /*dex=*/0, kIntelligence),
                std::make_unique<BossBehavior>(meleeProfile, /*blastPower=*/8,
                                                /*blastRange=*/5, /*tooCloseRange=*/2,
                                                /*enrageBonus=*/4),
                TalentSet(abilities));
            break;
        }
        case MonsterType::Lich: {
            // The Lich combines finite reinforcement rituals, readable bolts,
            // and a cleansable curse on its own persisted cooldown.
            constexpr int kStrength = 4;
            constexpr int kDexterity = 20;
            constexpr int kIntelligence = 16;
            MonsterAttackProfile boltProfile;
            boltProfile.power = 7;
            boltProfile.scalingStat = ScalingStat::Intelligence;

            std::vector<Talent> abilities;
            Talent raiseSkeleton;
            raiseSkeleton.name = "Raise Skeleton";
            raiseSkeleton.id = "lich.raise_skeleton";
            raiseSkeleton.description = "Three rituals total: Guard, Archer, Guard. Occupy the marked tile or interrupt. No summon XP/loot.";
            raiseSkeleton.cooldownTurns = 5;
            abilities.push_back(raiseSkeleton);
            Talent hex;
            hex.id="lich.hex"; hex.name="Grave Hex";
            hex.description="Clear sight/range 6. Mana Drain: 2 mana/tick for 4 ticks. At 60% HP, Doom instead: 10 + INT/4 delayed damage, four actions to cleanse. C removes both.";
            hex.cooldownTurns=10;
            abilities.push_back(hex);

            monster = std::make_unique<Monster>(
                type, "Lich", 'L', position, makeStats(150, kStrength, kDexterity, kIntelligence),
                std::make_unique<LichBehavior>(boltProfile, /*attackRange=*/6,
                                                /*tooCloseRange=*/2, /*maxSummons=*/3),
                TalentSet(abilities));
            break;
        }
        case MonsterType::GoblinRaider:
        case MonsterType::GoblinCaptain: {
            const bool captain=type==MonsterType::GoblinCaptain;
            MonsterAttackProfile profile;
            profile.power=scaledPower(captain?7:5,1,tier);
            profile.onHitEffect=StatusEffectInstance{StatusEffectType::Marked,3,0};
            profile.onHitChance=1.f;
            monster=std::make_unique<Monster>(type,
                captain?"Grik the Packleader":tieredName("Goblin Raider",tier), captain?'G':'r',position,
                makeStats(scaledHp(captain?55:24,tier),8,12,2),std::make_unique<Chaser>(profile));
            break;
        }
        case MonsterType::SkeletonArcher: {
            MonsterAttackProfile profile;
            profile.power=scaledPower(5,1,tier);
            profile.onHitEffect=StatusEffectInstance{StatusEffectType::Chill,2,20};
            profile.onHitChance=1.f;
            monster=std::make_unique<Monster>(type,tieredName("Skeleton Archer",tier),'v',position,
                makeStats(scaledHp(24,tier),5,18,4),std::make_unique<Kiter>(profile,6,2));
            break;
        }
        case MonsterType::SkeletonGuard: {
            MonsterAttackProfile profile;
            profile.power=scaledPower(8,2,tier);
            monster=std::make_unique<Monster>(type,tieredName("Skeleton Guard",tier),'K',position,
                makeStats(scaledHp(42,tier),10,0,2),std::make_unique<Chaser>(profile));
            break;
        }
        case MonsterType::Bonecaller: {
            Talent rally;
            rally.id="bonecaller.rally"; rally.name="Grave Rally";
            rally.description="Empowers a nearby ally with +3 damage for 3 turns.";
            rally.cooldownTurns=4;
            monster=std::make_unique<Monster>(type,tieredName("Bonecaller",tier),'n',position,
                makeStats(scaledHp(25,tier),2,0,16),std::make_unique<Support>(3,3,5),
                TalentSet(std::vector<Talent>{rally}));
            break;
        }
        case MonsterType::OssuaryWarden: {
            Talent blast;
            blast.id="ashkeeper.blast"; blast.name="Ashfall";
            blast.description="Radius-2 blast: three actions to escape. Also strikes other enemies.";
            blast.cooldownTurns=5; blast.scalingStat=ScalingStat::Intelligence;
            monster=std::make_unique<Monster>(type,"Veyra the Ashkeeper",'V',position,
                makeStats(scaledHp(65,tier),2,0,20),
                std::make_unique<AoEBomber>(scaledPower(12,4,tier),6,1),
                TalentSet(std::vector<Talent>{blast}));
            break;
        }
        case MonsterType::GoblinBulwark:
        case MonsterType::CryptSentinel:
        case MonsterType::GoblinMedic:
        case MonsterType::GraveMender:
        case MonsterType::GoblinStalker:
        case MonsterType::CryptShade:
        case MonsterType::GoblinSlinger:
        case MonsterType::FrostAcolyte: {
            const bool tank=enemyTank(type), healer=enemyHealer(type), ambush=enemyAmbusher(type);
            const bool crypt=type==MonsterType::CryptSentinel || type==MonsterType::GraveMender || type==MonsterType::CryptShade || type==MonsterType::FrostAcolyte;
            const char* name=type==MonsterType::GoblinBulwark?"Goblin Bulwark":type==MonsterType::CryptSentinel?"Crypt Sentinel":
                type==MonsterType::GoblinMedic?"Goblin Medic":type==MonsterType::GraveMender?"Grave Mender":
                type==MonsterType::GoblinStalker?"Goblin Stalker":type==MonsterType::CryptShade?"Crypt Shade":
                type==MonsterType::GoblinSlinger?"Goblin Slinger":"Frost Acolyte";
            const int strength=tank?12:ambush?8:4;
            MonsterAttackProfile profile;
            profile.power=scaledPower(tank?6:ambush?5:4,strength/5,tier);
            if(type==MonsterType::FrostAcolyte) { profile.onHitEffect=StatusEffectInstance{StatusEffectType::Chill,2,20}; profile.onHitChance=1.f; }
            if(type==MonsterType::GoblinSlinger) { profile.onHitEffect=StatusEffectInstance{StatusEffectType::Marked,2,0}; profile.onHitChance=.5f; }
            std::unique_ptr<AIBehavior> ai;
            std::vector<Talent> talents;
            if(healer) {
                Talent heal; heal.id="enemy.mend"; heal.name="Mend Ally";
                heal.description="Heals one visible ally within 5 tiles for 18% max HP. 3 casts per life; 6-turn cooldown.";
                heal.cooldownTurns=6; talents.push_back(heal);
                ai=std::make_unique<Support>(0,0,0);
            } else if(tank || ambush) ai=std::make_unique<Chaser>(profile);
            else ai=std::make_unique<Kiter>(profile,6,3);
            monster=std::make_unique<Monster>(type,tieredName(name,tier),tank?'T':healer?'+':ambush?'q':'f',position,
                makeStats(scaledHp((tank?44:healer?22:ambush?20:22)+(crypt?8:0),tier),strength,ambush?24:8,healer?16:4),
                std::move(ai),TalentSet(talents));
            break;
        }
        case MonsterType::Torchbearer: case MonsterType::Gloomstalker: case MonsterType::OrcFirebrand: case MonsterType::DrownedOne: {
            // The creatures that use light, dark, fire and water: their tricks
            // live in Application (light sources, surfaces), their bodies here.
            MonsterAttackProfile profile;
            const char* name = "Cultist Torchbearer";
            int hp = 26, strength = 8, dexterity = 8, power = 5;
            if (type == MonsterType::Torchbearer) { profile.onHitEffect = StatusEffectInstance{StatusEffectType::Burn, 3, 2}; profile.onHitChance = .5f; }
            if (type == MonsterType::Gloomstalker) { name = "Gloomstalker"; hp = 24; strength = 10; dexterity = 22; power = 6; }
            if (type == MonsterType::OrcFirebrand) { name = "Orc Firebrand"; hp = 26; strength = 6; power = 4; }
            if (type == MonsterType::DrownedOne) {
                name = "Drowned One"; hp = 38; strength = 10; dexterity = 2; power = 5;
                profile.onHitEffect = StatusEffectInstance{StatusEffectType::Chill, 2, 20}; profile.onHitChance = .4f;
            }
            profile.power = scaledPower(power, strength / 5, tier);
            std::unique_ptr<AIBehavior> ai;
            if (type == MonsterType::OrcFirebrand) ai = std::make_unique<Kiter>(profile, 5, 2);
            else ai = std::make_unique<Chaser>(profile);
            monster = std::make_unique<Monster>(type, tieredName(name, tier), 'c', position,
                makeStats(scaledHp(hp, tier), strength, dexterity, 4), std::move(ai));
            break;
        }
        case MonsterType::DeepLurker: case MonsterType::DrownedChorister: {
            // The Drowned Cathedral's own: their water tricks live in Application.
            const bool lurker = type == MonsterType::DeepLurker;
            MonsterAttackProfile profile;
            if (!lurker) profile.scalingStat = ScalingStat::Intelligence;
            profile.power = scaledPower(lurker ? 6 : 5, lurker ? 2 : 1, tier);
            std::unique_ptr<AIBehavior> ai;
            if (lurker) ai = std::make_unique<Chaser>(profile);
            else ai = std::make_unique<Kiter>(profile, 6, 2);
            monster = std::make_unique<Monster>(type, tieredName(lurker ? "Deep Lurker" : "Drowned Chorister", tier), lurker ? 'e' : 'w', position,
                makeStats(scaledHp(lurker ? 30 : 22, tier), lurker ? 12 : 4, lurker ? 16 : 10, lurker ? 2 : 14), std::move(ai));
            break;
        }
        case MonsterType::Mimic: {
            // A chest that bites: slow to wake, hard to shift, and its bite tears.
            MonsterAttackProfile bite;
            bite.power = scaledPower(7, 3, tier);
            bite.onHitEffect = StatusEffectInstance{StatusEffectType::Bleed, 3, 2}; bite.onHitChance = .5f;
            monster = std::make_unique<Monster>(type, tieredName("Mimic", tier), 'm', position,
                makeStats(scaledHp(44, tier), 14, 4, 6), std::make_unique<Chaser>(bite));
            break;
        }
        case MonsterType::OrcSmith: case MonsterType::SlagGolem: case MonsterType::Slagling: case MonsterType::BellowsImp: {
            // The Ashen Foundry's own: heat and splitting live in Application.
            MonsterAttackProfile profile;
            int hp = 34, strength = 12, dexterity = 4, intelligence = 2, power = 6, speed = 100;
            const char* name = "Orc Smith";
            if (type == MonsterType::SlagGolem) { name = "Slag Golem"; hp = 50; strength = 14; dexterity = 2; power = 7; speed = 70; }
            if (type == MonsterType::Slagling) { name = "Slagling"; hp = 12; strength = 6; dexterity = 8; power = 3; speed = 120; }
            if (type == MonsterType::BellowsImp) { name = "Bellows Imp"; hp = 18; strength = 2; dexterity = 12; intelligence = 10; power = 2;
                profile.scalingStat = ScalingStat::Intelligence; }
            profile.power = scaledPower(power, strength / 5, tier);
            std::unique_ptr<AIBehavior> ai;
            if (type == MonsterType::BellowsImp) ai = std::make_unique<Kiter>(profile, 5, 2);
            else ai = std::make_unique<Chaser>(profile);
            monster = std::make_unique<Monster>(type, tieredName(name, tier), 'f', position,
                makeStats(scaledHp(hp, tier), strength, dexterity, intelligence), std::move(ai));
            monster->stats().speed = speed;
            break;
        }
        case MonsterType::Forgemaster: {
            // A furnace that walks: a telegraphed hammer, a warned blast of heat
            // that sets the ground burning, and slaglings from the furnaces.
            MonsterAttackProfile hammer;
            hammer.power = 7;
            std::vector<Talent> abilities;
            Talent blast;
            blast.name = "Furnace Breath"; blast.id = "forgemaster.breath";
            blast.description = "A radius-2 blast of heat with three actions to escape; the ground it touches burns.";
            blast.cooldownTurns = 4; blast.scalingStat = ScalingStat::Strength;
            abilities.push_back(blast);
            monster = std::make_unique<Monster>(type, "The Forgemaster", 'F', position, makeStats(260, 16, 4, 8),
                std::make_unique<BossBehavior>(hammer, /*blastPower=*/9, /*blastRange=*/5, /*tooCloseRange=*/2, /*enrageBonus=*/3),
                TalentSet(abilities));
            break;
        }
        case MonsterType::TheSleeper: {
            // A drowned god's eye: bolts from range, never backs away. Its
            // flood, its charged water and its call live in Application.
            MonsterAttackProfile bolt;
            bolt.power = 8;
            bolt.scalingStat = ScalingStat::Intelligence;
            monster = std::make_unique<Monster>(type, "The Sleeper Below", 'S', position, makeStats(240, 6, 12, 18),
                std::make_unique<Kiter>(bolt, 7, 0));
            break;
        }
        case MonsterType::Skeleton: {
            // Lich minion, also placed naturally in Crypt opening groups.
            // Summons lose rewards in Application; natural spawns retain them.
            // Same base HP as a Goblin (20), with lower damage (
            // 3 total damage vs. 4) -- individually modest, dangerous
            // in numbers if left unchecked while focusing the Lich
            // itself, which is the whole point of the mechanic. Plain
            // Chaser melee, no on-hit effect -- a minion doesn't need
            // its own gimmick on top of just being another body in the
            // fight. Always Base tier: summonTier defaults to Base in
            // AIDecision (see AIBehavior.hpp) and LichBehavior never
            // overrides it -- a summoned skeleton doesn't scale with
            // character level the way a dungeon's own spawned roster
            // does.
            constexpr int kStrength = 5;
            MonsterAttackProfile profile;
            profile.power = scaledPower(3,1,tier); // Preserve allied skeleton damage; natural elites scale correctly.
            monster = std::make_unique<Monster>(
                type, tieredName("Skeleton", tier), 'z', position,
                makeStats(scaledHp(20, tier), kStrength, /*dex=*/0, /*int=*/0),
                std::make_unique<Chaser>(profile));
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
    if (type==MonsterType::GoblinWarlord || type==MonsterType::Lich)
        monster->statusEffects().setStunRules(1,2);
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
        case MonsterType::Lich:
            // The true final fight -- an even larger flat reward than
            // the Warlord's, matching "the last boss in the game," even
            // though in practice defeating it ends the run before any
            // further XP could matter. Same "always Base tier" reasoning
            // as GoblinWarlord.
            return 300;
        case MonsterType::GoblinBulwark: case MonsterType::CryptSentinel: baseReward=24; break;
        case MonsterType::GoblinMedic: case MonsterType::GraveMender: baseReward=22; break;
        case MonsterType::GoblinStalker: case MonsterType::CryptShade: baseReward=19; break;
        case MonsterType::GoblinSlinger: case MonsterType::FrostAcolyte: baseReward=18; break;
        case MonsterType::GoblinRaider: baseReward=14; break;
        case MonsterType::Torchbearer: baseReward=16; break;
        case MonsterType::Gloomstalker: case MonsterType::DrownedOne: baseReward=20; break;
        case MonsterType::OrcFirebrand: baseReward=18; break;
        case MonsterType::DeepLurker: baseReward=20; break;
        case MonsterType::DrownedChorister: baseReward=18; break;
        case MonsterType::TheSleeper: return 250;
        case MonsterType::Mimic: baseReward=40; break;
        case MonsterType::OrcSmith: baseReward=20; break;
        case MonsterType::SlagGolem: baseReward=26; break;
        case MonsterType::Slagling: baseReward=6; break;
        case MonsterType::BellowsImp: baseReward=18; break;
        case MonsterType::Forgemaster: return 220;
        case MonsterType::SkeletonArcher: baseReward=16; break;
        case MonsterType::SkeletonGuard: baseReward=22; break;
        case MonsterType::Bonecaller: baseReward=18; break;
        case MonsterType::GoblinCaptain: baseReward=65; break;
        case MonsterType::OssuaryWarden: baseReward=80; break;
        case MonsterType::Skeleton:
            baseReward = 5; // a modest fraction of Goblin's 10 -- a minion, not a real kill goal
            break;
    }
    return static_cast<int>(std::lround(baseReward * xpMultiplierForTier(tier)));
}

} // namespace engine
