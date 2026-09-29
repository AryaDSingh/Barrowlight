# Prompt 35 - approved talent-tree system

Implemented September 26, 2026. Supersedes the previous borrowed-talent/native
mastery proposal. Numbers are initial tuning, not playtested balance.

## Progression

| Reward | Tree points | Ability points | Attribute points |
|---|---:|---:|---:|
| Creation | 1 | 3 | 0 |
| Each level gained, 2-10 | 0 | 1 | 2 |
| Extra at 5, 7, 9 | 1 | 0 | 0 |
| Lifetime total at 10 | 4 | 12 | 18 |

Existing +1 maximum HP and HP/mana refill per level remain. STR allocation adds
one maximum HP; INT allocation adds one maximum mana. Equipment attributes do
not retroactively grant pools. Starting stats are unchanged:

| Class | STR/DEX/INT | HP/Mana | Eligible first tree |
|---|---|---|---|
| Warrior | 6/2/2 | 30/10 | One-Handed, Two-Handed, Shield |
| Thief | 2/6/2 | 25/15 | Stealth, Bow, Acrobatics |
| Mage | 2/2/6 | 20/20 | Fire, Ice, Lightning, Arcane |

Unlock costs one tree point. At level 5, subsequent purchases can access every
tree. The first tree must always come from the class pool. Specialization costs
one tree point, requires level 5 and four ability points invested in that tree,
and opens its advanced talent. One specialization per tree; no secondary-class
lock or borrowed-ability cap.

Each ability purchase costs one ability point: learn rank 1, then ranks 2 and 3.
No respec. Purchases are turn-free and preserve running cooldowns. Points can be
banked. First play requires one unlocked tree and one learned ability.

| Talent position | Minimum level | Prior points in tree | Gate |
|---|---:|---:|---|
| Foundation | 1 | 0 | Unlocked tree |
| Complement | 1 | 1 | Unlocked tree |
| Synergy passive | 4 | 3 | Unlocked tree |
| Advanced active | 5 | 4 | Specialized tree |

All ranks are available after learning. Most attacks use 100/120/145% damage.
Fury/Piercing Shot trade some damage growth for -1 cooldown at rank 3. Movement
ranks reduce mana by one, then add one tile. Buff ranks reduce mana, then cooldown.
Radius/control duration do not automatically increase. Numeric passives generally
use 4/5/6, or 10/12/15 percentage points. Arcane Efficiency uses 10/20/30% mana
reduction because integer costs quantize its benefit; round down, minimum one.

## Content

| Tree | Foundation | Complement | Passive | Advanced |
|---|---|---|---|---|
| One-Handed | Quick Strike | Parry | Riposte | Execution |
| Two-Handed | Cleave | Berserker's Fury | Bloodlust | Whirlwind |
| Shield | Shield Bash | Guard | Shield Training | Shield Shockwave |
| Bow | Quick Shot | Volley | Marksmanship | Piercing Shot |
| Stealth | Conceal | Ambush Strike | Ambush | Vanish Strike |
| Acrobatics | Tumble | Vault Kick | Footwork | Evasive Leap |
| Fire | Ember Bolt | Fireball | Kindle | Meteor |
| Ice | Ice Shard | Frost Nova | Frostbite | Shatter |
| Lightning | Lightning Bolt | Chain Lightning | Static Charge | Discharge |
| Arcane | Arcane Bolt | Blink | Arcane Efficiency | Mind Shatter |

Martial scales with STR; Bow/Stealth/Acrobatics with DEX; magic with INT. Basic
Attack scales with STR for everyone, costs no mana and works by bumping an enemy.
Space waits and grants Opening for the next action; it does not grant Guard.

### Interaction rules

- Burn ticks each affected actor turn; reapplication refreshes without stacking.
- Chill lasts three enemy turns: -20% outgoing direct damage, and movement is
  skipped on alternate turns (first/third). Attacks are not skipped.
- Shock is a non-stacking marker applied by Static Charge on Lightning hits.
  Discharge consumes it for +50% damage and cannot reapply it. Chain targets one
  visible secondary at half damage; it cannot recursively chain.
- Shatter consumes Chill for +50% damage and one-turn Stun. Meteor consumes Burn
  for +50% damage. Dodges do not consume these statuses.
- Guard subtracts flat direct-hit damage, including crits, with a zero floor.
  Shield Training requires an equipped shield. Damage-over-time bypasses Guard.
- Evasion adds dodge, capped at 60% total. Overlap keeps the stronger magnitude
  and longer remaining duration rather than summing.
- Conceal uses a rank/DEX/distance detection roll. Each non-stunned enemy within
  four Euclidean tiles and with line of sight rolls once on its actual turn.
  A successful detection removes Concealment globally and that enemy acts normally.
  Direct attacks, damage taken and Kindle still reveal you. No pursuit/hearing.
  Conceal ranks improve hiding strength; mana (3), cooldown (7) and duration
  (three responses) stay fixed. Vanish Strike uses its own rank for concealment.
