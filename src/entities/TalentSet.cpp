#include "entities/TalentSet.hpp"

#include <algorithm>

namespace engine {

TalentSet::TalentSet(std::vector<Talent> knownTalents)
    : knownTalents_(std::move(knownTalents)),
      cooldownsRemaining_(knownTalents_.size(), 0), ranks_(knownTalents_.size(), 1) {
    for (auto& talent : knownTalents_) { talent.tags = talentTags(talent); if (!talent.passive) hotbar_.push_back(talent.id); }
}

void TalentSet::learnTalent(Talent talent) {
    if (!talent.id.empty())
        for (const auto& known : knownTalents_) if (known.id == talent.id) return;
    talent.tags = talentTags(talent);
    if (!talent.passive) {
        const auto free=std::find(hotbar_.begin(),hotbar_.end(),std::string{});
        if (free!=hotbar_.end()) *free=talent.id;
        else if (hotbar_.size()<18) hotbar_.push_back(talent.id);
    }
    knownTalents_.push_back(std::move(talent));
    ranks_.push_back(1);
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
        if (isImbueVariant(knownTalents_[index].id) || knownTalents_[index].id=="spellblade.imbue")
            for (std::size_t i=0;i<knownTalents_.size();++i) if (isImbueVariant(knownTalents_[i].id) || knownTalents_[i].id=="spellblade.imbue")
                cooldownsRemaining_[i]=cooldownsRemaining_[index];
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

Talent TalentSet::effectiveTalent(std::size_t index) const {
    if (index>=knownTalents_.size()) return {};
    const auto* d=findTalentDefinition(knownTalents_[index].id);
    Talent t=d ? d->ranks[rank(index)-1] : knownTalents_[index];
    if (t.scalingCooldown<0) t.scalingCooldown=t.cooldownTurns;
    if (!t.passive && isSpell(t) && t.manaCost>0) {
        const int reduction=passiveValue(PassiveKind::ArcaneEfficiency);
        t.manaCost=std::max(1,t.manaCost*(100-reduction)/100);
    }
    if (t.tree==TalentTree::Lightning && !t.consumeShock) {
        const int duration=passiveValue(PassiveKind::StaticCharge);
        if (duration) t.onHitEffect=StatusEffectInstance{StatusEffectType::Shock,duration,0};
    }
    t.movementBurn=passiveValue(PassiveKind::Kindle);
    return t;
}
} // namespace engine
