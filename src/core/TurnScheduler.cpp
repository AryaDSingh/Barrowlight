// The energy-based turn scheduler (see TurnScheduler.hpp).

#include "core/TurnScheduler.hpp"

#include <algorithm>
#include <stdexcept>

namespace engine {

void TurnScheduler::add(Actor& actor) {
    entries_.push_back(Entry{&actor, 0});
}

void TurnScheduler::remove(const Actor& actor) {
    entries_.erase(
        std::remove_if(entries_.begin(), entries_.end(),
                        [&actor](const Entry& e) { return e.actor == &actor; }),
        entries_.end());
}

Actor& TurnScheduler::nextTurn() {
    if (entries_.empty()) {
        throw std::logic_error(
            "TurnScheduler::nextTurn() called with no actors registered");
    }

    while (true) {
        for (auto& entry : entries_) {
            entry.energy += entry.actor->stats().speed * speedPercent(entry.actor->statusEffects()) / 100;
        }

        // Among everyone who crossed the threshold this round, the one
        // with the most energy acts first -- this only matters when two
        // actors cross in the same round, which happens for exact
        // multiples of each other's speed.
        Entry* ready = nullptr;
        for (auto& entry : entries_) {
            if (entry.energy >= kActionThreshold &&
                (ready == nullptr || entry.energy > ready->energy)) {
                ready = &entry;
            }
        }

        if (ready != nullptr) {
            ready->energy -= kActionThreshold;
            return *ready->actor;
        }
    }
}

} // namespace engine
