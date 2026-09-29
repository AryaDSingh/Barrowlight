# Hidden trees: initial design

Four hidden trees, each with the standard four-talent shape (Foundation,
Complement, Passive, Advanced) and three ranks per talent, like every other tree.
Hidden trees do not appear in the tree browser until their unlock condition is
met; discovery follows GAME_DESIGN.md section 7.4 (lore fragments, Codex,
knowledge persisting across characters). Unlocking costs one tree point.

All numbers are initial placeholders for playtesting, not balance.

## Design principles

1. Same four-talent shape as every other tree; only discovery is different.
2. Combine the parent trees so they work together, rather than stacking them.
3. Build on existing mechanics: Burn, Chill, Shock, Marked, Concealed, Opening,
   Guard, push.
4. Adapt to the parent trees where it adds variety (see Imbue Weapon).

## Summary

| Hidden tree | Unlock condition | Lore sources |
|---|---|---|
| Spellblade | Any martial tree specialized + any elemental tree specialized | Warlord, battlemage books |
| Animation | Rare Lich item + Arcane specialized | Lich, crypt enemies |
| Blood Magic | Blood-themed lore item + any magic tree specialized | Warlord or Shamans (**revisit**) |
| Shadow Archer | Bow specialized + Stealth specialized | Archers, rogue-type enemies |

Martial trees: One-Handed, Two-Handed, Shield. Elemental/magic trees: Fire, Ice,
Lightning, Arcane.

---

## Spellblade

Melee and spells feeding each other: spells empower melee, melee brings spells
back sooner.

| Talent | Type | Effect |
|---|---|---|
| Imbue Weapon | Foundation (active) | Enchant your weapon with an element (variants below) |
| Spellstrike | Complement (active) | Melee hit that counts as a magic-tree hit, so it triggers Kindle, Frostbite, Static Charge and Spellweave |
| Battle Rhythm | Passive | Casting a spell empowers your next melee hit; landing a melee hit reduces one spell cooldown by 1 |
| Elemental Release | Advanced (active) | Consume all ailments on adjacent enemies for burst damage; more ailment types consumed means more damage |

### Imbue Weapon variants

You gain one variant for each elemental tree you have unlocked.

| Variant | Requires | Melee hits apply |
|---|---|---|
| Flame Blade | Fire | Burn |
| Frost Blade | Ice | Chill |
| Storm Blade | Lightning | Shock |
| Arcane Blade | Arcane | Marked |

Rules:

- Lasts **5 player turns or 3 melee hits, whichever comes first.**
- **Only one imbue active at a time.** Casting a new one replaces the old one.
- **All variants share one cooldown and one rank.** Ranking Imbue Weapon improves
  every variant.
- **Each variant has its own hotbar entry**, so switching element is one keypress.
- Multi-element investment adds options, not stacked damage. Elemental Release
  is what rewards mixing elements.
- Requires a melee weapon equipped (open question: does a bow count? Default: no).

---

## Animation (summoning necromancy)

Raise and command undead allies.

| Talent | Type | Effect |
|---|---|---|
| Raise Skeleton | Foundation (active) | Summon one allied skeleton adjacent to you, up to your minion cap |
| Bone Swap | Complement (active) | Swap places with one of your minions |
| Grave Pact | Passive | Your minions explode when they die, damaging adjacent enemies |
| Army of the Dead | Advanced (active) | Summon several temporary skeletons at once (they expire after a few turns and do not count toward the cap) |

### Minion rules

- **Minion cap:** 1 base, +1 per 10 effective INT, hard cap 5 (placeholder).
  Effective INT includes equipment.
- If effective INT drops below what the current minion count needs (for example
  after a gear swap), the **oldest minion over the cap dissolves.**
- **Minions dissolve when you take a floor door.** They do not travel between floors.
- Walking into your own minion swaps places with it (so minions never trap you
  in corridors).
- Minion stats scale with rank (placeholder); consider also scaling with INT.

### Engine work required

This is the largest engine change of the four trees. Nothing currently supports
allies.

