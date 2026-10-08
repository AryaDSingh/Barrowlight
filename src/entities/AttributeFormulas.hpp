#pragma once

#include <cstdint>
#include <random>

namespace engine {

// One source of in-run randomness: combat rolls, enemy on-hit rolls and the
// seeds of new floors all draw from it. Unseeded, it starts from the system's
// entropy as before; seedRandomness() makes a whole run repeatable, so two
// builds can be compared on the same luck (the playtest bot's --seed).
std::mt19937& combatRng();
void seedRandomness(std::uint32_t seed);
// A seed for something new (a floor, the loot stream), drawn from combatRng().
unsigned freshSeed();

// Attribute-driven combat formulas. There is no "average" baseline:
// origins start at low hand-picked values (2 or 6), and every point,
// starting or earned, counts at full value. (An earlier baseline-10 model,
// where points below 10 were penalties, made low stats feel like a debt
// and was replaced; DESIGN.md has the reasoning.)

// Which attribute a given talent's damage scales from. Every talent
// scales from exactly one -- no ability scales off more than one stat,
// and no attribute universally boosts all damage the way the old
// physicalDamageBonus/magicDamageBonus pair implicitly did for every
// Physical/Magic talent regardless of what it was.
enum class ScalingStat {
    Strength,
    Dexterity,
    Intelligence,
};

// How much a talent's own cooldown weights its attribute scaling --
// deliberately not a flat rate for every ability. A signature move you
// cast rarely should reward the points invested in it more than a
// filler you spam every turn; without this, a single scaling stat would
// make every ability scale identically regardless of how central it is
// to a build.
enum class AbilityCooldownTier {
    Filler,    // cooldown 0-1
    Core,      // cooldown 2-3
    Power,     // cooldown 4-5
    Signature, // cooldown 6+
};

// Classifies a talent's own cooldownTurns into a tier -- see
// AbilityCooldownTier's own comment for why cooldown (not, say, mana
// cost) is what tier is derived from.
AbilityCooldownTier tierForCooldown(int cooldownTurns);

// The damage bonus a talent gets from `statValue` (the caster's current
// total in whichever attribute the talent scales from -- starting
// spread plus every point allocated since, see PlayerLeveling.hpp),
// scaled by the talent's own cooldown tier. +1 damage per 5 points in
// the scaling stat, multiplied by the tier: Filler x1.0, Core x1.5,
// Power x2.0, Signature x2.5: abilities on long cooldowns get more from
// their attribute, so a big spell stays big as the character grows.
// Picks out whichever of strength/dexterity/intelligence `stat` refers
// to -- a small dispatch used by both damage-application paths
// (TalentEffects::applyTalentDamage for the player,
// Application::executeAIDecision for monsters) so the "which field does
// this ScalingStat mean" logic exists in exactly one place, not two
// near-identical switch statements.
int statValueForScalingStat(ScalingStat stat, int strength, int dexterity, int intelligence);

int abilityDamageBonus(ScalingStat stat, int statValue, int cooldownTurns);

// Dexterity: +0.5% dodge per point of *current total* Dexterity (no
// baseline subtraction -- every point counts, including a class's
// starting spread), capped at 25%. Works identically for players and
// monsters; there's no longer a separate "allocated points only"
// concept for dodge the way there explicitly is for HP/Mana (see
// PlayerLeveling.hpp) -- dodge and crit have no hand-picked starting
// value to conflict with, so computing them fresh from the total stat
// is consistent rather than double-counting anything.
float dodgeChance(int dexterity);
// Total dodge from every source (Dexterity, Evasion, armour, passives, gear) is capped here.
inline constexpr float kTotalDodgeCap = 0.75f;

// Dexterity: +0.5% crit chance per point of current total Dexterity,
// added on top of the 5% every actor (player and monster alike) starts
// with -- see kBaseCritChance in AttributeFormulas.cpp. No cap stated;
// unlike dodge, crit is allowed to climb as high as investment takes
// it.
float critChanceBonus(int dexterity);

// The flat multiplier a critical hit applies -- 1.5x, multiplying
// normally with any other damage multiplier a talent has (e.g. Piercing
// Shot's own bonus), not adding to it.
float critDamageMultiplier();

// Overload accepting a talent-specific bonus on top of the base 1.5x --
// Piercing Shot's own +50% increased crit damage (Talent::
// bonusCritDamageMultiplier) makes its own crits deal 2.0x rather than
// the global 1.5x. The single-argument critDamageMultiplier() above is
// unaffected -- every other talent still gets exactly 1.5x, since it
// passes a 0.f bonus implicitly by never calling this overload at all.
float critDamageMultiplier(float bonusMultiplier);

// Pure comparison, split out from the actual dice roll specifically so
// it's unit-testable with exact inputs: `roll` succeeds if it's
// strictly less than `chance`. rollDodge/rollCrit below are the actual
// dice-rolling functions and are, deliberately, not unit-tested
// themselves: a unit test can't assert an exact outcome from randomness,
// so the integration tests average many rolls instead.
bool didDodge(float chance, float roll);

// Rolls a real dodge check for a defender with this dexterity, using
// dodgeChance() and a shared RNG. Used identically by both damage-
// application paths (TalentEffects::applyTalentDamage for the player's
// own attacks, Application::executeAIDecision for monster attacks) --
// one function, not two copies, so dodge behaves identically regardless
// of which side is attacking.
bool rollDodge(int dexterity);

// Rolls a real crit check for an attacker with this dexterity, using
// critChanceBonus() plus the global base rate. Global -- rolled
// identically for players and monsters, per the "crit chance globally
// 5%... for all creatures and players" requirement.
bool rollCrit(int dexterity);

// Overload adding a talent-specific bonus on top -- Piercing Shot's own
// inherent +20% crit chance (Talent::bonusCritChance). The single-
// argument rollCrit() above is unaffected, same reasoning as
// critDamageMultiplier()'s own overload.
bool rollCrit(int dexterity, float bonusCritChance);

// A general-purpose "does this probability succeed" roll -- draws a
// uniform [0,1) value and returns whether it's less than `probability`.
// Every random roll in combat goes through here (rollDodge and rollCrit
// too), drawing from the one seeded combat stream, so a run is
// reproducible from its seed. The pure comparison it relies on (didDodge)
// is what the tests check exhaustively.
bool rollChance(float probability);

} // namespace engine
