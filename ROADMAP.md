# Prompt Roadmap — Roguelike Engine

The planned sequence of build steps for this project, worked out during
initial planning before this Project existed. Each prompt assumes the
previous one's state is already built and verified — build/test after each
one before moving to the next, and don't stack unverified systems.

Status: ✅ done · ⏳ up next · ⬜ not started

## ✅ Prompt 0 — Project instructions
Locked into the Project's custom instructions: scope, OOP + composition
constraint, C++17, data-driven definitions, "explain trade-offs" requirement.
See `ARCHITECTURE_DECISIONS.md` → "Project scope."

## ✅ Prompt 1 — Tech stack decision
Compare SDL2/SFML/raylib for a turn-based tile roguelike; recommend one.
**Result:** SFML 3.1.0. See `ARCHITECTURE_DECISIONS.md` → "Rendering/windowing library."

## ✅ Prompt 2 — Project skeleton
Folder layout, CMake build system, minimal window that opens/closes cleanly.
No game logic. **Result:** verified end-to-end on Linux (sandbox) and on the
person's actual Windows machine. See `ARCHITECTURE_DECISIONS.md` → "Build
system" / "Engine boundary."

## ✅ Prompt 3 — Core entity/actor classes
Implement the base Entity/Actor/Item/Feature hierarchy, with Actor composed
of Stats, AIBehavior, Inventory, TalentSet, StatusEffects member objects.
Minimal — just enough structure to compile, no gameplay yet. **Result:**
all classes header-only (nothing to implement yet), verified via a
standalone `entity_smoke_test` target with zero SFML dependency. See
`ARCHITECTURE_DECISIONS.md` → "Entity/Actor implementation."

## ✅ Prompt 4 — Turn scheduler
Energy/speed-based turn scheduler (ToME4-style: actors accumulate energy,
act when they cross a threshold). Standalone test printing turn order for
actors with different speeds, verified before wiring to real gameplay.
**Result:** `TurnScheduler` (energy tracked internally, not on `Actor`)
verified via `turn_scheduler_test` — speed-50/100/200 actors produced an
exact 4:1 act ratio between the slowest and fastest over 20 turns. See
`ARCHITECTURE_DECISIONS.md` → "Turn scheduler."

## ✅ Prompt 5 — Map and rendering
Tile/Map class rendering to screen, hardcoded test map (no procgen yet).
Basic player movement via keyboard input, to confirm rendering + input +
turn scheduler are integrated correctly. Tiles/player render as flat
colored rectangles (no art assets yet), 20×10 ASCII-art test room, arrow
keys/WASD move the player with wall collision, each move routed through
the real `TurnScheduler`. **Confirmed on the person's machine:** window
opens, grid + player render correctly, movement works, wall collision
(border and interior pillar) actually stops the player rather than
passing through. See `ARCHITECTURE_DECISIONS.md` → "Map, rendering, and
input."

## ✅ Prompt 6 — FOV
Field-of-view (recursive shadowcasting or similar); dimmed "remembered but
not visible" state for explored-but-out-of-sight tiles. `Tile` gained the
`transparent` flag deferred from Prompt 5. `computeFieldOfView()` is a
pure function (no Map mutation, no player/entity knowledge); `ExploredMap`
separately tracks Hidden/Remembered/Visible over time. Verified via a
standalone `fov_test` printing an ASCII grid against a known wall pillar
— confirmed correct shadow-casting behavior including top/bottom symmetry
and vision wrapping around obstacle corners, not just "it compiled."
**Confirmed on the person's machine:** darkness beyond sight radius,
dimmed remembered tiles, and pillar shadow all behaving correctly. See
`ARCHITECTURE_DECISIONS.md` → "Field of view."

## ✅ Prompt 7 — Pathfinding
A* pathfinding for monster movement toward a target, and a basic
AIBehavior strategy class (Chaser) that uses it to move toward the player
when in range. `AIBehavior` is now a true abstract class (`decideMove()`
pure virtual) -- the moment flagged back in Prompt 3. `findPath()` is
4-directional, matching player movement, no diagonals. `Chaser` reuses
`computeFieldOfView()` (Prompt 6) to gate whether it can even see its
target, rather than a raw distance check. One deliberate scope extension
beyond the bare prompt text: an actual goblin (Chaser AI) is now spawned
in the real game, not just proven via standalone tests -- Prompt 5 had
already left the exact hook for this. Verified with unusual rigor given
three interacting non-trivial algorithms: `pathfinder_test` (ASCII grid,
hand-traced a forced 21-tile detour around a wall, matched exactly),
`chaser_test` (hand-computed step-by-step chase sequence, matched
exactly, including correctly stopping at adjacency), plus the two earlier
tests fixed to use the new `NullAIBehavior` instead of instantiating the
now-abstract `AIBehavior` directly. **Confirmed on the person's machine:**
the goblin correctly chases and stops at adjacency in the live game.
See `ARCHITECTURE_DECISIONS.md` → "Pathfinding and Chaser AI."