- An **ally faction**: monsters that fight enemies, not the player.
- **Ally AI**: likely reuse Chaser, targeting the nearest visible enemy, and
  following the player when no enemy is visible.
- **Enemy targeting**: enemies must be able to attack minions, not only the player.
- **Friendly fire rules**: whether player AoE hits minions (suggest: no).
- **Enemy intents** (Bomber cross, Ogre slam): decide whether they can hit minions.
- **XP and loot** from minion kills: suggest they go to the player.
- **Save/load** support for allies.
- The existing Lich summon code (AIActionType::Summon) can be a starting point
  for creating monsters mid-turn.

---

## Blood Magic

Spend life to fuel power, and take it back from enemies. Connects naturally to
Two-Handed's Berserker's Fury (HP cost) and Bloodlust.

| Talent | Type | Effect |
|---|---|---|
| Blood Pact | Foundation (active) | For a few turns, spells cost HP instead of mana |
| Drain Life | Complement (active) | Damage a target and heal for part of the damage dealt |
| Deathless | Passive | Once per floor, survive a lethal hit at 1 HP |
| Wither | Advanced (active) | Curse a target for a few turns; every hit you land on it heals you |

Notes:

- Blood Pact must not let HP costs kill the player outright. Suggest: a spell
  that would reduce you below 1 HP cannot be cast.
- Deathless resets on each new floor. Show whether it is available in the HUD or
  inspection.

**Revisit:** Blood Magic's unlock source (Warlord, Shamans, or elsewhere) is a
placeholder and should be reconsidered.

---

## Shadow Archer

Hide, shoot, kill, stay hidden. Strong by design, so it has explicit limits.

| Talent | Type | Effect | Limit |
|---|---|---|---|
| Shadow Shot | Foundation (active) | Bow attack from Concealment with a chance not to reveal you | Never a guaranteed stay-hidden |
| Hunter's Mark | Complement (active) | Mark a target from range | Marked targets have reduced detection chance, never zero |
| Unseen | Passive | A kill while Concealed refreshes Concealment | At most once per Conceal cast |
| Death from Shadows | Advanced (active) | Heavy shot with large bonus damage from Concealment, then return to Concealment | Long cooldown |

Existing limits that also apply: high-DEX enemies (Archers, Spiders) already
detect well, Conceal has real downtime, and requiring both Bow and Stealth
specialized makes this a late unlock. Requires a bow equipped.

Playtest risk: this tree could make stealth play too safe. Watch for runs where
the player is almost never seen.

---

## Open questions

1. Blood Magic unlock source (**revisit**).
2. Does Spellblade's Imbue work with a bow?
3. Minion cap numbers and whether minion stats scale with INT.
4. Can enemy intents (Bomber/Ogre) damage minions?
5. Does player AoE damage minions?
6. Future hidden trees to consider: Shield + Fire (flame-guarding knight),
   Acrobatics + Lightning (blink-striking storm dancer).


## Implemented first pass

All four trees now have four talents and three ranks, revealed to a character
only when its requirements are met. Revealing a tree records its name and full
condition in the persistent Codex. Buying it still costs one tree point and
its advanced talent still requires specialization. Knowledge grants no access
to a later character by itself.

### Progression and post-Lich play

- Level cap and final floor are now 20. Starting stats and points are unchanged.
- Tree points continue at odd levels 5 through 19 (9 total including level 1).
  Attribute points remain 2 per level, ability points 1 per level plus 3 at start.
- Floor 10's Lich opens the descent to floors 11–20, the Deep Crypts. They use
  the existing undead/support roster and stronger stats/rewards; floor 20 ends
  with a stronger Lich using the established encounter mechanics. This is a
  provisional content band, not a new boss design or a dungeon-selection menu.
- Spellblade and Shadow Archer can first be purchased at level 11 if the player
  invests the earlier points in both required specializations.

### Accepted defaults

- Blood Testament: 20% per eligible Shaman kill until found; guaranteed Warlord
  drop. Ossuary Seal: guaranteed rare relic from the floor-10 Lich. Relics are
  auto-collected run flags shown in the Codex, with no bag slot, selling or
  transfer to a new character. Summons never drop relics.
