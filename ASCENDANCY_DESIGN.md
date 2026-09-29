# Ascendancy: initial design

A late, permanent, identity-defining choice layered on top of talent trees.
Hidden trees widen what a character can do; ascendancy sharpens who they are.
All numbers are placeholders for playtesting, not balance.

## Rules

- **One ascendancy per character.** It is permanent; there is no switching.
- **Class-gated.** Your starting class decides which ascendancies you can ever
  choose: only those that include your class's primary attribute (see table).
  Each class has exactly four options.
- **Build-qualified.** Within your class's options, your specialized trees decide
  which ascendancies you currently qualify for. Class decides what you can see;
  build decides what you can pick.
- **Hybrids are shared.** A Warrior Templar and a Mage Templar are the same
  ascendancy, reached from opposite directions. Class-specific versions may come
  later.
- **Two ascendancy points total.**
  - **First trial:** choose your ascendancy and gain your first point.
  - **Second trial:** a harder trial deeper in the world grants your second point,
    spent in the same ascendancy.
- **Each ascendancy has at least five nodes** (currently six each). Each point
  buys one node, so a character ends with two of the six. Nodes have no ranks.
- **Nodes can be actives or passives.** A character may take two actives, two
  passives, or one of each.
- **Actives** take hotbar slots like any other talent. **Passives** do not.
- Only the ten core trees count toward eligibility; armour trees and hidden trees
  do not.

## Earning points: trials  [Open]

Ascendancy points come from trials: dangerous, optional challenges inside
dungeons, most likely built around a trial boss. The exact format is still to be
designed. Requirements:

- A real risk/reward moment (permadeath applies).
- The first trial should be reachable mid-progression; the second clearly later
  and harder.
- The ascendancy choice screen appears when the first trial is completed, showing
  only ascendancies the character currently qualifies for.
- If the character qualifies for none, the reward should be held until they do
  (open question).

## Class access and eligibility

| Ascendancy | Attributes | Mage | Warrior | Thief | Qualify by specializing |
|---|---|---|---|---|---|
| Elementalist | INT | ✓ | | | Two elemental trees |
| Juggernaut | STR | | ✓ | | Two martial trees |
| Trickster | DEX | | | ✓ | Two DEX trees |
| Templar | STR/INT | ✓ | ✓ | | One martial + one elemental tree |
| Shadowcaster | DEX/INT | ✓ | | ✓ | One DEX + one elemental tree |
| Duelist | STR/DEX | | ✓ | ✓ | One martial + one DEX tree |
| Paragon | STR/DEX/INT | ✓ | ✓ | ✓ | One tree from each family |

Families: martial (One-Handed, Two-Handed, Shield), DEX (Bow, Stealth,
Acrobatics), elemental (Fire, Ice, Lightning, Arcane).

A character may qualify for more than one of its class's ascendancies at the
first trial. All qualifying options are shown together with a clear warning that
the choice is permanent.

---

## Balance pass (first review)

Each node below has a balance rating:

- **Good:** fine as designed.
- **Watch:** fine, but a playtest risk.
- **Rework:** a design problem worth fixing before implementation. A proposed
  change is given, but **node effects have not been changed yet**; reworks are
  pending approval.

Because each character gets only two of six nodes, the review also notes which
pairs are likely to dominate. Issues affecting several ascendancies are listed
in "Cross-cutting issues" after the ascendancies.

---

## Elementalist

Master of ailments and elemental combinations.

