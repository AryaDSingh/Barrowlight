#include "entities/PlayerClassFactory.hpp"

#include "entities/FighterTalents.hpp"
#include "entities/SorcererTalents.hpp"
#include "entities/SpellbladeTalents.hpp"
#include "entities/ThiefTalents.hpp"

namespace engine {

Stats statsForClass(PlayerClass cls) {
    Stats stats;
    switch (cls) {
        case PlayerClass::Spellblade: {
            // The original Str+Int hybrid (Prompt 9) -- deliberately
            // left on the old baseline-10 model and its old hand-picked
            // maxMana (20, previously computed as 12 base +
            // manaBonusFromIntelligence(18)) rather than migrated to
            // the new attribute system. There is currently no way to
            // actually reach this class in play (still reserved for a
            // future unlock, never offered at character select) --
            // fixing it up to match the new system is a no-op until
            // that unlock mechanism actually exists, so it's left as a
            // known, harmless inconsistency rather than busywork now.
            stats.hp = 30;
            stats.maxHp = 30;
            stats.strength = 14;
            stats.dexterity = 10;
            stats.intelligence = 18;
            stats.maxMana = 20;
            stats.mana = stats.maxMana;
            break;
        }
        case PlayerClass::Warrior: {
            // Pure Strength -- originally "Fighter" (Prompt 19),
            // renamed again as part of the full attribute-system
            // redesign. Starting hp/mana are hand-picked literals, not
            // derived from Strength/Intelligence at all -- the new
            // system deliberately keeps starting resource pools and
            // starting attribute investment as two independent
            // decisions (see ARCHITECTURE_DECISIONS.md). The 6/2/2
            // Str/Dex/Int split is used purely for ability-scaling
            // damage and secondary effects (dodge/crit from Dex) from
            // this point on -- it never retroactively affects maxHp or
            // maxMana. Only points earned through leveling do that (see
            // PlayerLeveling.hpp).
            stats.hp = 30;
            stats.maxHp = 30;
            stats.mana = 10;
            stats.maxMana = 10;
            stats.strength = 6;
            stats.dexterity = 2;
            stats.intelligence = 2;
            break;
        }
        case PlayerClass::Thief: {
            // Pure Dexterity, ranged/evasive. Not to be confused with
            // MonsterType::Archer, the Kiter-AI enemy -- separate
            // enums, no code collision. Same "hand-picked pools,
            // separate from the 6/2/2 split" reasoning as Warrior
            // above.
            stats.hp = 25;
            stats.maxHp = 25;
            stats.mana = 15;
            stats.maxMana = 15;
            stats.strength = 2;
            stats.dexterity = 6;
            stats.intelligence = 2;
            break;
        }
        case PlayerClass::Mage: {
            // Pure Intelligence -- originally "Sorcerer" (Prompt 19),
            // renamed again as part of the attribute-system redesign.
            // Same "hand-picked pools, separate from the 6/2/2 split"
            // reasoning as Warrior above -- the largest starting mana
            // pool of the three (20), reflecting a genuinely spell-
            // hungry identity, same as it always has, just no longer
            // computed from Intelligence directly.
            stats.hp = 20;
            stats.maxHp = 20;
            stats.mana = 20;
            stats.maxMana = 20;
            stats.strength = 2;
            stats.dexterity = 2;
            stats.intelligence = 6;
            break;
        }
    }
    return stats;
}

TalentSet talentSetForClass(PlayerClass cls) {
    switch (cls) {
        case PlayerClass::Spellblade:
            return TalentSet(spellbladeTalents());
        case PlayerClass::Warrior:
            return TalentSet(fighterTalents());
        case PlayerClass::Thief:
            return TalentSet(thiefTalents());
        case PlayerClass::Mage:
            return TalentSet(sorcererTalents());
    }
    return TalentSet(); // unreachable -- all enum values handled above
}

std::optional<Talent> talentUnlockedAtLevel(PlayerClass cls, int level) {
    switch (cls) {
        case PlayerClass::Spellblade:
            return std::nullopt; // reserved for its own separate unlock mechanism, not this one
        case PlayerClass::Warrior:
            return fighterTalentUnlockedAtLevel(level);
        case PlayerClass::Thief:
            return thiefTalentUnlockedAtLevel(level);
        case PlayerClass::Mage:
            return sorcererTalentUnlockedAtLevel(level);
    }
    return std::nullopt; // unreachable -- all enum values handled above
}

} // namespace engine
