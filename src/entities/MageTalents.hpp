#pragma once

#include <optional>
#include <vector>

#include "entities/Talent.hpp"

namespace engine {

// The Mage's starting talent kit: 4 talents, the same "deliberately
// smaller than the Spellblade's 9" discipline established for the
// Fighter and Thief (Prompts 15-16). Pure Intelligence -- the third
// base class, completing the one-pure-class-per-attribute trio at
// Prompt 19. Every talent is Magic (Intelligence-scaled); unlike the
// Thief (whose damage stays Strength-scaled since Dexterity carries no
// damage bonus in this project's formula system), the Mage's
// dominant stat already covers both damage *and* mana, so there's no
// equivalent need to keep a secondary attribute moderate the way the
// Thief keeps Strength at 14 -- Strength and Dexterity are both true
// dump stats here (6 and 8).
//
//   0. Arcane Bolt   -- the filler: ranged, cheap, fast. Same role
//                       Quick Shot/Slam/Ember Bolt play for the other
//                       classes.
//   1. Arcane Storm  -- ranged AoE, the same AreaAroundTarget shape
//                       Fireball/Volley already use, re-themed as raw
//                       arcane force.
//   2. Arcane Focus  -- SelfBuff: applies Empowered, the same status
//                       effect Rallying Cry/Steady Aim and the boss's
//                       own enrage all already use -- reused, not
//                       reinvented.
//   3. Mind Shatter  -- the signature move: modest damage with a real
//                       chance to Stun the target -- the first player
//                       talent to apply a status effect to its target
//                       rather than the caster (see
//                       Talent::onHitEffect/onHitChance). Stun has
//                       existed since Prompt 10 (the Ogre inflicts it),
//                       but only ever landed on the player before this;
//                       the Mage is the first class that can turn
//                       it back around.
//
// As of Prompt 23, two more are unlocked through play rather than known
// from the start -- see mageTalentUnlockedAtLevel() below.
//
//   4. Meteor    -- unlocked at level 4. The single hardest-hitting
//                   talent in the kit -- this class's answer to the
//                   Fighter's Berserker's Fury, at a mana/cooldown cost
//                   to match rather than an hp one.
//   5. Overload  -- unlocked at level 7. A genuine upgrade on Arcane
//                   Focus: a longer, stronger Empowered.
//
// See ARCHITECTURE_DECISIONS.md, "Three base classes (Prompt 19)," for
// the resource-economy reasoning (the biggest mana pool of any class,
// reflecting a genuinely spell-hungry identity) and why Mind Shatter's
// on-hit effect needed a new Talent field rather than a new
// TalentEffectKind.
std::vector<Talent> mageTalents();

// Returns the talent unlocked at exactly this level, if any -- see
// FighterTalentUnlockedAtLevel's comment (the same reasoning applies
// here unchanged). nullopt for every level that isn't 4 or 7.
std::optional<Talent> mageTalentUnlockedAtLevel(int level);

} // namespace engine
