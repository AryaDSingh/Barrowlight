#include "entities/PlayerClassFactory.hpp"


namespace engine {

Stats statsForClass(PlayerClass cls) {
    Stats stats;
    switch (cls) {
        case PlayerClass::Spellblade: {
            // Not offered at character selection; these stats exist for the
            // isolated combat tests (tests/fixtures/), which compute their
            // expected damage from them.
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
            // Pure Strength. Starting hp/mana are hand-picked, not derived
            // from Strength/Intelligence: starting resource pools and the
            // starting attribute spread are two independent decisions. The 6/2/2
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
            // Pure Intelligence. Hand-picked pools as for the Warrior: the
            // largest starting mana of the three (20), for a spell-hungry
            // origin.
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

} // namespace engine
