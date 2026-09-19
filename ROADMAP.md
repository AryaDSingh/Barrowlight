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

## ⬜ Prompt 5 — Map and rendering
Tile/Map class rendering to screen, hardcoded test map (no procgen yet).
Basic player movement via keyboard input, to confirm rendering + input +
turn scheduler are integrated correctly.

## ⬜ Prompt 6 — FOV
Field-of-view (recursive shadowcasting or similar); dimmed "remembered but
not visible" state for explored-but-out-of-sight tiles.

## ⬜ Prompt 7 — Pathfinding
A* pathfinding for monster movement; a Chaser AIBehavior strategy that uses
it to move toward the player when in range.

## ⬜ Prompt 8 — Procedural dungeon generation
Rooms + corridors (or alternative) generator producing a connected,
playable Map. Quick regenerate-with-a-keypress debug loop.

## ⬜ Prompt 9 — One playable class + talents
Design + implement one playable class, 6–8 talents across 2 talent trees,
with real trade-offs (cooldown vs damage, AoE vs single-target, risk vs
reward) rather than flat power increases. Talent/TalentSet system first,
then this class's talents as data.

## ⬜ Prompt 10 — Enemy roster + status effects
5–8 enemy types with genuinely distinct AI (ranged kiter, melee rusher,
support/buffer, AoE threat) via the AIBehavior strategy pattern. Whatever
StatusEffect types their kits need (poison, stun, etc).

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
