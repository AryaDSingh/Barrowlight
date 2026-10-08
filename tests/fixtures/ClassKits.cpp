#include "fixtures/ClassKits.hpp"

#include "fixtures/MageTalents.hpp"
#include "fixtures/SpellbladeTalents.hpp"
#include "fixtures/ThiefTalents.hpp"
#include "fixtures/WarriorTalents.hpp"

namespace engine {

TalentSet talentSetForClass(PlayerClass cls) {
    switch (cls) {
        case PlayerClass::Spellblade:
            return TalentSet(spellbladeTalents());
        case PlayerClass::Warrior:
            return TalentSet(warriorTalents());
        case PlayerClass::Thief:
            return TalentSet(thiefTalents());
        case PlayerClass::Mage:
            return TalentSet(mageTalents());
    }
    return TalentSet(); // unreachable -- all enum values handled above
}

std::optional<Talent> talentUnlockedAtLevel(PlayerClass cls, int level) {
    switch (cls) {
        case PlayerClass::Spellblade:
            return std::nullopt; // the Spellblade kit has no unlocks
        case PlayerClass::Warrior:
            return warriorTalentUnlockedAtLevel(level);
        case PlayerClass::Thief:
            return thiefTalentUnlockedAtLevel(level);
        case PlayerClass::Mage:
            return mageTalentUnlockedAtLevel(level);
    }
    return std::nullopt; // unreachable -- all enum values handled above
}

std::vector<Talent> fullKitForClass(PlayerClass cls) {
    std::vector<Talent> kit = talentSetForClass(cls).knownTalents();
    for (const int level : {2, 4, 7})
        if (const auto unlocked = talentUnlockedAtLevel(cls, level)) kit.push_back(*unlocked);
    return kit;
}

} // namespace engine
