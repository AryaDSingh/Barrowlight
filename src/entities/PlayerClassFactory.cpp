#include "entities/PlayerClassFactory.hpp"

#include "entities/AttributeFormulas.hpp"
#include "entities/FighterTalents.hpp"
#include "entities/SorcererTalents.hpp"
#include "entities/SpellbladeTalents.hpp"
#include "entities/ThiefTalents.hpp"

namespace engine {

Stats statsForClass(PlayerClass cls) {
    Stats stats;
    switch (cls) {
        case PlayerClass::Spellblade: {
            // The Str+Int hybrid corner of the attribute triangle (see
            // ROADMAP.md, "Phase 2") -- Dexterity left at baseline,
            // evasion isn't this class's identity. maxMana == 12 base +
            // manaBonusFromIntelligence(18) == 8, landing on 20 exactly
            // -- the same value this class has had since Prompt 9, now
            // genuinely formula-driven (see Prompt 14). Reserved for a
            // future unlock as of Prompt 19 (see PlayerClass.hpp) --
            // not offered on the selection screen, but still fully
            // functional if ever reached via that future mechanism or
            // an existing save file.
            constexpr int kBaseMana = 12;
            stats.hp = 30;
            stats.maxHp = 30;
            stats.strength = 14;
            stats.dexterity = 10;
            stats.intelligence = 18;
            stats.maxMana = kBaseMana + manaBonusFromIntelligence(stats.intelligence);
            stats.mana = stats.maxMana;
            break;
        }
        case PlayerClass::Fighter: {
            // Pure Strength -- originally "Marauder" (Prompt 15),
            // renamed at Prompt 19. Intelligence and Dexterity both sit
            // below baseline, a genuine trade-off (Intelligence below
            // 10 floors its mana contribution at 0, not negative, but
            // still means no bonus at all). Higher hand-tuned maxHp
            // than the Spellblade (45 vs 30) -- Strength giving a
            // dynamic hp bonus was deliberately deferred at Prompt 14
            // (would have meant rebalancing every already-tuned hp
            // number project-wide), so "tanky" is expressed directly in
            // this class's own data instead, the same way every other
            // hp value in this project already is. maxMana == 10 base +
            // manaBonusFromIntelligence(4) == 0, landing on exactly 10
            // -- a small pool for Cleave/Rallying Cry, not the
            // Spellblade's constant mana-juggling.
            constexpr int kBaseMana = 10;
            stats.hp = 45;
            stats.maxHp = 45;
            stats.strength = 20;
            stats.dexterity = 8;
            stats.intelligence = 4;
            stats.maxMana = kBaseMana + manaBonusFromIntelligence(stats.intelligence);
            stats.mana = stats.maxMana;
            break;
        }
        case PlayerClass::Thief: {
            // Pure Dexterity -- originally "Archer" (Prompt 16), renamed
            // at Prompt 19. The mirror of the Fighter's pure Strength.
            // Dexterity 20 hits the dodge-chance cap exactly (30%, see
            // AttributeFormulas::dodgeChance) -- the maximum possible
            // evasion in the game, this class's entire defensive
            // identity, since its hp pool is deliberately the lowest of
            // the three base classes (24, versus the Fighter's 45):
            // survives by not getting hit, not by soaking hits.
            // Strength (14) is moderate, not a dump stat -- someone has
            // to actually draw the bow -- while Intelligence (6) sits
            // below baseline on purpose, a real (if modest) penalty
            // reinforcing "not a caster at all." maxMana == 8 base +
            // manaBonusFromIntelligence(6) == 0 (floored, not
            // negative), landing on exactly 8.
            constexpr int kBaseMana = 8;
            stats.hp = 24;
            stats.maxHp = 24;
            stats.strength = 14;
            stats.dexterity = 20;
            stats.intelligence = 6;
            stats.maxMana = kBaseMana + manaBonusFromIntelligence(stats.intelligence);
            stats.mana = stats.maxMana;
            break;
        }
        case PlayerClass::Sorcerer: {
            // Pure Intelligence -- new at Prompt 19, completing the
            // trio. Strength and Dexterity are both true dump stats (6
            // and 8) -- unlike the Thief, whose damage still needed a
            // moderate Strength since Dexterity carries no damage bonus
            // in this project's formula system, the Sorcerer's
            // Intelligence already covers both its own damage *and*
            // mana, so there's no equivalent reason to keep anything
            // else moderate. The lowest hp of any class (22, even below
            // the Thief's 24) paired with 0% dodge (Dexterity 8) --
            // doubly fragile, the purest glass cannon of the three,
            // deliberately not compensated for since the upcoming
            // leveling/monster-rebalance work (Prompt 20+) will reshape
            // overall danger levels anyway. maxMana == 14 base +
            // manaBonusFromIntelligence(24) == 14, landing on 28 -- the
            // largest pool of any class by a wide margin, reflecting a
            // genuinely spell-hungry identity rather than the other
            // classes' "mostly free basics, small pool for specials"
            // economy.
            constexpr int kBaseMana = 14;
            stats.hp = 22;
            stats.maxHp = 22;
            stats.strength = 6;
            stats.dexterity = 8;
            stats.intelligence = 24;
            stats.maxMana = kBaseMana + manaBonusFromIntelligence(stats.intelligence);
            stats.mana = stats.maxMana;
            break;
        }
    }
    return stats;
}

TalentSet talentSetForClass(PlayerClass cls) {
    switch (cls) {
        case PlayerClass::Spellblade:
            return TalentSet(spellbladeTalents());
        case PlayerClass::Fighter:
            return TalentSet(fighterTalents());
        case PlayerClass::Thief:
            return TalentSet(thiefTalents());
        case PlayerClass::Sorcerer:
            return TalentSet(sorcererTalents());
    }
    return TalentSet(); // unreachable -- all enum values handled above
}

std::optional<Talent> talentUnlockedAtLevel(PlayerClass cls, int level) {
    switch (cls) {
        case PlayerClass::Spellblade:
            return std::nullopt; // reserved for its own separate unlock mechanism, not this one
        case PlayerClass::Fighter:
            return fighterTalentUnlockedAtLevel(level);
        case PlayerClass::Thief:
            return thiefTalentUnlockedAtLevel(level);
        case PlayerClass::Sorcerer:
            return sorcererTalentUnlockedAtLevel(level);
    }
    return std::nullopt; // unreachable -- all enum values handled above
}

} // namespace engine
