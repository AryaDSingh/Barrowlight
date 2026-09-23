#pragma once

namespace engine {

class Actor;

// Ticks all of `actor`'s active status effects for one turn: applies
// Poison damage, decrements every effect's remaining duration, and
// removes anything that's expired. Returns whether `actor` is Stunned
// *this* turn (checked before the decrement, so a stun applied this same
// turn still prevents this turn's action) -- the caller should skip the
// actor's action entirely if true, but still let time pass (this
// function should still be called, and the actor's turn still consumed).
//
// Deliberately the one place status-effect behavior gets applied, kept
// out of StatusEffects itself (pure bookkeeping) -- same split as
// TalentSet / TalentEffects.
bool tickStatusEffects(Actor& actor);

} // namespace engine
