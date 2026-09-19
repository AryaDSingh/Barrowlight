#pragma once

namespace engine {

// Placeholder. Concrete StatusEffect types -- Poison, Stun, Haste, etc. --
// get designed in Prompt 10, alongside the abilities that apply them.
// This exists now purely so Actor has a statusEffects_ member and
// compiles.
//
// Treat this as disposable, not a foundation to build on: expect it to be
// replaced wholesale in Prompt 10, not incrementally extended.
class StatusEffects {
public:
    bool empty() const { return true; }
};

} // namespace engine
