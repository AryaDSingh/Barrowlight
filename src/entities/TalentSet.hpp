#pragma once

#include <cstddef>
#include <vector>

#include "entities/Talent.hpp"

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

    const std::vector<Talent>& knownTalents() const { return knownTalents_; }
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
};

} // namespace engine
