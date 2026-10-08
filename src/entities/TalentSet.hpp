#pragma once

#include "entities/Stats.hpp"
#include <algorithm>
#include <cstddef>
#include <vector>

#include "entities/Talent.hpp"
#include "entities/TalentCatalog.hpp"

namespace engine {

// Tracks which talents an Actor knows, their ranks, each one's current
// cooldown and the hotbar.
//
// Deliberately doesn't know how to *apply* a talent's effect -- that's
// TalentEffects' job. TalentSet only owns per-actor state (which talents
// are known, how long until each is ready again).
// How much a passive's flat number has grown with its attribute.
inline int passiveGrowth(const Talent& t, const Stats& stats) {
    if (t.scalePer<=0) return 0;
    const int value=t.scaleHighest ? std::max({stats.strength,stats.dexterity,stats.intelligence})
        : t.scalingStat==ScalingStat::Dexterity ? stats.dexterity : t.scalingStat==ScalingStat::Intelligence ? stats.intelligence : stats.strength;
    return std::max(0,value)/t.scalePer;
}

class TalentSet {
public:
    TalentSet() = default;
    explicit TalentSet(std::vector<Talent> knownTalents);

    // Appends a new talent to what's already known, with a fresh 0
    // cooldown -- immediately usable, a deliberate reward feel rather
    // than making a person wait out an arbitrary cooldown on something
    // they just unlocked. Safe to call mid-game: this only ever
    // *appends* (never inserts or reorders), so every existing talent's
    // index -- and therefore every place cooldowns are tracked or
    // referenced by index during live play -- stays
    // exactly where it was. (Saves use stable IDs, not indices.)
    void learnTalent(Talent talent);

    const std::vector<Talent>& knownTalents() const { return knownTalents_; }
    Talent effectiveTalent(std::size_t index) const;
    int rank(std::size_t index) const { return index < ranks_.size() ? ranks_[index] : 0; }
    void setRank(std::size_t index, int value) { if (index < ranks_.size()) ranks_[index] = std::clamp(value, 1, kMaxTalentRank); }
    int rankOf(const std::string& id) const {
        for (std::size_t i=0; i<knownTalents_.size(); ++i) if (knownTalents_[i].id==id) return rank(i);
        return 0;
    }
    // With the owner's attributes: each passive's flat number grows with them.
    int passiveValue(PassiveKind kind, const Stats& stats) const {
        int value=0;
        for (std::size_t i=0;i<knownTalents_.size();++i) {
            const auto* d=findTalentDefinition(knownTalents_[i].id);
            const auto& t=d ? d->atRank(rank(i)) : knownTalents_[i];
            if (t.passiveKind==kind) value+=t.passiveMagnitude+passiveGrowth(t,stats);
        }
        return value;
    }
    // The base numbers alone, before any growth.
    int passiveValue(PassiveKind kind) const {
        int value=0;
        for (std::size_t i=0;i<knownTalents_.size();++i) {
            const auto* d=findTalentDefinition(knownTalents_[i].id);
            const auto& t=d ? d->atRank(rank(i)) : knownTalents_[i];
            if (t.passiveKind==kind) value+=t.passiveMagnitude;
        }
        return value;
    }
    std::vector<std::string>& hotbar() { return hotbar_; }
    const std::vector<std::string>& hotbar() const { return hotbar_; }
    std::optional<std::size_t> hotbarIndex(std::size_t slot) const {
        if (slot >= hotbar_.size()) return std::nullopt;
        for (std::size_t i=0;i<knownTalents_.size();++i) if (knownTalents_[i].id==hotbar_[slot] && !knownTalents_[i].passive) return i;
        return std::nullopt;
    }
    void bind(const std::string& id, std::size_t slot) {
        if (slot>=18) return;
        hotbar_.resize(18);
        for (auto& binding:hotbar_) if (binding==id) binding.clear();
        hotbar_[slot]=id;
    }
    bool empty() const { return knownTalents_.empty(); }

    bool isReady(std::size_t index) const;
    int cooldownRemaining(std::size_t index) const;

    // Puts the talent at `index` on its full cooldown. Called after a
    // successful activation.
    void startCooldown(std::size_t index);

    // Directly sets talent `index`'s remaining cooldown to `turns`. Used
    // only by save/load to restore exact cooldown state; normal gameplay
    // uses startCooldown()/tickCooldowns() instead, never this.
    void setCooldownRemaining(std::size_t index, int turns);

    // Decrements every talent's remaining cooldown by one (floored at
    // 0). Called once per turn the owning Actor takes -- including
    // turns spent moving, not just turns spent casting, since cooldowns
    // progress with time passing, not specifically with casting.
    void tickCooldowns();

    // Clears every cooldown back to 0 (the sandbox's "ready all
    // cooldowns", and tests).
    void resetCooldowns();

private:
    std::vector<Talent> knownTalents_;
    std::vector<int> cooldownsRemaining_; // parallel to knownTalents_
    std::vector<int> ranks_;
    std::vector<std::string> hotbar_;
};

} // namespace engine
