#include "entities/TalentSet.hpp"

#include <algorithm>

namespace engine {

TalentSet::TalentSet(std::vector<Talent> knownTalents)
    : knownTalents_(std::move(knownTalents)),
      cooldownsRemaining_(knownTalents_.size(), 0) {
    for (auto& talent : knownTalents_) talent.tags = talentTags(talent);
}

void TalentSet::learnTalent(Talent talent) {
    if (!talent.id.empty())
        for (const auto& known : knownTalents_) if (known.id == talent.id) return;
    talent.tags = talentTags(talent);
    knownTalents_.push_back(std::move(talent));
    cooldownsRemaining_.push_back(0);
}

bool TalentSet::isReady(std::size_t index) const {
    return index < cooldownsRemaining_.size() && cooldownsRemaining_[index] <= 0;
}

int TalentSet::cooldownRemaining(std::size_t index) const {
    return index < cooldownsRemaining_.size() ? cooldownsRemaining_[index] : 0;
}

void TalentSet::startCooldown(std::size_t index) {
    if (index < knownTalents_.size()) {
        cooldownsRemaining_[index] = effectiveTalent(index).cooldownTurns;
    }
}

void TalentSet::setCooldownRemaining(std::size_t index, int turns) {
    if (index < cooldownsRemaining_.size()) {
        cooldownsRemaining_[index] = turns;
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

const RuneInstance* TalentSet::attachedRune(std::size_t index) const {
    if (index >= knownTalents_.size() || knownTalents_[index].id.empty()) return nullptr;
    for (const auto& rune : runes_) if (rune.talentId == knownTalents_[index].id) return &rune;
    return nullptr;
}
Talent TalentSet::effectiveTalent(std::size_t index) const {
    if (index >= knownTalents_.size()) return {};
    const auto* rune = attachedRune(index);
    return resolveEffectiveTalent(knownTalents_[index], rune ? rune->definitionId : "");
}
bool TalentSet::attachRune(std::size_t runeIndex, std::size_t talentIndex) {
    if (runeIndex >= runes_.size() || talentIndex >= knownTalents_.size()) return false;
    const auto& talent = knownTalents_[talentIndex];
    if (runes_[runeIndex].talentId == talent.id || !runeUnavailableReason(talent, runes_[runeIndex].definitionId).empty()) return false;
    for (auto& rune : runes_) if (rune.talentId == talent.id) rune.talentId.clear();
    runes_[runeIndex].talentId = talent.id; // displaced rune stays owned, now unbound
    return true;
}
bool TalentSet::removeRune(std::size_t talentIndex) {
    if (talentIndex >= knownTalents_.size()) return false;
    for (auto& rune : runes_) if (!rune.talentId.empty() && rune.talentId == knownTalents_[talentIndex].id) {
        rune.talentId.clear(); return true;
    }
    return false;
}

} // namespace engine