- Imbue requires a one- or two-handed melee weapon; bows and staves do not count.
- Minions scale with rank and effective INT when summoned. Dropping INT later
  reduces the cap immediately, dissolving the oldest excess permanent minion.
- Enemy direct attacks and committed intents can hit minions. Player damage and
  Grave Pact cannot hurt allies. Ally kills award normal player XP/loot.
- Allies dissolve on floor transitions and Waystone travel, with no explosion.
- Deathless is spent per floor and saved. Backtracking and inn visits do not
  reset it. A first visit to another floor gives its independent use.

### Specific mechanical choices

- Imbue: learn/rank the single Imbue node. Owned elements automatically receive
  distinct hotbar entries; V selects an element in that node; B then a number binds it.
  All variants share rank and cooldown, and only one buff can be active. Each
  landed melee hit consumes one of three charges, even when Guard absorbs it.
  Duration is five subsequent player turns. Rank 2 lowers mana cost from 4 to 3;
  rank also improves the applied element
  (Burn damage, Chill strength, or Shock/Marked duration).
- Spellstrike counts as a spell for costs/passives and as melee for Imbue and
  Battle Rhythm. Kindle applies its Burn to the struck target; movement is not
  required for this explicit exception.
- Battle Rhythm grants 4/5/6 damage to the next melee attack. One landed melee
  action reduces the longest running spell cooldown by one; AoE cannot multiply
  the reduction. Spellstrike can consume the previous bonus and earn the next.
- Release consumes harmful ailments (including Stun), never Guard, Concealed or
  stun-recovery immunity. Each consumed type adds 25% to its damage multiplier.
- Minion stats: HP = 12 + 4*rank + INT; STR = 2 + rank + INT/5. Permanent cap is
  min(5, 1 + INT/10). Army summons up to three for 5/6/7 actions, outside this
  cap; only one temporary army may be alive. Missing adjacent space limits the
  number raised. Summoning at the cap or without space still spends the cast.
- Walking into an ally swaps for a turn. Bone Swap targets a visible ally.
  Other movement abilities cannot pass through allies.
- Grave Pact deals 4/5/6 flat damage to orthogonally adjacent enemies on death;
  dissolution and expiration do not trigger it.
- Blood Pact lasts 3/4/5 subsequent actions and converts final spell mana costs
  into equal HP costs; activation itself costs mana. Cannot spend the last HP.
- Drain heals 40/50/60% of HP actually removed; overkill is excluded. Wither heals
  2 HP per landed direct player hit, capped by HP removed, for 3/4/5 enemy turns.
- Deathless leaves 1 HP; ranks 2/3 also give 1/2 Guard briefly. It does not remove
  damage-over-time effects or grant immunity to the next hit.
- Shadow Shot retains hiding 35/45/55% of the time. Hunter's Mark halves that
  enemy's eligible detection chance, with a 5% floor. Unseen restores 2/3/4
  responses of concealment on a qualifying kill once per actual Conceal cast.
  Death from Shadows gains 50% damage when concealed, then returns to hiding
  for two responses; it does not replenish Unseen.
- New costs are final base costs; existing doubled elemental costs remain.
  Arcane Efficiency also affects the new spell abilities and Spellstrike.

### Saves and remaining work

Format 15 persists allies, lifetimes, shared Imbue state, relics and Deathless
usage. Versions 9–14 remain readable. Legacy floor-10 saves gain an exit; a
completed old Lich encounter grants the Seal. Past Warlord progress grants the
Testament when it can be inferred. A save cannot reconstruct XP discarded while
previously capped at level 10. Older executables cannot read format 15.

The Deep Crypts need playtesting for XP pace, ally survivability, permanent
summon strength, Blood Pact/Drain sustain and Shadow Archer concealment uptime.
No automated tests or interactive playthroughs were run for this implementation.
Broader regions, ascendancies and dungeon/depth selection remain future work.
