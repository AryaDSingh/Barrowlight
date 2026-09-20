#pragma once

#include <string>

namespace engine {

enum class TalentTree {
    Blade,
    Flame,
};

enum class TargetingMode {
    Self,               // affects the caster only (Blink, Immolate's origin)
    AdjacentEnemy,      // must target an enemy in one of the 4 adjacent tiles
    RangedEnemyInSight, // any enemy within the caster's current field of view
};

enum class EffectShape {
    SingleTarget,      // affects only the resolved target
    AreaAroundTarget,  // affects the resolved target and anything within areaRadius of it
    AreaAroundSelf,    // affects anything within areaRadius of the caster
    Movement,          // relocates the caster; power/areaRadius unused
};

// A talent's full definition -- deliberately POD-like data, no behavior
// of its own. The generic logic that interprets these fields lives in
// TalentEffects (application) and Application (targeting resolution),
// not here. See ARCHITECTURE_DECISIONS.md for why this counts as
// "data-driven" even without an external file: logic and data are
// cleanly separated, the data table is trivially swappable for a file
// loader later, but no such loader exists yet.
struct Talent {
    std::string name;
    std::string description;
    TalentTree tree = TalentTree::Blade;
    TargetingMode targeting = TargetingMode::AdjacentEnemy;
    EffectShape shape = EffectShape::SingleTarget;

    int manaCost = 0;
    int hpCost = 0;       // Reckless Lunge: costs the caster's own hp too -- risk/reward
    int cooldownTurns = 0;

    int power = 0;        // base damage; unused for Movement
    int areaRadius = 0;   // for AreaAroundTarget / AreaAroundSelf
    int moveDistance = 0; // for Movement (Blink)

    // Execution-style conditional bonus: if the target's hp fraction is
    // at or below this threshold, `power` is multiplied by
    // conditionalMultiplier instead of used as-is. 0 threshold means no
    // conditional effect (the common case).
    float conditionalHpFraction = 0.f;
    int conditionalMultiplier = 1;
};

} // namespace engine
