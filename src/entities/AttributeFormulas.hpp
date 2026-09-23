#pragma once

namespace engine {

// Attribute-driven combat formulas (Prompt 14). All baselines are 10 --
// matching Stats' own default member initializers, so a freshly
// constructed Actor with every attribute left untouched contributes
// exactly zero bonus anywhere. Deliberately free functions, not Stats
// methods -- Stats stays plain data (see its own header comment); the
// formulas that interpret it live here, same separation-of-concerns
// reasoning as TalentEffects being separate from Talent.

// Strength: +1 physical damage per 2 points above the 10 baseline,
// using integer division truncated toward zero (C++'s native behavior).
// Every actual value used in this project's stat tables is an even
// offset from 10 specifically to avoid truncation ambiguity at the
// boundary. Can go negative below baseline (a physically weak attacker
// genuinely hits softer), unlike dodgeChance, which floors at zero --
// there's no equivalent floor for a below-baseline attacker; nothing in
// this project currently sets strength below 10 by more than an even
// amount, so this never produces a surprising off-by-one.
int physicalDamageBonus(int strength);

// Intelligence: +1 magic damage per 2 points above the 10 baseline.
// Identical shape to physicalDamageBonus, mirrored onto the other
// offense stat -- the two damage types are deliberately symmetric.
int magicDamageBonus(int intelligence);

// Dexterity: +3% dodge chance per point above the 10 baseline, capped
// at 30% (at dexterity 20). Floors at 0% below baseline rather than
// going negative -- a "chance" can't be negative, so this clamp is a
// real floor, not a stylistic choice like physicalDamageBonus's lack of
// one. Returns a probability in [0, 0.30].
float dodgeChance(int dexterity);

// Intelligence: +1 max mana per point above the 10 baseline. Applied
// once, when an Actor's Stats are first defined (Player construction,
// MonsterFactory) -- max mana is a pool size, not a per-attack value
// like the two damage bonuses above, so nothing calls this mid-combat.
int manaBonusFromIntelligence(int intelligence);

// Pure comparison, split out from the actual dice roll specifically so
// it's unit-testable with exact inputs: `roll` dodges if it's strictly
// less than `chance`. rollDodge(int dexterity) below is the one place
// that actually draws a random number and is, deliberately, not
// unit-tested itself -- same precedent as Chaser's onHitChance roll
// (Prompt 10), which was never isolated for testing either; live play
// is what confirms a randomized rate behaves plausibly, not a unit test
// asserting an exact outcome from randomness.
bool didDodge(float chance, float roll);

// Rolls a real dodge check for a defender with this dexterity, using
// dodgeChance() and a shared RNG. Used identically by both damage-
// application paths (TalentEffects::applyTalentDamage for the player's
// own attacks, Application::executeAIDecision for monster attacks) --
// one function, not two copies, so dodge behaves identically regardless
// of which side is attacking.
bool rollDodge(int dexterity);

// A general-purpose "does this probability succeed" roll -- draws a
// uniform [0,1) value and returns whether it's less than `probability`.
// Introduced at Prompt 19 once a second independent "local static RNG +
// uniform_real_distribution" pattern (Chaser's onHitChance roll, Prompt
// 10) was about to become a third (Sorcerer's Mind Shatter, in
// TalentEffects) -- consolidated here rather than left triplicated, the
// same "extract once a third copy would appear" discipline AIUtils.hpp
// followed at Prompt 11. rollDodge() above is now implemented in terms
// of this. Not unit-tested in isolation, same precedent as rollDodge
// and Chaser's original onHitChance roll -- it's the one function here
// that actually draws a random number; didDodge (the pure comparison
// this and rollDodge both ultimately rely on) is what gets the
// exhaustive testing.
bool rollChance(float probability);

} // namespace engine