| Node | Type | Effect | Balance |
|---|---|---|---|
| Elemental Fury | Active | Greatly increased elemental damage for a few turns | **Good.** Needs a long cooldown; stacks with Meteor/Shatter's +50% consumes for a big burst |
| Elemental Ward | Active | Gain an absorb shield; consuming an ailment restores part of it | **Watch.** Absorbing all damage is strong for the lowest-HP class. Cap shield size; recharge only through consumes |
| Lingering Elements | Passive | Ailments you apply last longer | **Watch.** Longer Chill pushes toward permanent slows (Prompt 35 lists control uptime as a risk) |
| Conduit | Passive | Consuming an ailment (Meteor, Shatter, Discharge) refunds mana | **Good.** Solves mana pressure and rewards combo play |
| Elemental Overload | Passive | Applying a different ailment to an already-ailing enemy causes a small explosion | **Good.** Fits a two-element build. The explosion must not trigger itself in a chain |
| Attunement | Passive | Hitting with a different element than your last spell deals bonus damage | **Rework.** Overlaps Overload (both reward mixing) and does nothing for single-element builds. **Proposed:** repeated hits with the *same* element stack bonus damage, giving focused builds a node |

Likely dominant pairs: Fury + Conduit (offense), Ward + Conduit (safety).

**Elemental Ward note:** the game has no damage types or resistances yet, so the
shield currently absorbs **all** damage and is recharged by elemental play. If
damage types are added later, it could instead absorb only elemental or
INT-scaled damage. **Open question.**

---

## Juggernaut

Unstoppable frontline fighter who shrugs off control.