- Ambush increases direct damage from Concealment across trees. Frostbite adds
  direct damage against Chilled enemies across trees.
- Successful movement abilities grant Opening; Marksmanship uses it for Bow
  critical chance. Footwork grants one enemy response of Evasion.
- Kindle ignites highlighted adjacent enemies after actual movement/retreat,
  once per action, using cast-time visibility to match previews. Walking and
  blocked/no-op movement do not trigger movement passives.
- Whirlwind/Shield Bash push surviving hits one tile if terrain/occupancy permit.
  There is no collision damage.

## Equipment and runes

Weapon, Armour, Charm, Off-hand. Martial weapon trees require matching equipment;
Shield requires a shield. Shields conflict with two-handed weapons and bows.
Conflicting equips fail freely; explicitly remove the other item first. Gear
changes retain their one-turn cost.

First weapon trees supply bonus-free training gear, preserving starting effective
stats. Greatswords/shields enter loot; training items do not. Later weapon trees
require finding matching equipment. Cast preflight explains missing equipment.

Runes, attachments, UI and rewards are removed. Chest equipment and two rare boss
items remain; there are no replacement ability-point drops. Chain targeting and
other generic combat machinery are reused by tree abilities.

## Architecture and saves

- TalentCatalog: stable tree/ability IDs, eligibility and three rank profiles.
- TalentProgression: purchase validation and spending.
- Player: tree ownership/specialization and point balances.
- TalentSet: ranks, cooldowns and two hotbar pages by stable IDs. Passives do not
  take hotbar slots. Learning auto-fills; browser bindings can be changed.
- effectiveTalent: shared rank/passive resolution for preview/cast. Original
  cooldown scaling tier is fixed. Bloodlust eligibility is captured before HP
  costs so preview and commitment agree.
- ApplicationTalents: browser/input and movement-triggered effects.
- PlayerLeveling grants every crossed level's points, including 5/7/9. Attributes
  resolve first, then tree review. Closing banks points and completes any pending
  victory. No free native unlocks remain.
- Save version 9 persists trees, ranks, balances, cooldowns, hotbar and pending
  review/victory. Old saves require a new run. Validation precedes live restore.
  Monster talents remain rank 1 and have no player progression gates.
- Legacy fixed-kit helpers remain for old isolated combat fixtures only. Live
  class creation/leveling do not grant those kits.

## Verification boundary

Debug game compilation succeeds. No tests were added or run, and no runtime or
visual playthrough was performed. Existing fixtures were adapted to removed
APIs; obsolete rune assertions were removed. Earlier pass counts do not validate
this implementation.

Next: focused runtime verification of all starts, banked points, level jumps,
specialization, late hybrids, saves during progression/victory, ranks, equipment
conflicts, hotbar persistence and status interactions. Balance risks include
flat passive bonuses, integer mana costs, stealth downtime, control uptime and
finding equipment for late weapon trees. Elemental resistances remain future
work. Scheduler energy/AI-internal counters remain outside save scope. R is
still a development regeneration shortcut. Reconcile Prompt 36+ before using
its old rune/status proposals.


## Stealth balance follow-up (2026-09-26)

Detection probability, before clamping to 2%-85%:

`38% - 12% * (rank - 1) - 1.5% * min(player DEX, 16)
 + 0.8% * min(enemy DEX, 60) - 22% * max(distance - 1, 0)`

DEX is effective DEX including equipment; negative inputs are treated as zero.
Distance is Euclidean. Beyond four tiles or blocked sight: no check and no RNG
consumption. Stunned enemies cannot check. Viewing UI never rolls. Detection uses
the existing combat RNG, separate from loot; combat RNG is not serialized.

At rank 3 and player DEX 6, rounded detection chance per eligible enemy turn:

| Enemy | Enemy DEX | 1 tile | 2 tiles | 3 tiles |
|---|---:|---:|---:|---:|
| Goblin/Ogre and other DEX-0 enemies | 0 | 5% | 2% | 2% |
| Lich | 20 | 21% | 2% | 2% |
| Spider | 36 | 34% | 12% | 2% |
| Archer | 48 | 43% | 21% | 2% |

Against adjacent DEX-0 enemies, player DEX 6 gives 29%/17%/5% detection at
ranks 1/2/3. Extra DEX helps high-perception matchups but does not eliminate risk.
Crowds compound risk: these are per-enemy-turn chances, not whole-cast guarantees.
Spiders also have high DEX; tiers currently leave enemy DEX unchanged.

Visible enemy inspection shows the current chance while concealed. The tree
browser shows each rank's hiding strength. Concealed status magnitude stores the
source rank, so save/load retains it without a schema change. Old version-9
concealment with magnitude zero behaves as rank 1 until it expires or is recast.
This tuning is analytically chosen; runtime/playtest balance is not established.
