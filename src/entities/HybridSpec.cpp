#include "entities/HybridSpec.hpp"

#include "entities/PlayerClassFactory.hpp"

namespace engine {

bool isHybridEligible(PlayerClass cls) {
    return cls == PlayerClass::Fighter || cls == PlayerClass::Sorcerer;
}

PlayerClass hybridPoolClass(PlayerClass cls) {
    switch (cls) {
        case PlayerClass::Fighter:
            return PlayerClass::Sorcerer;
        case PlayerClass::Sorcerer:
            return PlayerClass::Fighter;
        case PlayerClass::Thief:
        case PlayerClass::Spellblade:
            return cls; // not eligible -- see isHybridEligible(); an inert default
    }
    return cls; // unreachable
}

std::vector<Talent> fullKitForClass(PlayerClass cls) {
    std::vector<Talent> kit = talentSetForClass(cls).knownTalents();
    if (const std::optional<Talent> levelFour = talentUnlockedAtLevel(cls, 4);
        levelFour.has_value()) {
        kit.push_back(*levelFour);
    }
    if (const std::optional<Talent> levelSeven = talentUnlockedAtLevel(cls, 7);
        levelSeven.has_value()) {
        kit.push_back(*levelSeven);
    }
    return kit;
}

std::vector<Talent> availableHybridPicks(PlayerClass cls, const TalentSet& known) {
    std::vector<Talent> available;
    if (!isHybridEligible(cls)) {
        return available; // empty -- not a hybrid-eligible class at all
    }

    const std::vector<Talent> pool = fullKitForClass(hybridPoolClass(cls));
    for (const Talent& candidate : pool) {
        bool alreadyKnown = false;
        for (const Talent& existing : known.knownTalents()) {
            if (existing.name == candidate.name) {
                alreadyKnown = true;
                break;
            }
        }
        if (!alreadyKnown) {
            available.push_back(candidate);
        }
    }
    return available;
}

std::vector<Talent> pickedHybridTalents(PlayerClass cls, const TalentSet& known) {
    std::vector<Talent> picked;
    if (!isHybridEligible(cls)) {
        return picked; // empty -- not a hybrid-eligible class at all
    }

    const std::vector<Talent> pool = fullKitForClass(hybridPoolClass(cls));
    for (const Talent& candidate : pool) {
        for (const Talent& existing : known.knownTalents()) {
            if (existing.name == candidate.name) {
                picked.push_back(candidate);
                break;
            }
        }
    }
    return picked;
}

} // namespace engine
