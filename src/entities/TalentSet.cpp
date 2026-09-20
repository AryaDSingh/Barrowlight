#include "entities/TalentSet.hpp"

#include <algorithm>

namespace engine {

TalentSet::TalentSet(std::vector<Talent> knownTalents)
    : knownTalents_(std::move(knownTalents)),
      cooldownsRemaining_(knownTalents_.size(), 0) {}

bool TalentSet::isReady(std::size_t index) const {
    return index < cooldownsRemaining_.size() && cooldownsRemaining_[index] <= 0;
}

int TalentSet::cooldownRemaining(std::size_t index) const {
    return index < cooldownsRemaining_.size() ? cooldownsRemaining_[index] : 0;
}

void TalentSet::startCooldown(std::size_t index) {
    if (index < knownTalents_.size()) {
        cooldownsRemaining_[index] = knownTalents_[index].cooldownTurns;
    }
}

void TalentSet::tickCooldowns() {
    for (int& remaining : cooldownsRemaining_) {
        remaining = std::max(0, remaining - 1);
    }
}

void TalentSet::resetCooldowns() {
    std::fill(cooldownsRemaining_.begin(), cooldownsRemaining_.end(), 0);
}

} // namespace engine
