#pragma once

namespace engine {

// Placeholder. The real Talent type -- cooldowns, activation, data-driven
// definitions -- gets designed in Prompt 9, once there's an actual
// playable class to design talents for. This exists now purely so Actor
// has a talents_ member and compiles.
//
// Treat this as disposable, not a foundation to build on: expect it to be
// replaced wholesale in Prompt 9, not incrementally extended.
class TalentSet {
public:
    bool empty() const { return true; }
};

} // namespace engine
