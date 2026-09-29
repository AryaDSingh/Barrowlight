#include "entities/MonsterTier.hpp"

namespace engine {

MonsterTier tierForLevel(int playerLevel) {
    if (playerLevel <= 2) {
        return MonsterTier::Base;
    }
    if (playerLevel <= 6) {
        return MonsterTier::Elite;
    }
    return MonsterTier::Nightmare; // levels 7-10
}

float hpMultiplierForTier(MonsterTier tier) {
    switch (tier) {
        case MonsterTier::Base:
            return 1.f;
        case MonsterTier::Elite:
            return 1.5f;
        case MonsterTier::Nightmare:
            return 2.2f;
    }
    return 1.f; // unreachable -- all enum values handled above
}

float damageMultiplierForTier(MonsterTier tier) {
    switch (tier) {
        case MonsterTier::Base:
            return 1.f;
        case MonsterTier::Elite:
            return 1.4f;
        case MonsterTier::Nightmare:
            return 1.8f;
    }
    return 1.f; // unreachable
}

float xpMultiplierForTier(MonsterTier tier) {
    switch (tier) {
        case MonsterTier::Base:
            return 1.f;
        case MonsterTier::Elite:
            return 2.f;
        case MonsterTier::Nightmare:
            return 4.f;
    }
    return 1.f; // unreachable
}

const char* namePrefixForTier(MonsterTier tier) {
    switch (tier) {
        case MonsterTier::Base:
            return "";
        case MonsterTier::Elite:
            return "Elite ";
        case MonsterTier::Nightmare:
            return "Rare ";
    }
    return ""; // unreachable
}

} // namespace engine