| Node | Type | Effect | Balance |
|---|---|---|---|
| Unstoppable | Active | Immune to stun, push and all ailments for a few turns | **Watch.** Strong against Ogres and Spiders, but stun recovery and boss stun limits already exist, so it is worth less than it sounds |
| Earthshaker | Active | Heavy area slam that pushes and stuns (respects boss stun limits) | **Watch.** Stun interrupts enemy telegraphs, so this cancels Bomber/Ogre attacks in an area. Give it a long cooldown |
| Iron Skin | Passive | Guard is stronger | **Rework.** Guard only comes from the Shield tree; a Two-Handed + One-Handed Juggernaut gets nothing. See Guard dependency below |
| Rampage | Passive | Killing an enemy reduces your cooldowns by 1 | **Good.** Strong against groups, weak against bosses; a fair trade |
| Last Stand | Passive | Below a third of max HP, deal more damage and take less | **Watch.** Can be triggered deliberately with self-damage (Berserker's Fury, Blood Pact) |
| Stalwart | Passive | Stuns on you are shorter and ailments on you tick for less | **Rework.** Overlaps Unstoppable and existing stun recovery. **Proposed:** replace with an offensive node, such as "hits on stunned enemies deal bonus damage" |

Reminder: Guard reduces every direct hit by a flat amount (including crits,
never below zero) and does not reduce damage over time.

Unstoppable is designed with enemy ailments in mind. Spiders already poison and
Ogres already stun; more enemies are expected to inflict ailments and curses later.

---

## Trickster

Evasive opportunist who turns openings and misdirection into damage.

| Node | Type | Effect | Balance |
|---|---|---|---|
| Shadowstep | Active | Teleport behind a visible enemy and gain Opening | **Good.** Strong mobility and a real answer to telegraphs. Requires line of sight and a free tile behind the target |
| Decoy | Active | Create a clone that enemies target instead of you for a few turns | **Watch.** Very strong while it lasts. Give it low HP so it breaks when hit |
| Quick Hands | Passive | Opening lasts an extra turn | **Watch.** Only useful with something that uses Opening (Marksmanship, Light armour). A Stealth + Acrobatics Trickster gets almost nothing |
| Opportunist | Passive | Crits from Concealment or Opening deal extra damage | **Good.** Stacks with Ambush, which is the point of a Thief build |
| Slippery | Passive | Dodging an attack grants Opening | **Watch.** Loops with Agile Fit (dodge → Opening → more dodge). The 60% dodge cap limits it |
| Misdirection | Passive | When an enemy's telegraphed attack misses you, reduce your cooldowns | **Rework.** Only Bombers and Ogres telegraph, so it triggers too rarely to be worth a point. **Proposed:** broaden the trigger, or replace it. Revisit after the telegraph rework (PRIORITIES.md Tier 1), which may add more telegraphs |

**Decoy dependency:** enemies currently only ever target the player. The decoy
needs enemies that can target something else, which is the same engine work as
the Animation hidden tree's minions (see HIDDEN_TREES_DESIGN.md). Building
Animation first makes Decoy much cheaper.

---

## Templar (STR/INT)

A holy knight who uses magic to protect and punish. Deliberately defensive, to
stay distinct from the Spellblade hidden tree's melee/spell offense.

| Node | Type | Effect | Balance |
|---|---|---|---|
| Consecrate | Active | Bless the ground around you; enemies standing in it take damage each turn | **Watch.** Good for a tank, since melee enemies come to you. Needs new engine work: lasting ground effects do not exist yet |
| Mana Shield | Active | For a few turns, part of the damage you take is paid from mana instead of HP | **Watch.** Great for a Mage (20 mana), weak for a Warrior (10 mana): the same node is worth very different amounts by class |
| Righteous Guard | Passive | Guard is stronger while you have more than half mana | **Rework.** Same Guard dependency as Iron Skin (Templar needs a martial tree, not necessarily Shield). Also works against Mana Shield, which spends the mana it needs |
| Retribution | Passive | When a direct hit is fully absorbed by Guard, the attacker takes damage | **Rework.** Only triggers against weak hits; useless against bosses. **Proposed:** reflect part of any damage Guard reduces |
| Sanctuary | Passive | Waiting restores a little HP and mana | **Rework.** If it works outside combat, you heal fully between every fight, removing attrition. **Proposed:** only works while enemies are visible, or replace it |
| Zeal | Passive | Casting a spell grants Guard for your next enemy turn | **Watch.** Good idea; needs a defined Guard amount for characters without the Shield tree |

Templar leans heavily on Guard; it needs the Guard dependency resolved more than
any other ascendancy.

---

## Shadowcaster (DEX/INT)

A caster who strikes from hiding and slips away, mixing stealth, blinks and
ailments.

| Node | Type | Effect | Balance |
|---|---|---|---|
| Veil | Active | Blink a short distance and become Concealed | **Watch.** An excellent escape, which is exactly why it needs a long cooldown |
| Hex | Active | Curse a target; ailments on it last longer and deal more damage | **Good.** Overlaps Lingering Elements, but a Mage must choose between Elementalist and Shadowcaster anyway |
| Hidden Casting | Passive | Spells cast from Concealment deal bonus damage | **Good.** Stacks with Ambush, like Opportunist |
| Lingering Shadow | Passive | Casting from Concealment has a chance not to reveal you | **Watch.** Combined with Shadow Archer's Shadow Shot, stealth could become too safe |
| Blink Strike | Passive | Movement abilities apply your last-used element's ailment to adjacent enemies | **Watch.** "Last-used element" is hard to track and explain. **Proposed:** simplify to one fixed effect, such as Chill |
| Spell Thief | Passive | Killing an ailing enemy restores mana | **Good.** Solid mana sustain, similar to Conduit |

Likely dominant pair: Veil + Hidden Casting.

---

## Duelist (STR/DEX)

A one-on-one specialist who wins through timing and counterattacks.

| Node | Type | Effect | Balance |
|---|---|---|---|
| Challenge | Active | Mark one enemy as your duel target; deal more damage to it and take less from it | **Good.** A clear answer to bosses. Stacks with Marked |
| Flurry | Active | Several quick hits on one adjacent enemy | **Watch.** Each hit rolls crit separately and builds Momentum quickly. Also burns all three Imbue Weapon hits in one action. Consider one crit roll per ability |
| Counter | Passive | Dodging a melee attack lets you strike back automatically | **Watch.** Needs new engine work (the player acting during an enemy's turn). At 60% dodge, that is a lot of free attacks |
| Momentum | Passive | Consecutive hits on the same target deal increasing damage; resets on switching targets | **Good.** Pairs well with Flurry; watch that pair |
| Footwork Master | Passive | Opening also grants extra dodge | **Rework.** Duplicates Agile Fit (Light armour) exactly, and its name clashes with Acrobatics' Footwork passive. **Proposed:** replace |
| Finisher | Passive | Hits against enemies below a quarter of their HP deal extra damage | **Good.** Overlaps Execution, but a natural duelist fit |

Duelist is strong against single targets and weak against groups. That is a good
identity, but encounters currently come in pairs, so it may feel weak on
ordinary floors.

---

## Paragon (STR/DEX/INT)

A master of all three disciplines, rewarded for rotating between them rather
than being a slightly better version of every other ascendancy.

| Node | Type | Effect | Balance |
|---|---|---|---|
| Convergence | Active | For a few turns, all your abilities scale from your highest attribute | **Good.** Makes off-family abilities genuinely useful |
| Adaptation | Active | Reset the cooldown of one ability from each family | **Watch.** Resets three cooldowns; could be the strongest active in the game, especially with Harmony |
| Versatility | Passive | Using an ability from a different family than your last one grants bonus damage | **Good.** Supports the rotation identity |
| Balanced Mind | Passive | Small bonus to dodge, crit and Guard | **Rework.** Flat bonuses to everything tend to be boring or the "safe" pick every time. **Proposed:** replace with a rotation-themed node |
| Jack of All Trades | Passive | Your lowest attribute counts as higher for damage scaling | **Rework.** Overlaps Convergence (both boost weaker attributes' scaling). **Proposed:** keep one of the two |
| Harmony | Passive | Using one ability from each family within a few turns grants a large burst of bonus damage | **Watch.** The payoff node. Fine alone; dangerous with Adaptation |

Paragon requires three specialized trees, so it is naturally a late-game option.

---

## Cross-cutting issues

1. **Guard dependency (biggest issue).** Iron Skin, Righteous Guard, Retribution
   and Zeal all assume the character has Guard, which only the Shield tree
   provides. Options:
   - These nodes grant their own base Guard, so they work for any build
     (recommended: simplest).
   - They require the Shield tree to be specialized.
2. **Opening dependency.** Quick Hands and Slippery only pay off if the build
   already uses Opening (Marksmanship, Light armour).
3. **Overlaps to resolve:** Attunement and Elemental Overload; Footwork Master
   and Agile Fit; Jack of All Trades and Convergence; Stalwart and Unstoppable.
4. **New engine work:** Consecrate (lasting ground effects), Counter (player
   reactions during enemy turns), Decoy (enemies targeting something other than
   the player).
5. **Actives crowd out passives.** With only two points, a strong active is
   tempting every time. Each ascendancy should have at least one passive that
   competes with its best active.
6. **Class asymmetry in shared hybrids.** Mana Shield is worth far more to a Mage
   than a Warrior. Acceptable, but worth noting when tuning shared ascendancies.

## Reworks pending approval

| Ascendancy | Node | Proposed change |
|---|---|---|
| Elementalist | Attunement | Same-element hits stack bonus damage |
| Juggernaut | Iron Skin | Resolve Guard dependency |
| Juggernaut | Stalwart | Replace with offensive node (e.g. bonus damage vs. stunned enemies) |
| Trickster | Misdirection | Broaden trigger or replace; revisit after telegraph rework |
| Templar | Righteous Guard | Resolve Guard dependency and Mana Shield conflict |
| Templar | Retribution | Reflect part of any damage Guard reduces |
| Templar | Sanctuary | Only while enemies are visible, or replace |
| Shadowcaster | Blink Strike | Simplify to one fixed effect (e.g. Chill) |
| Duelist | Footwork Master | Replace (duplicates Agile Fit) |
| Paragon | Balanced Mind | Replace with a rotation-themed node |
| Paragon | Jack of All Trades | Keep only one of it and Convergence |

---

## Open questions

1. Trial format: what the trials are, where they appear, and what the trial
   bosses are.
2. What happens if a character completes the first trial but qualifies for no
   ascendancy?
3. Elemental Ward: absorb all damage, or only elemental/INT-scaled damage once
   damage types exist?
4. Class-specific versions of shared hybrid ascendancies (later, optional).
5. Guard dependency: should Guard-based nodes grant their own Guard, or require
   the Shield tree?
6. Which proposed reworks to accept.
