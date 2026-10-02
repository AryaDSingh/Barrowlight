#pragma once

#include <cstddef>
#include <vector>

#include "entities/Talent.hpp"
#include "entities/TalentCatalog.hpp"

namespace engine {

// Tracks which talents an Actor knows and each one's current cooldown.
// This is the wholesale replacement flagged back in Prompt 3 ("expect
// this to be replaced wholesale, not incrementally extended") -- the
// stub's empty() method is gone; this is real bookkeeping now.
//
// Deliberately doesn't know how to *apply* a talent's effect -- that's
// TalentEffects' job. TalentSet only owns per-actor state (which talents
// are known, how long until each is ready again).
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
    // exactly where it was. Saves use stable IDs. Prompt 23's level-gated talent unlocks are
    // the reason this exists.
    void learnTalent(Talent talent);

    const std::vector<Talent>& knownTalents() const { return knownTalents_; }
    Talent effectiveTalent(std::size_t index) const;
    int rank(std::size_t index) const { return index < ranks_.size() ? ranks_[index] : 0; }
    void setRank(std::size_t index, int value) { if (index < ranks_.size()) ranks_[index] = std::clamp(value, 1, kMaxTalentRank); }
    int rankOf(const std::string& id) const {
        for (std::size_t i=0; i<knownTalents_.size(); ++i) if (knownTalents_[i].id==id) return rank(i);
        return 0;
    }
    int passiveValue(PassiveKind kind) const {
        int value=0;
        for (std::size_t i=0;i<knownTalents_.size();++i) {
            const auto* d=findTalentDefinition(knownTalents_[i].id);
            const auto& t=d ? d->ranks[rank(i)-1] : knownTalents_[i];
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

    // Clears every cooldown back to 0. Used when a level regenerates
    // (Prompt 8's debug feature) -- a fresh dungeon should mean a
    // genuinely fresh start, not carried-over cooldown state from
    // whatever was tested before.
    void resetCooldowns();

private:
    std::vector<Talent> knownTalents_;
    std::vector<int> cooldownsRemaining_; // parallel to knownTalents_
    std::vector<int> ranks_;
    std::vector<std::string> hotbar_;
};

} // namespace engine
