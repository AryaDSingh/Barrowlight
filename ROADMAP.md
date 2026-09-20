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

## ⏳ Prompt 8 — Procedural dungeon generation
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
Sandbox-verified end-to-end in the live game too, including the
regenerate key itself -- installed `xdotool` and sent real key events to
the actual SFML window under a virtual display, producing two
consecutive successful regenerations with different seeds/layouts each
time, not just trusting the code path. Map now exceeds the old fixed
test room's size (38x20 vs 20x10) but still fits the current window
without scrolling -- camera/viewport is flagged as the next real gap.
See `ARCHITECTURE_DECISIONS.md` → "Dungeon generation."

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

## ⏳ Prompt 10 — Enemy roster + status effects
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
on death. See `ARCHITECTURE_DECISIONS.md` → "Enemy roster and status
effects."

## ⬜ Prompt 11 — Boss + integration pass
One boss encounter as a set-piece finale. Full playthrough start to finish,
fix what's broken or feels bad.

## ⬜ Prompt 12 — Save/load + polish
Save/load for game state. Final polish pass (UI, menus, distributable
build) scoped to time remaining before publishing.

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
