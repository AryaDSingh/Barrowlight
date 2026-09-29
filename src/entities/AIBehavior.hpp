#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "core/Position.hpp"
#include "entities/AttributeFormulas.hpp"
#include "entities/MonsterTier.hpp"
#include "entities/MonsterType.hpp"
#include "entities/StatusEffects.hpp"

namespace engine {

class Actor;
class Map;

enum class AIActionType {
    Move,
    Attack,      // a basic, uncooldowned attack (see MonsterAttackProfile)
    UseAbility,  // a cooldown-gated special move, from the actor's own TalentSet
    SelfBuff,    // applies effectToApply directly to the acting actor itself
    Summon,      // spawns a new monster at movePosition -- see summonType/summonTier
    Wait,
};

// What an AIBehavior decided to do this turn. Application executes this
// -- AIBehavior only decides, it never mutates anything itself, matching
// how the player's input handling works (Application applies moves/
// talent effects, not the input code). For Attack, the behavior packages
// the actual damage/on-hit-effect outcome itself (including rolling any
// on-hit chance) rather than Application reaching back into the
// behavior's private MonsterAttackProfile to figure out what happened.
struct AIDecision {
    AIActionType type = AIActionType::Wait;
    Position movePosition{};    // for Move
    Actor* target = nullptr;    // for Attack (always the player today) / UseAbility
                                 // (an ally for Support, the player for AoEBomber)
    std::size_t abilityIndex = 0;   // for UseAbility, which of the actor's own talents
    int healAmount = 0;
    int attackPower = 0;             // for Attack (or a damaging UseAbility), already resolved

    // For Summon: which monster type to create at movePosition, and at
    // what tier. Always Base tier in practice today -- the Lich's own
    // summoned Skeletons aren't meant to individually scale the way a
    // dungeon's regular roster does (see LichBehavior) -- but tracked
    // as real data here, not hardcoded in Application's executeAIDecision,
    // the same "AIBehavior decides, Application just executes" split
    // every other action type already follows.
    MonsterType summonType = MonsterType::Skeleton;
    MonsterTier summonTier = MonsterTier::Base;

    // Which attribute attackPower scales with. Defaults to Strength,
    // matching MonsterAttackProfile's own default -- Chaser and Kiter
    // (both MonsterAttackProfile-backed) never need to set this
    // explicitly. AoEBomber and BossBehavior's blast phase set it to
    // Intelligence directly in their own decision-building code, not
    // via a constructor parameter -- there's exactly one Bomber and one
    // boss, so this is a fixed characteristic of that specific ability,
    // not data that varies across instances the way
    // MonsterAttackProfile's scalingStat genuinely does across the
    // Chaser-based roster. Treated as Filler tier for the damage-bonus
    // calculation, same as MonsterAttackProfile's own attacks -- see
    // that struct's comment for why.
    ScalingStat scalingStat = ScalingStat::Strength;

    std::optional<StatusEffectInstance> effectToApply; // for Attack (on-hit proc), UseAbility
                                                          // (a buff), or SelfBuff
                                                          // (unset if nothing applies)

    // Optional flavor text Application should print verbatim (e.g. a
    // boss announcing a phase transition). Empty means nothing to print.
    // Kept generic rather than boss-specific, so Application doesn't
    // need special-case knowledge of which behaviors are "bosses."
    std::string announcement;
};

// Strategy-pattern interface for monster decision-making. A Player holds
// no AIBehavior (nullptr, see Player.hpp); a Monster is always given a
// concrete strategy object -- what makes enemy types feel distinct is
// which Stats, which on-hit effects, and which AIBehavior (parameterized
// with its own numbers) get plugged into an otherwise identical Monster,
// not one class per enemy type (see ARCHITECTURE_DECISIONS.md).
//
// `self`, `map`, `player`, and `Actor` are only forward-declared here,
// not #included -- deliberately, to avoid a circular include (Actor.hpp
// already includes this header to declare its ai_ member).
class AIBehavior {
public:
    virtual ~AIBehavior() = default;

    // `allies` is every other Monster currently in the level (not
    // including self) -- only Support actually uses it (to find someone
    // to buff); everyone else ignores it. `player` is non-const because
    // a returned AIDecision may need to reference it as a mutable
    // Attack/UseAbility target for Application to act on later; this
    // function itself doesn't mutate anything.
    virtual AIDecision decideAction(const Actor& self, const Map& map, Actor& player,
                                     const std::vector<Actor*>& allies) = 0;
};

} // namespace engine
