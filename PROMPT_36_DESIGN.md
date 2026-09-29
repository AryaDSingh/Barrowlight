# Prompt 36 - ailment combinations and defensive decisions

Implemented against the talent-tree system. Runtime/balance verification pending.
This adapts the old roadmap: existing Guard remains flat reduction per direct hit;
there is no additional Guarded status or finite absorption pool.

## Marked

- Shield Bash, Volley and Arcane Bolt apply one charge lasting three affected
  actor turns, on successful hits against survivors. Arcane Bolt base power is
  reduced from 4 to 3 to account for its repeatable setup effect.
- Next landed direct hit receives +25% damage and spends the charge. Refreshing
  Marked restores duration and one charge; charges never stack.
- A dodged attack preserves the charge. Poison/Burn neither benefit from Marked
  nor consume it. A landed hit fully absorbed by Guard still consumes it.
- Existing Marked is consumed before a marking attack applies its new mark.
- Previews include the bonus without consuming anything. Combat logs report the
  consumed bonus and final damage. Inspection shows charge and duration.

## Universal Cleanse

C (or its hotbar slot) activates basic.cleanse: zero mana, one committed action,
eight-turn cooldown. It removes Poison, Burn, Chill and Marked. It does not remove
Stun, stun recovery, beneficial buffs or Concealment. It cannot be used while a
stun is denying the player's turn. If there are no listed debuffs, rejection
costs no turn, resources or cooldown. No ability/tree point is required.

Cleanse runs before its action's enemy response and the player's next status tick.
Cooldown follows the normal cast/tick convention; it shows seven remaining after
the activation turn. Cooldowns now also progress through skipped player stun turns.

## Stun recovery

An active Stun cannot be refreshed or extended. After its final skipped turn,
normal actors gain one actor turn of stun immunity. Bosses (Warlord/Lich) limit
any stun application to one skipped turn and then gain two turns of immunity.
Recovery is not cleansable. Duration is measured in the affected actor's turns,
not player turns; faster actors progress through recovery more quickly.

Recovery starts after expiry processing, so it is not decremented on the same
turn it is granted. A blocked reapplication never resets its timer. Inspection
explains limits and displays active recovery. Other effects, such as Chill,
remain independent; recovery prevents stun rather than every kind of control.

## Damage order and examples (arithmetic, not executed tests)

Player direct damage: base/scaling/flat modifiers and existing execute bonus;
rank and status-consumption multipliers; outgoing Chill reduction; Marked;
critical multiplier; Guard subtraction, floored at zero. Integer divisions
truncate at their respective stages. Enemy direct attacks use the same final
Chill -> Marked -> crit -> Guard order. Damage-over-time bypasses Guard.

- A 10-damage hit against Marked becomes 12; a normal 1.5x critical becomes 18;
  Guard 4 reduces that critical to 14. The mark is consumed only if the hit lands.
- A 10-damage Shatter against Chill gains 50% (15), then Marked gives 18;
  a normal critical gives 27, then Guard 4 leaves 23. Chill/Marked are consumed;
  the stun is independently rejected if recovery is active.
- Burn 2 still deals 2 through Guard and leaves a Marked charge untouched.

Useful sequences: Volley -> Piercing Shot; Shield Bash -> follow-up hit;
Arcane Bolt -> a heavy spell; Burn -> Guard while the enemy burns; cleanse
Poison/Burn before their next tick. Existing Fire/Ice/Lightning consumption
combos continue to work, with explanatory combat log entries.

## Persistence

Writes version 10. Version 9 tree saves are migrated by adding free Cleanse and
an available hotbar binding; points, ranks and other bindings remain unchanged.
Version 8 and older remain unsupported. Status durations, Marked charge, Guard
magnitude, stun recovery and Cleanse cooldown use the existing serialized fields.
Boss stun rules are restored from the monster definition; restored stun durations
are capped to the current maximum. No extra RNG is used by migration or previews.

## Verification boundary

Game compilation is the verification performed here. No tests were added or run,
and no gameplay session was performed. Existing save fixtures were adapted to
include the universal action. Needed next: runtime checks of status ordering,
dodges, full Guard mitigation, cleanse rejection/use, recovery across different
speeds, version-9 migration and version-10 round trips. Values are provisional.