## ✅ Prompt 8 — Procedural dungeon generation
Rooms + corridors generator producing a connected, playable Map. Quick
regenerate-with-a-keypress debug loop. Random non-overlapping rooms
connected in placement order by L-shaped corridors -- guarantees full
connectivity by construction (like a linked list), no separate graph
pass needed. Deterministic given a seed: fixed seed at launch, fresh
random seed on the regenerate key (**R**). `parseAsciiMap` deliberately
kept (not replaced) for the existing hand-crafted test maps in
`fov_test`/`pathfinder_test`/`chaser_test`, which need deterministic
layouts to hand-verify against -- procedural generation would undermine
that. Verified with real rigor: `dungeon_test` flood-fills from player
start across **10 different seeds**, confirming 100% of floor tiles
reachable in every one (the one property that actually matters for "a
connected, playable Map"), not just eyeballing one printed layout.
Verified end-to-end in the live game too, including the regenerate key
itself -- installed `xdotool` and sent real key events to the actual
SFML window under a virtual display, producing two consecutive
successful regenerations with different seeds/layouts each time, not
just trusting the code path. Map now exceeds the old fixed test room's
size (38x20 vs 20x10) but still fits the current window without
scrolling -- camera/viewport is flagged as the next real gap.
**Confirmed on the person's machine.** See `ARCHITECTURE_DECISIONS.md` →
"Dungeon generation."

## ✅ Prompt 9 — One playable class + talents
Design + implement one playable class, 6–8 talents across 2 talent trees,
with real trade-offs (cooldown vs damage, AoE vs single-target, risk vs
reward) rather than flat power increases. Talent/TalentSet system first,
then this class's talents as data. **Result:** the **Spellblade** -- 8
talents across Blade (melee, must be adjacent, cheap/efficient) and Flame
(ranged/AoE, mana-hungry) trees. `TalentSet` fully replaced its Prompt-3
stub. `AIBehavior`, `Talent`, `TalentSet`, and `TalentEffects` each stay
focused on one job (interface / data / cooldown bookkeeping / damage
application); Application resolves targeting since it's the one thing
that knows about every Actor in the level. One deliberate scope
extension, flagged explicitly: added minimal flat-damage application (not
a general combat formula system) so talents are actually testable in the
live game, not just inert data -- goblin now has real hp (25) and can
die. All 8 talents are instant-effect only; no ongoing buffs/debuffs
(`StatusEffects` stays a stub, still Prompt 10's job). Talent data is a
C++ table, not an external file -- same file-I/O-risk reasoning as the
ASCII test maps. Verified with real rigor: `talent_test` hand-computed
damage/cooldown/conditional-multiplier values, all matched exactly. Full
live end-to-end test via `xdotool`: computed a genuine A*-pathfound route
to the goblin for the fixed seed, walked it in the real window, and cast
three different talents in sequence -- damage numbers (6, then 16, then a
correctly-triggered 3x execute for 30) matched the talent data exactly,
followed by a correct death message and confirmed post-death behavior
(dead target correctly unattackable by both melee and ranged talents,
game stable afterward). **Confirmed on the person's machine:** the kit
feels right in actual play. See `ARCHITECTURE_DECISIONS.md` → "Talent
system."

## ✅ Prompt 10 — Enemy roster + status effects
5–8 enemy types with genuinely distinct AI (ranged kiter, melee rusher,
support/buffer, AoE threat) via the AIBehavior strategy pattern. Whatever
StatusEffect types their kits need (poison, stun, etc). **Result:** 6
enemy types from just 4 `AIBehavior` classes (`Chaser` extended with
attack + on-hit effects → Goblin/Spider/Ogre; new `Kiter` → Archer; new
`Support` → Shaman; new `AoEBomber` → Bomber) -- the founding "data +
which behavior gets plugged in, not a new subclass" philosophy from
Prompt 0, finally proven with a real roster. `StatusEffects` fully
replaced its Prompt-3 stub: Poison, Stun, Empowered, applied via a new
`tickStatusEffects` free function mirroring `TalentEffects`' separation.
`AIBehavior` gained its long-anticipated second verb (`decideAction`
returning `AIDecision`, not just a move) -- monsters can finally attack.
This forced the "game state" refactor flagged as overdue since Prompt 5:
`Application` now owns a `vector<unique_ptr<Monster>>`, not one
hardcoded goblin. `DungeonGenerator` now exposes every room's center so
the roster can populate a level (deterministic given seed, one enemy
type per room, up to 6). Player death is now handled (message + clean
window close; no game-over screen yet -- Prompt 11/12). Monster
abilities reuse the *existing* `Talent`/`TalentSet` from Prompt 9 rather
than a parallel system. Verified with real rigor: `monster_ai_test`
(17 checks) covers `tickStatusEffects` (poison damage/expiry, stun
detection) and Kiter/Support/AoEBomber decision logic against
hand-computed values -- all passed, including catching and fixing two
bugs in the *test's own* distance assumptions, not the code under test.
Live end-to-end via `xdotool`: real A*-computed walks to multiple
enemies produced a full two-way combat exchange with every damage number
matching the data exactly (including the Goblin's own AI closing
distance and attacking *before* the player did), correct Poison
application/refresh-not-stack behavior, a genuine multi-enemy encounter
that killed the player, and confirmed the window closes itself cleanly
on death. **Confirmed on the person's machine:** the full roster plays
correctly in real combat. See `ARCHITECTURE_DECISIONS.md` → "Enemy
roster and status effects."

## ✅ Prompt 11 — Boss + integration pass
One boss encounter as a set-piece finale. Full playthrough start to
finish, fix what's broken or feels bad. **Result: Goblin Warlord**, a
3-phase fight (aggressive melee → cooldown-gated AoE blast while keeping
distance → one-time self-Empower then all-in enraged melee) built from
one new `BossBehavior` class, reusing the existing `TalentSet` (Prompt 9)
and `Empowered` status effect (Prompt 10) rather than new mechanics.
`AIDecision` gained `SelfBuff` and an optional `announcement` string for
phase-transition flavor text, generic enough that `Application` needs no
boss-specific knowledge. Placed in a deliberately larger room at the end
of the room chain (not the start) -- required real empirical tuning, not
just implementation: the first parameter set only found space ~6% of the
time; swept parameters properly and landed on one that succeeds ~97-98%
across 300 seeds. The integration-pass audit -- genuinely reviewing the
whole system, not just adding the boss -- **found and fixed a real bug**:
movement only ever checked terrain walkability, never whether another
actor already occupied the destination tile, so players and monsters
could walk directly onto each other. Never surfaced earlier because every
scripted test happened to stop *adjacent* to targets, never step onto
them. Also refactored three duplicated small geometry helpers (Chaser/
Kiter/AoEBomber) into a shared `AIUtils.hpp` once a third independent
copy (for the boss) made the duplication worth removing. Verified
extensively: `boss_test` (10 checks) confirms every phase threshold,
transition-announced-once behavior, and per-phase decision against
hand-computed values. Live via `xdotool`: confirmed the occupancy fix
directly (a scripted approach got physically blocked by a Goblin that
had moved to intercept, then an immediate attack found it adjacent,
proving the block was a real actor, not a wall) -- and, investigating an
unexpected mid-test block, discovered the *boss itself* had proactively
closed distance once the player entered its sight radius, confirming
Phase 1 behavior is genuinely wired end-to-end in live play (not just
unit-tested): its Ember Bolt response and the boss's counter-attack both
matched `MonsterFactory`'s configured damage numbers exactly, ending in
an honest player death (already critically wounded from the earlier
gauntlet, not a bug) with the window closing cleanly as designed. Phase
2/3 were not observed live in this session (would need a full-health
playthrough reaching the boss) but are hand-verified via `boss_test`, and
Phase 1's live confirmation gives strong confidence the same
already-proven mechanisms (attack/ability/self-buff execution) carry
through correctly. **Confirmed on the person's machine.** See
`ARCHITECTURE_DECISIONS.md` → "Boss encounter and integration pass."

## ⏳ Prompt 12 — Save/load + polish
Save/load for game state. Final polish pass (UI, menus, distributable
build) scoped to time remaining before publishing. **Save/load result:**
a hand-rolled, human-readable text format (`SaveGame.hpp/.cpp`) --
deliberately not JSON/binary, same one-dependency-only reasoning as every
other format decision in this project. Entirely SFML-independent, so
genuinely unit-testable without a window: `savegame_test` round-trips a
hand-built state and checks every field individually (map, fog-of-war,
all 8 talent cooldowns, status effects, a regular monster and the boss
with its own effects), plus missing/corrupt-file handling -- 15 checks,
all passing. Required a real prerequisite: `Monster` didn't remember its
own `MonsterType` after construction, so the enum moved into its own
header (`MonsterType.hpp`, avoiding a circular include with `Monster.hpp`)
and every construction site across the codebase was updated. Bound to
**F5**/**F9**. **Live cross-process testing caught a real bug before it
shipped**: loading initially copied `regenerateLevel()`'s turn-advancing
step, which would have silently applied one unintended extra
status-effect tick to an already-poisoned loaded player (fine for a
freshly generated player, who never has any status effects yet -- not
fine for a loaded one who might). Fixed, then confirmed live: hp restored
as exactly 20/30 (not 18), and a cooldown showed exactly "3 turns left,"
matching the established tick-immediately-after-start semantic from
Prompt 9 precisely. **Polish-pass prioritization is still open** --
genuinely needs to know how much time is available before publishing
before it can be a useful answer rather than a guess; asked directly
rather than assumed. See `ARCHITECTURE_DECISIONS.md` → "Save/load."

---

# Phase 2 — Beyond the vertical slice

The original 12-prompt sequence (above) is complete: a working vertical
slice with one class, a 6-enemy roster, a boss, and save/load. This
section is a newly drafted continuation, not yet executed -- started
because there's real time available and a genuine interest in going
further, specifically: a proper UI, and multiple playable classes on a
Path of Exile-style Strength/Dexterity/Intelligence triangle.

Two things worth being upfront about, since they reshape what "just add
classes" actually means:

1. **There's no text/font rendering anywhere in this engine.** Flagged
   as a deliberate simplification since Prompt 5, re-flagged as
   "genuinely limiting" since Prompt 10. A class-selection screen,
   talent tooltips, a readable HUD, end-game screens -- none of it works
   with colored rectangles alone. This is the actual prerequisite for
   everything else in this phase, not a side item.
2. **`Stats::strength` and `Stats::dexterity` have existed since Prompt 3
   but nothing has ever read them.** Every talent/monster attack is a
   flat number baked into its own data (see ARCHITECTURE_DECISIONS.md,
   "no general combat formula system... only worth building if the
   numbers-as-data approach stops feeling sufficient"). A triangle of
   classes that are supposed to feel different *because of* their
   attribute spread only works if attributes actually do something --
   so this phase means finally building that formula layer, not just
   adding more talent tables.

The existing **Spellblade already occupies the Str+Int hybrid corner**
of the triangle (melee Blade tree + magic Flame tree -- what PoE calls a
Templar). No need for a redundant second hybrid; the new classes should
fill the other two corners.

## ⬜ Prompt 13 — Text rendering + HUD upgrade
Source and bundle a permissively-licensed monospace font (likely via
apt's `fonts-dejavu-core` or similar, copied into `assets/`); build
minimal SFML text-drawing capability -- a small helper, not a new
abstraction layer. Replace the current colored-bar-only HUD with real
readable labels: hp/mana as "23/30" text, talent names + cooldowns as a
readable list instead of only console output, an on-screen combat log
(last few messages) instead of console-only. No new gameplay, no new
classes yet -- purely the infrastructure + presentation upgrade
everything else in this phase depends on, matching the "prove the
pipeline first" discipline of Prompts 2 and 5.

## ⬜ Prompt 14 — Attribute-driven combat formulas
Add `Intelligence` to `Stats` (joining the existing `strength`/
`dexterity`, both currently decorative). Design and implement real
formulas connecting attributes to combat: Strength → bonus physical
damage (+ some hp), Dexterity → dodge/evasion chance (a real chance to
avoid an incoming hit entirely, rolled symmetrically for both the player
and monsters), Intelligence → bonus magic damage + max mana scaling.
Tag `Talent` with a `DamageType` (Physical/Magic) so the right bonus
applies. Recalibrate the Spellblade's existing 8 talents' numbers under
the new formula -- this changes established, tested balance, so it needs
its own careful pass, not a silent side effect. Foundational systems
work, not new content -- closes the long-flagged "no combat formula
system" gap from Prompt 9.

## ⬜ Prompt 15 — Multi-class system + Marauder (pure Strength)
`PlayerClass` enum + `PlayerClassFactory`, mirroring `MonsterFactory`'s
already-proven pattern exactly. A class-selection screen (using Prompt
13's text rendering) shown before the dungeon generates. **Marauder**:
pure Strength, no meaningful mana pool -- resource identity is HP, not
mana (leaning further into the blood-magic precedent Reckless Lunge
already set). Concept sketch, not final numbers: a Cleave (AreaAroundSelf,
hits everything adjacent -- meaningfully AoE now that there's a full
roster to hit), a self-Empower "Rallying Cry" (reusing the existing
Empowered status effect), a high-risk guaranteed-hit "Berserker's Fury"
that costs hp for massive damage, and a no-cost spammable basic strike
(cooldown-only, no resource at all -- a genuinely different economy from
Spellblade's mana-juggling).

## ⬜ Prompt 16 — Shadow (Dexterity + Intelligence) + end-game screens
Third class, completing the initial triangle. **Shadow**: agile
assassin/rogue-caster hybrid, reusing existing mechanisms in new
combinations rather than inventing more -- mobility talents building on
Blink's precedent, a "Backstab"-style conditional-bonus talent (reusing
Execution's conditional-multiplier mechanic from Prompt 9), and
player-castable Poison (currently only Spider can inflict it -- the
player has never had access to it). Also: proper on-screen "You died" /
"You have won" screens (using Prompt 13's text rendering), replacing the
"window just closes" behavior from Prompts 10-11 -- closes that
long-flagged gap too.

## ⬜ Prompt 17+ — open-ended, lower priority
Camera/viewport for dungeons bigger than one screen (flagged since
Prompt 5). Sound (`SFML_BUILD_AUDIO` has been off since Prompt 2,
deliberately, until there was a reason to want it -- there's a reason
now). Additional classes to round out the full PoE-style set (Duelist,
Ranger, Witch, Scion) once the 3-class pattern is proven. Packaging/
distribution for actually publishing. Not sequenced precisely yet --
revisit once Prompts 13-16 are done and it's clearer what matters most.

## Open questions for discussion before starting Prompt 13

- **Naming:** keep "Spellblade" as its own name, or rename to "Templar"
  to match PoE's convention now that there's a real triangle? Either
  works mechanically -- pure naming/flavor preference.
- **Exact attribute formula numbers** (Prompt 14) aren't drafted here on
  purpose -- worth designing live when we get there, the same way
  Spellblade's actual talent numbers were designed live in Prompt 9
  rather than pre-decided in the original `ROADMAP.md`.
- **Dodge chance symmetry:** applying it to monsters too (not just the
  player) is more consistent and reuses one mechanism for both sides,
  but makes fights against Dexterity-flavored content swingier. Worth
  confirming that's the intent before building it.

---

## Planned module breakdown (from initial planning, pre-Prompt-0)

| Module | Responsibility |
|---|---|
| `Entity`/`Actor`/`Item`/`Feature` | Base classes — see `ARCHITECTURE_DECISIONS.md` |
| `TurnScheduler` | Energy/speed-based turn order (Prompt 4) |
| `Map` / `Tile` | Grid, tile types, walkable/transparent flags (Prompt 5) |
| `FOV` | Shadowcasting or similar, computes visible tiles (Prompt 6) |
| `Pathfinder` | A* for monster movement (Prompt 7) |
| `DungeonGenerator` | Produces a `Map` from parameters (Prompt 8) |
| `TalentSet` / `Talent` | Talent definitions, cooldowns, activation (Prompt 9) |
| `StatusEffect` | Base class + subclasses: Poison, Stun, Haste... (Prompt 10) |
| `AIBehavior` | Strategy classes: Chaser, Kiter, Support... (Prompts 3, 7, 10) |
| `Renderer` | Draws the `Map` + entities to screen (Prompt 5) |
| `InputHandler` | Maps keys/clicks to game actions (Prompt 5) |
| `SaveManager` | Serialize/deserialize game state (Prompt 12) |

This is the conceptual breakdown from planning, not a mandated folder
structure — actual folders get created only when the system that needs them
gets built (see `ARCHITECTURE_DECISIONS.md` → "Folder layout").
