#include "entities/AttributeFormulas.hpp"

#include <algorithm>
#include <random>

namespace engine {

namespace {
constexpr int kAttributeBaseline = 10;
constexpr float kDodgePercentPerPoint = 0.03f;
constexpr float kDodgeCap = 0.30f;
} // namespace

int physicalDamageBonus(int strength) {
    return (strength - kAttributeBaseline) / 2;
}

int magicDamageBonus(int intelligence) {
    return (intelligence - kAttributeBaseline) / 2;
}

float dodgeChance(int dexterity) {
    const float raw = static_cast<float>(dexterity - kAttributeBaseline) * kDodgePercentPerPoint;
    return std::clamp(raw, 0.f, kDodgeCap);
}

int manaBonusFromIntelligence(int intelligence) {
    return std::max(0, intelligence - kAttributeBaseline);
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

} // namespace engine
