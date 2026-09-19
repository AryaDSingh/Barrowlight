#pragma once

#include <vector>

#include "entities/Actor.hpp"

namespace engine {

// Energy/speed-based turn order, ToME-style: every registered actor
// accumulates energy each tick in proportion to its Stats::speed, and
// acts once its energy crosses kActionThreshold. A double-speed actor
// acts roughly twice as often as a baseline (speed 100) actor -- no
// explicit turn-counting needed, speed alone determines frequency.
//
// Deliberately doesn't know about Action, Map, or AIBehavior yet. It only
// answers "whose turn is it" -- nothing about what that actor then does.
// Wiring this into real gameplay is Prompt 5, not this one (see
// ROADMAP.md).
class TurnScheduler {
public:
    static constexpr int kActionThreshold = 1000;

    // Registers an actor for scheduling. TurnScheduler does not own the
    // Actor -- whoever does (eventually a level/Map) must call remove()
    // before the Actor is destroyed, or nextTurn() could return a
    // dangling reference.
    void add(Actor& actor);
    void remove(const Actor& actor);

    // Advances simulated time until exactly one actor's energy has
    // crossed the action threshold, deducts the threshold from that
    // actor's energy (any overflow carries over -- it isn't discarded),
    // and returns that actor. Call this once per turn. Throws if no
    // actors are registered.
    Actor& nextTurn();

private:
    struct Entry {
        Actor* actor;
        int energy = 0;
    };

    std::vector<Entry> entries_;
};

} // namespace engine
