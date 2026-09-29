# Prompt 39: boss counterplay

Implemented locally. One Debug build passed. No tests or playthroughs run;
class viability, full-fight balance and save round trips remain pending.

## Warlord

The existing HP phases stay: healthy melee, magic at <=60%, enrage at <=30%.
Phase-2 blasts commit to a radius-1 cross and allow two player actions to leave.
Enraged melee commits to one tile and allows one action to leave. Other melee
remains immediate. Stun or displacement interrupts a wind-up. After a committed
heavy attack resolves (hit or miss), or a wind-up is interrupted, the Warlord
waits until the player completes one action before acting again. Its existing
boss stun limits remain. Recovery adds time to heal, position or attack, not
another damage modifier.

## Lich

Dark bolts now commit to one tile with a one-player-action warning. Rituals
commit to a visible adjacent spawn tile with two player actions of warning;
purple distinguishes them from damaging strikes. Occupying that exact tile
at resolution fizzles the ritual; stun/displacement also interrupts. No
retargeting to another free tile. Rituals end in one player action of recovery.
New skeletons also wait for one player action before they can act.

The previous cap was three total summons, not three simultaneously alive.
This remains a lifetime budget, now counted as **three committed attempts**.
Interruption or a blocked ritual consumes an attempt; kills never replenish
it. Merely deciding an action without showing/committing the warning does not
spend an attempt. Ritual cooldown starts at commitment. Summons still give
zero XP and zero loot. Ordinary naturally spawned Crypt skeletons retain their
normal rewards and do not count against the Lich's budget.

## Shared rules and persistence

Existing player-action countdowns prevent fast enemies or skipped stunned
player turns from shortening the reaction window. Caster and warning tile must
be visible when committed. Warnings remain fixed even when the player conceals
themselves or leaves sight. Boss control resistance and floor/victory progression
are unchanged. Heavy attacks interrupted by displacement spend that enemy turn.

Save version 13 persists intent kind, recovery window, ritual count, Warlord
one-time enrage and phase-announcement state. Current phase itself derives from
saved HP. Scheduler energy is still not serialized, so exact next actor order
can shift on load; remaining player reaction/recovery windows cannot shrink.
Versions 9-12 migrate. They cannot reveal already-killed summons, so a loaded
legacy Lich conservatively has all three attempts spent. Fresh Lich encounters
start at zero. Legacy Warlord phase/enrage are inferred from HP and Empowered.
Version 13 resumes exact recorded boss state without granting new rituals.

## Pending play checks

- All classes and representative hybrid builds versus both bosses.
- Dodge areas, interruption, ritual occupancy, fast/slow actors and player stun.
- Save/load in each phase, wind-up, recovery and after minion deaths.
- Warlord exit progression and Lich victory after pending level choices.
- Watch for overly easy step-and-shoot loops, especially once Lich minions die.
  Add pressure only if play evidence warrants it; keep ordinary movement useful.

Next roadmap step: Prompt 40 integration/playtesting and a shareable build.
