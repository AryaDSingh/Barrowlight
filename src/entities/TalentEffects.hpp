#pragma once

#include "entities/Talent.hpp"

namespace engine {

class Actor;

// Applies `talent`'s damage from `attacker` to `target`'s Stats::hp,
// including the attacker's attribute bonus (Prompt 14:
// physicalDamageBonus/magicDamageBonus, chosen by talent.damageType),
// the conditional/execute bonus (Execution) if the target qualifies,
// and a dodge roll on the target's dexterity. Returns false (and
// applies nothing) if the attack was dodged, true if it landed --
// callers need this to print the right message, not assume every call
// necessarily dealt damage.
//
// Deliberately the one place damage gets applied -- kept out of Talent
// (pure data) and TalentSet (cooldown bookkeeping) so each stays focused
// on one job.
//
// Doesn't touch cooldowns, mana/hp costs, or targeting -- those are the
// caller's responsibility (Application resolves who's affected, since
// it's the one thing that currently knows about every Actor in the
// level; for an AreaAroundTarget/AreaAroundSelf talent, this gets called
// once per affected Actor).
bool applyTalentDamage(const Talent& talent, Actor& attacker, Actor& target);

// Applies `talent`'s healing to `target`'s Stats::hp, added rather than
// subtracted and capped at maxHp, including the caster's own
// magicDamageBonus(intelligence) -- reusing the same "how much your
// intelligence empowers your spells" formula that governs magic damage,
// applied to restoration instead of harm. No dodge roll: avoiding your
// own beneficial spell doesn't make sense the way avoiding an incoming
// attack does, so this always lands.
void applyTalentHeal(const Talent& talent, Actor& caster, Actor& target);

// Applies talent.selfBuffEffect directly to the caster (e.g. the
// Marauder's Rallying Cry applying Empowered to themselves). A no-op if
// selfBuffEffect is unset. No targeting resolution, no dodge, no
// attribute bonus -- a status effect isn't a damage number to scale.
void applyTalentSelfBuff(const Talent& talent, Actor& caster);

} // namespace engine
