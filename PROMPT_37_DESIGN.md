# Prompt 37: enemy intent and mixed encounters

Implemented locally. The single Debug game build passed; no automated tests or playthroughs run.
Balance and runtime/save round-trip verification remain pending.

## Committed attacks

- Bomber Blast marks a Manhattan-radius-1 cross (center plus four cardinal
  walkable neighbors). It allows **two completed player actions**, then resolves
  on the Bomber's next turn. Two ordinary movement steps can leave the cross.
- Ogre retains its existing 35% stun-attack selection chance. A selected Stun
  Slam now marks the player's current tile and allows **one completed player
  action** before resolving on the Ogre's next turn. Other melee hits stay instant.
- Orange/red overlays show affected tiles and the remaining action count. They
  are drawn above actors so the player's tile cannot hide the warning.
- Intent starts only while the caster is visible. The target never follows the
  player. A committed attack resolves even if the player subsequently conceals
  themselves or the caster loses sight; stepping outside the area avoids it.
- Stun or displacement of the caster interrupts on its next turn and spends
  that turn. Death cancels it. Chill slows/reduces damage using existing rules;
  it does not interrupt. Bomber cooldown begins at commitment, including misses
  and interruptions, and keeps ticking during the wind-up.
- Attacks damage only the player, preserving the existing no-friendly-fire rule.
  Dodge, crit, Guard, Marked and stun recovery still apply on impact.

## Scheduler contract

Movement, successful ability use, waiting and inventory actions decrement all
existing intent counters once. Failed actions, inspection, targeting, menus,
saving/loading and automatic stunned-player skips do not. A fast enemy waits
without retargeting until the counter reaches zero. A slow enemy may release
later than the minimum window. Countdown 0 means ready for its next enemy turn.

## Floor populations

`planEncounters` uses the generated room order and floor number, independently
of player level and combat/loot RNG. A fixed map and floor produce a fixed plan.
The regular-enemy budget is `14 + 2 * (floor - 1)` (floors 1-10).

| Spawn | Cost |
| --- | --- |
| Goblin / Spider | 2 |
| Archer / Shaman | 3 |
| Ogre / Bomber | 4 |
| Elite Goblin upgrade | +2 |

Groups contain exactly two monsters, placed together or omitted:

- Goblin + Spider: ordinary group, including the first room.
- Goblin + Archer: up to two groups per floor.
- Goblin + Bomber: at most one, eligible from floor 2.
- Ogre + Shaman: at most one, eligible from floor 3.

Budget may prevent an eligible group from appearing. Goblins can become elite
from floor 3 (at most one; two from floor 7), except in the first room or a
Bomber/Ogre-Shaman group. No new Nightmare spawns. Existing loaded populations
retain their tiers. Elite inspection shows HP/damage/XP multipliers; speed and
control rules are unchanged. This supersedes Prompt 22's player-level tier rule.

Members occupy distinct walkable cells within one tile of the room center;
melee goes toward the previous room, support/ranged toward the back. Every
regular spawn is more than eight tiles from the player's starting center.
A boss whose center is too close uses the far side of its own room, with its
progression door moved beneath it. Bosses remain outside the regular budget.

Caps govern initial populations, not how many enemies the player can pull
between rooms. The new curve is intentionally conservative and needs play data,
especially late-floor strength and progression rate. No extra XP/reward changes.

## Persistence

Version 11 adds a pointer-free intent per monster: origin, target, radius,
remaining player actions and attack power. It restores without rerolling the
attack. Versions 9 and 10 migrate with no pending intents; the version-9 free
Cleanse migration remains. Version 8 and earlier remain unsupported.

Scheduler energy and unrelated AI counters still reset on load. The exact next
actor order can differ, but loading cannot shorten an intent's remaining player
reaction window. Loaded populations stay intact; future floors use the new plan.

## Pending checks (not executed)

- Step out, wait, dodge and interrupt each wind-up; inspect warning visibility.
- Fast/slow enemies, stunned player and multiple simultaneous intents.
- Save/load with remaining windows of 2, 1 and 0; version-9/10 migration.
- Sample dungeon seeds for spawn budgets, opening safety and boss placement.
- Full-run XP and difficulty curve with all starting classes.

Next feature: Prompt 38, floor themes and optional vaults, using item rewards
and talent-tree builds (runes have been removed).
