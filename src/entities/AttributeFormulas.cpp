#include "entities/AttributeFormulas.hpp"

#include <algorithm>
#include <random>

namespace engine {

namespace {
constexpr float kDodgePercentPerPoint = 0.005f;
constexpr float kDodgeCap = 0.25f;
constexpr float kCritPercentPerPoint = 0.005f;
constexpr float kBaseCritChance = 0.05f; // every actor, player and monster alike, starts here
constexpr float kCritDamageMultiplier = 1.5f;

constexpr int kAbilityDamagePerPoints = 5; // +1 damage per this many points in the scaling stat

float tierMultiplier(AbilityCooldownTier tier) {
    switch (tier) {
        case AbilityCooldownTier::Filler:
            return 1.0f;
        case AbilityCooldownTier::Core:
            return 1.5f;
        case AbilityCooldownTier::Power:
            return 2.0f;
        case AbilityCooldownTier::Signature:
            return 2.5f;
    }
    return 1.0f; // unreachable -- all enum values handled above
}
} // namespace

AbilityCooldownTier tierForCooldown(int cooldownTurns) {
    if (cooldownTurns <= 1) {
        return AbilityCooldownTier::Filler;
    }
    if (cooldownTurns <= 3) {
        return AbilityCooldownTier::Core;
    }
    if (cooldownTurns <= 5) {
        return AbilityCooldownTier::Power;
    }
    return AbilityCooldownTier::Signature;
}

int statValueForScalingStat(ScalingStat stat, int strength, int dexterity, int intelligence) {
    switch (stat) {
        case ScalingStat::Strength:
            return strength;
        case ScalingStat::Dexterity:
            return dexterity;
        case ScalingStat::Intelligence:
            return intelligence;
    }
    return strength; // unreachable -- all enum values handled above
}

int abilityDamageBonus(ScalingStat /*stat*/, int statValue, int cooldownTurns) {
    // `stat` isn't read here -- it already did its job by selecting
    // which of Strength/Dexterity/Intelligence's value the caller
    // passed in as `statValue`. Kept as a parameter anyway (not just
    // taking a bare int) so every call site stays self-documenting
    // about which attribute is actually powering a given talent.
    const float rawBonus =
        static_cast<float>(statValue) / static_cast<float>(kAbilityDamagePerPoints);
    const float tiered = rawBonus * tierMultiplier(tierForCooldown(cooldownTurns));
    return static_cast<int>(tiered); // truncates -- a partial point of bonus doesn't round up
}

float dodgeChance(int dexterity) {
    const float raw = static_cast<float>(dexterity) * kDodgePercentPerPoint;
    return std::clamp(raw, 0.f, kDodgeCap);
}

float critChanceBonus(int dexterity) {
    return static_cast<float>(dexterity) * kCritPercentPerPoint;
}

float critDamageMultiplier() {
    return kCritDamageMultiplier;
}

float critDamageMultiplier(float bonusMultiplier) {
    return kCritDamageMultiplier + bonusMultiplier;
}

bool didDodge(float chance, float roll) {
    return roll < chance;
}

bool rollChance(float probability) {
    static std::mt19937 rng{std::random_device{}()};
    std::uniform_real_distribution<float> roll(0.f, 1.f);
    return didDodge(probability, roll(rng)); // didDodge is really just "roll < chance" --
                                              // reused generically here, not dodge-specific
}

bool rollDodge(int dexterity) {
    return rollChance(dodgeChance(dexterity));
}

bool rollCrit(int dexterity) {
    return rollChance(kBaseCritChance + critChanceBonus(dexterity));
}

bool rollCrit(int dexterity, float bonusCritChance) {
    return rollChance(kBaseCritChance + critChanceBonus(dexterity) + bonusCritChance);
}

} // namespace engine
