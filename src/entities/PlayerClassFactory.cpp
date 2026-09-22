#include "entities/PlayerClassFactory.hpp"

#include "entities/ArcherTalents.hpp"
#include "entities/AttributeFormulas.hpp"
#include "entities/MarauderTalents.hpp"
#include "entities/SpellbladeTalents.hpp"

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
            // genuinely formula-driven (see Prompt 14).
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
        case PlayerClass::Marauder: {
            // Pure Strength -- Intelligence and Dexterity both sit
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
        case PlayerClass::Archer: {
            // Pure Dexterity -- the mirror of Marauder's pure Strength.
            // Dexterity 20 hits the dodge-chance cap exactly (30%, see
            // AttributeFormulas::dodgeChance) -- the maximum possible
            // evasion in the game, this class's entire defensive
            // identity, since its hp pool is deliberately the lowest of
            // any class (24, versus the Spellblade's 30 and the
            // Marauder's 45): survives by not getting hit, not by
            // soaking hits. Strength (14) is moderate, not a dump stat
            // -- someone has to actually draw the bow -- while
            // Intelligence (6) sits below baseline on purpose, a real
            // (if modest) penalty reinforcing "not a caster at all."
            // maxMana == 8 base + manaBonusFromIntelligence(6) == 0
            // (floored, not negative), landing on exactly 8 -- the
            // smallest pool of any class.
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
    }
    return stats;
}

TalentSet talentSetForClass(PlayerClass cls) {
    switch (cls) {
        case PlayerClass::Spellblade:
            return TalentSet(spellbladeTalents());
        case PlayerClass::Marauder:
            return TalentSet(marauderTalents());
        case PlayerClass::Archer:
            return TalentSet(archerTalents());
    }
    return TalentSet(); // unreachable -- all enum values handled above
}

} // namespace engine
