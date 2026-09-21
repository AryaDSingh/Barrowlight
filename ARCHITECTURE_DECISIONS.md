# Architecture Decisions — Roguelike Engine

Living record of what's been decided on this project, why, and what to check
before contradicting it. Claude doesn't retain memory between separate
conversations — paste this whole file at the start of a fresh session to
restore context, and ask for it to be updated as new decisions get made.

Last updated: 2026-09-19 (through completion of Prompt 2).

---

## Project scope (locked in Prompt 0)

- Portfolio piece: a **custom** C++ roguelike engine — not a game built on
  top of an existing engine (see "Rejected: Godot" below).
- Architecture style: **composition over inheritance**. `Actor` holds
  component-style members — `Stats`, `AIBehavior`, `Inventory`, `TalentSet`,
  `StatusEffects` — not a deep inheritance tree.
- **Data-driven where reasonable**: monster/talent definitions live
  separately from logic. Not implemented yet — `data/` is currently an
  empty placeholder.
- **C++17 minimum.**
- Vertical-slice target: 1 playable class, 3–5 procedural levels, 5–8 enemy
  types with distinct AI, 1 boss.
- Explicit priority: the person wants to **understand every system**, not
  just receive code — decisions get explained, not just shipped.
- Scope discipline: flag anything that risks creep beyond the vertical
  slice, and flag anything that conflicts with a decision already made here.

## Base class hierarchy: Entity/Actor/Item/Feature (decided pre-Prompt-0)

Decided during initial architecture planning, before the Project's custom
instructions were even written:

```
Entity (base)
├── position, sprite/tile, name
├── Actor (extends Entity) — anything that takes turns
│   ├── Player
│   └── Monster
├── Item (extends Entity) — things sitting on the ground / in inventory
└── Feature (extends Entity) — doors, stairs, traps (non-actor, non-item)
```

Composition still governs `Actor` specifically (see "Project scope" above).
The principle behind the hierarchy: a monster variant is data + which
behavior objects get plugged in, not a new subclass. E.g. a "fast
poisonous flying goblin" is a `Monster` with a speed-tuned `Stats`, a
poison-on-hit status effect, and an `AIBehavior` of `FlyingChaser` — not
`FastPoisonousFlyingGoblin extends Goblin extends Monster`.

See `ROADMAP.md` for the full planned module breakdown this hierarchy sits
within, and the full sequence of prompts this project is following.

## Rendering/windowing library: SFML 3 (decided Prompt 1)

**Decision:** SFML 3.1.0, not SDL3 or raylib.

**Why:** SFML's C++-native RAII types (`sf::Texture`, `sf::Sprite`,
`sf::RenderWindow`) match a composition-based, exception-safe architecture
without hand-writing wrapper classes around C handles first. SFML 3 requires
C++17 itself, matching the project constraint exactly, and its
`std::variant`-based event API rewards idiomatic modern C++. None of the
three candidates impose a scene graph/ECS/game loop, so that wasn't the
deciding factor — paradigm fit was.

**Explicitly considered and rejected:** SDL3 (larger community/tutorial
base, but C API — would've meant writing our own RAII layer first). raylib
(fastest to prototype in, but its global-context C-API style actively
works against the composition/encapsulation story this project is telling).

**Guardrail this implies:** `sf::` types must never leak into `Actor` or
component headers — wrap SFML behind a thin interface instead. (See
`Application`, below, for where this is actually implemented.) Explicitly
decided *against* building a full swappable multi-backend renderer
abstraction — that's scope creep for a single-library vertical slice.

**Revisit if:** tutorial/ecosystem breadth becomes more important than
paradigm fit, or a concrete reason to prefer SDL/raylib shows up later.

## Rejected: Godot

**Considered:** switching the whole project to the Godot engine instead of
a custom C++ engine, partly prompted by Windows toolchain setup friction.

**Decision:** stayed with the custom C++ engine.

**Why:** Godot is a complete engine with its own architecture (Node/scene
tree) and its own renderer — using it would mean *inheriting* an
architecture rather than *designing* one, which directly undercuts the
stated portfolio goal (demonstrating systems/architecture ability). A
middle ground exists (GDExtension: native C++ game logic hosted by Godot's
editor/renderer) but wasn't adopted, since it doesn't actually solve
toolchain friction and only partially preserves the "I designed this"
story.

**Don't re-propose this** unless the underlying goal changes — e.g. shifts
from "demonstrate architecture/systems skill" toward "ship a finished,
playable game as fast as possible."

## Build system: CMake + FetchContent (decided Prompt 2)

- `cmake_minimum_required(VERSION 3.28)` — needed for the modern
  `FetchContent` `EXCLUDE_FROM_ALL SYSTEM` syntax.
- SFML fetched via `FetchContent`, pinned to git tag `3.1.0` (not tracking
  `master`) — reproducible builds.
- `SFML_BUILD_AUDIO` and `SFML_BUILD_NETWORK` are **OFF**. **Revisit this
  when sound or networking is actually needed** — flip the cache vars back
  on rather than reinventing that wiring.
- Only `SFML::Graphics` is linked (pulls in `Window` + `System`
  transitively).
- **Platform-specific output path quirk:** Visual Studio is a multi-config
  generator → binary lands at `build/bin/<Config>/roguelike.exe` (e.g.
  `build/bin/Debug/roguelike.exe`). Ninja/Makefiles (single-config, typical
  on Linux/macOS CLI) → `build/bin/roguelike` directly. This is correct
  CMake behavior, not a bug — do **not** "fix" it by collapsing configs into
  one output folder; that would make Debug and Release silently overwrite
  each other.

## Folder layout (decided Prompt 2)

```
roguelike/
├── CMakeLists.txt
├── README.md
├── .gitignore
├── assets/        <- placeholder, empty (textures/fonts land here later)
├── data/          <- placeholder, empty (data-driven monster/talent defs)
└── src/
    ├── main.cpp
    └── core/
        ├── Application.hpp
        └── Application.cpp
```

Deliberately shallow: `entities/`, `ai/`, `world/`, etc. were **not**
pre-created, since those systems haven't been designed yet. Don't add
subfolders speculatively ahead of the system that needs them.

## Engine boundary: `engine::Application` (decided Prompt 2)

- `src/core/Application.{hpp,cpp}` is the **only** code allowed to
  `#include <SFML/...>` or reference `sf::` types.
- Owns the `sf::RenderWindow` and the loop shell: `run()` →
  `processEvents()` / `update()` / `render()`.
- `update()` is currently an empty stub, reserved for the turn/tick loop
  once there's actual game state to advance.
- Game logic (`Actor` and its components, once built) will depend on
  `Application`'s own interface, never on `sf::` directly.
- This is the concrete implementation of the SFML decision's guardrail
  above. **Any future prompt that has game logic reaching for `sf::`
  directly conflicts with this and should be flagged.**

## Verified working (Prompt 2 completion)

- Compiles clean with GCC 13 (Linux, Ninja generator) — sandbox-verified.
- Compiles clean with MSVC 19.38 / VS 17 2022 (Windows, Visual Studio
  generator) — verified on the person's actual machine.
- Window opens: 1280×720, titled "Roguelike Engine - Dev Window", 60fps cap.
- Closes cleanly on the X button and on Escape — confirmed on the person's
  machine.
- No game logic yet, as intended for this step.

## Entity/Actor implementation (decided Prompt 3)

- All of `Entity`, `Item`, `Feature`, `Stats`, `AIBehavior`, `Inventory`,
  `TalentSet`, `StatusEffects`, `Actor`, `Player`, `Monster` are
  **header-only** for now. Not a style preference — every one of them is
  genuinely trivial at this stage (no logic beyond constructors and
  getters), so a `.cpp` file would hold nothing. **Expect real `.cpp`
  files to appear as actual behavior gets added** (Prompt 4 onward);
  don't read header-only as a permanent pattern for this codebase.
- `TalentSet` and `StatusEffects` are throwaway placeholders (`empty()`
  always returns `true`) — real design is Prompts 9 and 10 respectively.
  **Expect these to be replaced wholesale, not incrementally extended.**
- `AIBehavior` is currently a concrete (non-abstract) empty class — just a
  virtual destructor, no pure virtual methods. It'll gain a
  `decideAction()`-style pure virtual once `Action`/`Map` exist (Prompt
  5/7), at which point it becomes a true abstract base and concrete
  strategies (Chaser, Kiter, ...) get built against it.
- `Entity` uses a plain `char` glyph as a placeholder visual identity
  (roguelike ASCII-tile convention), not a texture ID or sprite handle —
  deferring that decision to Prompt 5 rather than guessing at a tile/sprite
  system now.
- **New verification pattern established:** `entity_smoke_test` is a
  second CMake executable target, console-only, that links no SFML at
  all. It exists to (a) prove the entity/component classes actually
  compile and compose correctly, and (b) enforce the "no `sf::` outside
  `Application`" boundary mechanically — if this target ever fails to
  compile because an entity header pulled in `<SFML/...>`, that's the
  boundary being broken, caught immediately. This matches the standalone
  console-test pattern the roadmap already calls for at Prompt 4 (turn
  scheduler) — expect more targets like this as non-rendering systems get
  built.

## Turn scheduler (decided Prompt 4)

- **Energy lives inside `TurnScheduler`, not on `Actor`/`Stats`.** A small
  internal `Entry{Actor*, energy}` struct pairs each registered actor with
  its own energy counter. `TurnScheduler` doesn't own the `Actor` — the
  caller must `remove()` before destroying one, or risk a dangling
  reference. Considered putting `energy` directly on `Stats` instead
  (simpler lookup, travels with the actor automatically) but rejected it:
  scheduling is simulation-level state, not an actor attribute, and
  keeping it out of `Stats` means `Stats` stays purely "what combat math
  will eventually read," not mixed with "how turn order currently works."
- **Fixed threshold model** (`kActionThreshold = 1000`): every tick, every
  actor's energy increases by `Stats::speed`; whoever crosses the
  threshold acts, and the threshold (not the actor's full energy) is
  subtracted — overflow carries over rather than being discarded, so a
  much-faster actor doesn't lose banked energy. This is the standard
  Angband/ToME energy model.
- **`nextTurn()` deliberately only answers "whose turn is it."** It knows
  nothing about `Action`, `Map`, or `AIBehavior` — what an actor *does*
  once it's their turn is explicitly out of scope until Prompt 5+.
- **Verified with `turn_scheduler_test`**, a third no-SFML console target
  (same enforcement pattern as `entity_smoke_test` — see "Entity/Actor
  implementation" above). Three monsters at speed 50/100/200, run for 20
  turns: the slowest and fastest landed on an exact 4:1 act ratio,
  confirming the speed→frequency relationship is correct rather than just
  plausible-looking.
- **Not wired into `Application`/`main.cpp` yet** — per the prompt's own
  scope, that's deferred to Prompt 5, once there's a `Map` and real
  gameplay for turn order to actually drive.

## Map, rendering, and input (decided Prompt 5)

- **Rendering is flat colored rectangles, not glyphs or sprites.** No font
  or texture assets exist yet, and building either pipeline (font
  loading + monospace layout, or a sprite/tile-atlas system) is real
  design work on its own -- arguably deserving its own future prompt once
  actual art exists. Walls/floor/player are distinguished by color alone.
  This is a fully self-contained choice inside `Application::render()`;
  swapping in glyphs or sprites later touches nothing in `Map` or
  `Entity`, since neither has any idea how it's drawn.
- **The hardcoded test level is a source-literal ASCII array, not a
  `data/` file.** Loading from `data/` would fit the project's
  data-driven preference well, but introduces file I/O and path
  resolution on top of the Windows path friction already hit earlier in
  this project, right when the goal is a clean verification of three
  *other* systems integrating. Data-driven level loading is a natural fit
  once real levels need authoring outside code -- likely Prompt 8.
- **`Position` moved out of `Entity.hpp` into `core/Position.hpp`.** Once
  `world/` also needed a coordinate type, leaving it homed inside
  `entities/` would have made `world/` depend on `entities/` for
  something both modules need equally. It's foundational/shared, so it
  now lives in `core/`.
- **`Map` gained a default constructor** (`Map() = default`, empty 0×0)
  specifically so `Application` can hold `Map map_;` as a plain member
  and assign its real content in the constructor body, rather than
  contorting member-initializer-list ordering to make the ASCII parse
  happen in one expression. Slightly less clever, deliberately: order-
  dependent initializer lists are a well-known C++ footgun, and this
  avoids the whole category of bug for a one-constructor cost.
- **`Application` now directly owns `Map`, `Player`, and `TurnScheduler`.**
  This is the simplest wiring that proves rendering + input + turn
  scheduling actually integrate. Expect a dedicated "game state" concept
  to get pulled out once there's enough state to justify separating
  "what's being simulated" from "the window" -- multiple monsters, level
  transitions -- probably around Prompt 7/8. Not built now on purpose;
  nothing yet needs it.
- **Movement is turn-gated, not real-time.** A keypress attempts a move;
  if the target tile is walkable, the player moves and `nextTurn()` is
  called again. Bumping a wall consumes no turn (standard roguelike
  behavior). With only the player registered, `nextTurn()` trivially
  always returns the player right now -- but it's routed through the real
  scheduler API rather than skipped, so adding monsters (Prompt 7) means
  extending `tryMovePlayer`'s aftermath, not restructuring the loop.
- **`entity_smoke_test` and `turn_scheduler_test` are unaffected** by this
  prompt (no `Map`/`TurnScheduler`-in-`Application` coupling reaches
  them) -- confirmed by rerunning both after the change.

## Field of view (decided Prompt 6)

- **`Tile` gained `transparent`** — the field explicitly deferred back in
  Prompt 5 ("held off on `transparent` until Prompt 6 needs it"). Walls
  are opaque, floor is transparent; correlates with `walkable` for now,
  same reasoning as before for keeping them separate fields rather than
  deriving one from the other.
- **`computeFieldOfView(map, origin, radius)` is a pure function.**
  Recursive shadowcasting (the standard octant-transform/slope-tracking
  algorithm), no knowledge of players or entities, no mutation of `Map`,
  no memory between calls. Given the same map/origin/radius it always
  returns the same result — deliberately kept this way so it's testable
  in complete isolation from everything else (see verification below).
- **`ExploredMap` is a separate class** holding the *stateful* part —
  Hidden/Remembered/Visible per tile, updated once per turn from a fresh
  `computeFieldOfView()` result. Splitting "compute visibility" (pure,
  stateless) from "remember what's been seen" (stateful, simple
  bookkeeping) keeps both pieces small and independently reasoned about,
  consistent with how `TurnScheduler` and `Map` are each scoped to one
  job.
- **Verified by printing an ASCII grid, not just by compiling.**
  Shadow-casting bugs are exactly the kind that "look plausible" while
  being subtly wrong (common failure: asymmetric shadows). `fov_test`
  builds a room with a single wall pillar and prints the FOV result as
  text; confirmed by eye: direct line-of-sight blocked behind the pillar,
  correct top/bottom symmetry around the blocked row, and vision
  correctly wrapping around the pillar's corners into the rows beyond it.
- **Dimming is computed** (`dim()` scales RGB by a constant factor) rather
  than hand-picked per tile color, so "remembered" always stays
  programmatically tied to "visible," however many tile colors exist
  later.
- **Sight radius is a named constant (8 tiles),** not yet exposed as a
  per-actor stat. Revisit once talents/equipment might plausibly modify
  sight range (Prompt 9+).

## Pathfinding and Chaser AI (decided Prompt 7)

- **`AIBehavior::decideMove()` returns `std::optional<Position>`, not a
  full `Action` type.** There's exactly one verb that exists in this game
  right now (move) -- a polymorphic Action/Command hierarchy for a single
  implementation would be the premature abstraction the "keep me scoped"
  instruction warns against. Revisit once combat exists (Prompt 10+) and
  there's an actual second verb (attack) to decide between.
- **`AIBehavior` is now a true abstract class** (`decideMove()` pure
  virtual) -- the exact moment flagged in Prompt 3. Broke both earlier
  tests that did `std::make_unique<AIBehavior>()`; fixed by introducing
  `NullAIBehavior` (a trivial concrete "never moves" class) rather than
  coupling those entity/scheduler tests to `Chaser` just to satisfy the
  abstract base. `NullAIBehavior` is also a legitimate "dormant"/"guard"
  behavior in its own right, not purely a test artifact.
- **`AIBehavior.hpp` only forward-declares `Actor` and `Map`**, doesn't
  `#include` them -- `Actor.hpp` already includes `AIBehavior.hpp` to
  declare its `ai_` member, so including `Actor.hpp` back would be
  circular. Reference parameters in a pure-virtual declaration don't need
  the full type, only the `.cpp` files that actually call methods on them
  do.
- **`findPath()` is 4-directional**, matching how player movement already
  works (no diagonals wired anywhere). A monster that could cut corners
  the player can't would be an inconsistent rule the player has no way to
  see coming.
- **`Chaser` reuses `computeFieldOfView()`** to decide whether it can see
  its target, rather than a raw distance check -- a direct payoff of
  Prompt 6 deliberately not hard-coding FOV to "the player." A `Chaser`
  now can't "see" through walls just because a target is within radius
  distance.
- **The `targetPosition` parameter isn't named "playerPosition"** even
  though it's always the player for now -- nothing in the interface
  should hard-code that, so monster-vs-monster targeting or guarding a
  fixed point (later) aren't fighting the interface's naming.
- **Monster rendering checks `Visibility::Visible` specifically, not
  `Remembered`.** Unlike static terrain, a monster that has moved away
  from a tile the player once saw shouldn't still appear to be standing
  there -- "remembered" makes sense for a wall, not for something that
  moves on its own.
- **Scope extension beyond the bare prompt text:** actually spawned a
  goblin in `Application` rather than stopping at standalone tests.
  Prompt 5 had explicitly left this exact hook ("once monsters exist,
  this is the spot that needs to branch"), and watching a monster
  actually chase across the window is a much stronger integration check
  than trusting a console-printed path alone.
- **Verified with more rigor than usual**, given three interacting
  non-trivial algorithms landed in one prompt: a standalone
  `pathfinder_test` (hand-traced a forced 21-tile detour around a wall --
  matched exactly) and a standalone `chaser_test` (hand-computed the
  exact step-by-step chase sequence including the stop-at-adjacency case
  -- matched exactly), on top of the existing `fov_test` that `Chaser`
  now depends on.
- **`Application` is getting close to the "game state" extraction point**
  flagged as a future refactor back in Prompt 5 -- now owns `map_`,
  `player_`, `goblin_`, `scheduler_`, `exploredMap_`. Still deliberately
  not done: this is one more monster, not yet enough state to force the
  issue. Likely due by Prompt 8 or once a second monster exists (Prompt
  10).

## Dungeon generation (decided Prompt 8)

- **Random rooms + corridors, connected in placement order**, not BSP.
  Each new room connects to the *previous* room by an L-shaped corridor
  -- this guarantees the whole dungeon is one connected component by
  construction, the same way a linked list is, without needing a
  separate graph/connectivity pass afterward. BSP would guarantee this
  too, but at more implementation/explanation complexity for no real
  gain here.
- **Deterministic given a seed** (`std::mt19937`, explicit seed
  parameter). Same seed always produces the same layout. This is what
  makes `dungeon_test`'s checks reproducible, and it's also literally how
  the in-game regenerate key works: a new seed, not a different
  algorithm or any special-cased "randomize" path.
- **`parseAsciiMap` was deliberately kept, not replaced.** It's no longer
  used for the *live* game's map, but `fov_test`, `pathfinder_test`, and
  `chaser_test` all still build deterministic, hand-crafted test maps
  with it -- procedural generation would undermine those tests' whole
  point (verifying known geometry by hand). Two tools for two different
  jobs: `parseAsciiMap` for "I need this exact known layout to check
  against," `generateDungeon` for "give me a real playable level."
- **The dungeon stays within the current window's size (38×20 tiles,
  1216×640px in a 1280×720 window) rather than being genuinely
  screen-exceeding.** A "real" dungeon arguably should be bigger for
  actual exploration gameplay, but adding camera/viewport scrolling in
  the same prompt as dungeon generation would be answering a question
  this prompt didn't ask. Flagged repeatedly as an open item since Prompt
  5 -- this is now clearly the next sharp edge, not a vague someday.
- **Monster spawns in the last room placed, player in the first.** A
  simple heuristic (not true graph-distance-based placement) that
  reliably keeps them apart given how rooms chain together. Revisit if
  it ever produces awkwardly-close spawns, or once multiple monsters
  (Prompt 10) need smarter placement.
- **Verified with real rigor, not just one eyeballed layout:**
  `dungeon_test` flood-fills from player start across 10 different seeds
  and confirms 100% floor-tile reachability in every one -- the actual
  property "a connected, playable Map" requires, checked programmatically
  rather than trusted because the carving logic looks simple. The
  regenerate key itself was verified by installing `xdotool` and sending
  real key events to the live SFML window under a virtual display,
  producing genuinely different seeds/layouts across two consecutive
  presses -- not just trusting that the code path compiles.

## Talent system (decided Prompt 9)

- **Class concept: Spellblade**, two trees that differ in *delivery
  mechanism*, not just numbers -- Blade (melee, must be adjacent,
  positional risk, cheap/efficient) and Flame (ranged/AoE, mana-hungry,
  safer positioning but costlier). Chosen specifically because it doesn't
  need any control/crowd-control mechanics (which would require
  `StatusEffects`, still a stub) to feel distinct.
- **All 8 talents are instant effects only -- no ongoing buffs/debuffs.**
  `StatusEffects` staying a stub is a Prompt 3 decision that still holds;
  building duration-tracking now to serve one class's kit would be
  exactly the premature infrastructure this project has repeatedly
  avoided. Revisit when Prompt 10 actually designs `StatusEffect` types.
- **Talents carry their own flat damage numbers; there's still no general
  combat formula system.** `Stats` staying mathless (no methods, just
  data) was a deliberate Prompt 3 decision -- this doesn't walk it back.
  A talent's `power` field is data the talent itself declares, not a
  formula involving strength/defense/resistances. That kind of general
  damage-resolution system is legitimately Prompt 10's territory, once
  there's a real enemy roster to justify a consistent formula across many
  attackers.
- **Talent.hpp / TalentSet.hpp / TalentEffects.hpp / SpellbladeTalents.hpp
  each stay focused on one job:** `Talent` is pure data (no behavior),
  `TalentSet` is per-actor cooldown bookkeeping (no effect logic),
  `TalentEffects::applyTalentDamage` is the one place damage actually
  gets applied (doesn't touch cooldowns/costs/targeting), and
  `SpellbladeTalents` is just the data table. Mirrors the same
  separation-of-concerns pattern used throughout (`Map`/`FieldOfView`/
  `Pathfinder` each doing one thing).
- **Application resolves talent targeting (who's affected), not
  Talent/TalentEffects.** Application is the one thing that currently
  knows about every Actor in the level, so "who's in this AoE" has to be
  answered there. For `AreaAroundTarget`/`AreaAroundSelf`, the mechanism
  is genuinely area-based (not special-cased for exactly one monster) --
  it just can't currently *add* anyone beyond the anchor target, since
  there's only one monster to find. This will visibly matter once Prompt
  10 adds more enemies; it's not faked to look right today.
- **Deliberate scope extension, flagged explicitly:** added a minimal
  damage-application mechanism (`applyTalentDamage`, `Stats::hp -=
  damage`) so talents are genuinely testable in the live game rather than
  inert data nobody can observe working. The goblin's hp was bumped from
  the Prompt-7 default (10) to 25 specifically so a fight has some real
  duration/decisions rather than dying to any single talent instantly.
  Explicitly **not** built: enemy attacks (goblin still can't hurt the
  player -- Chaser only moves), armor/resistances, damage types, critical
  hits. Two-way combat is Prompt 10's job.
- **Talent data is a C++ table, not an external file** (same reasoning as
  the ASCII test maps in `Map.cpp`): a JSON/data-file pipeline would need
  a new parsing dependency and introduces the same file-path-resolution
  risk already hit with Windows earlier in this project, for a prompt
  whose actual focus is talent design and trade-offs, not a content
  pipeline. "Data-driven" here means logic (`TalentEffects`,
  `TalentSet`) is fully separated from data (`SpellbladeTalents`' table)
  -- the table is trivially swappable for a file loader later without
  touching any resolution logic, but no such loader exists yet.
- **Blink's direction comes from the player's last move**
  (`lastMoveDirection_`, a new `Application` member), not a separate
  aiming/targeting-cursor system -- avoids building real UI/input
  machinery for one talent's sake. It stops at the last walkable tile
  before an obstacle rather than requiring the full distance to be clear.
- **No text rendering exists, so talent feedback is console output**
  (what got cast, damage dealt, why a cast failed, cooldown/mana
  messages) plus simple HP/mana bars using the same rectangle-drawing
  already in place for tiles -- not a UI system. Consistent with how
  dungeon generation already reports to the console.
- **`std::cout << std::unitbuf;`** added at the top of `Application`'s
  constructor -- auto-flushes every `cout` insertion from then on. Fixes
  the same stdout-buffering gap hit back in Prompt 5 (output invisible if
  the process is interrupted rather than exiting normally) for all
  current and future diagnostic lines at once, rather than remembering
  `std::endl` on each one individually.
- **Verified with real rigor:** `talent_test` hand-computes exact
  expected damage (including the Execution conditional multiplier) and
  cooldown-tick sequences against the actual code, not just plausible
  numbers. The live integration test went further than any prior
  prompt's: rather than hoping simulated keypresses would land near a
  monster, `findPath` (already verified in Prompt 7) was reused to
  compute a genuine walkable route from the fixed seed's player start to
  the goblin, that exact route was played back via `xdotool` against the
  real running window, and three different talents were cast in sequence
  -- the resulting damage numbers (6, then 16, then a correctly-triggered
  30 from Execution's 3x multiplier at 12% target hp) matched
  `SpellbladeTalents`' data exactly, followed by correct death handling
  and confirmed stability afterward (dead target correctly unattackable,
  no crash).

## Enemy roster and status effects (decided Prompt 10)

- **4 `AIBehavior` classes produce 6 enemy types, not 6 classes.**
  `Chaser` (extended with an attack + optional on-hit status effect,
  parameterized via `MonsterAttackProfile`) powers Goblin, Spider, and
  Ogre. New `Kiter`, `Support`, and `AoEBomber` cover the remaining
  archetypes. This is the project's founding philosophy from Prompt 0 --
  "a monster variant is data + which behavior objects get plugged in,
  not a new subclass" -- finally proven out with a real roster instead
  of a single example.
- **`AIBehavior` gained its second verb.** `decideMove()` (Prompt 7)
  became `decideAction()`, returning an `AIDecision` (Move / Attack /
  UseAbility / Wait) instead of just an optional position. This is
  exactly the moment flagged repeatedly since Prompt 7/9: enemies could
  only ever move, never attack, until now. `AIDecision` carries the
  *already-resolved* outcome (attack power, any on-hit effect, already
  rolled) rather than Application reaching back into a behavior's private
  state to figure out what an Attack meant.
- **Simple attackers use `MonsterAttackProfile` (flat damage + optional
  on-hit effect), not the full `Talent`/`TalentSet` machinery.** A
  monster with exactly one repeatable move doesn't need cooldown
  tracking. Only `Support` and `AoEBomber` -- which genuinely need "one
  special move on a cooldown" -- reuse `TalentSet`, and only for its
  cooldown bookkeeping; the buff/blast magnitude are the behavior's own
  constructor parameters, not stored in the `Talent` entry (whose other
  fields are damage-application-oriented, not buff-oriented).
- **Monster abilities reuse the *existing* `TalentSet` from Prompt 9,
  not a parallel system.** Nothing about `TalentSet`/`Talent` was ever
  player-specific -- this is exactly the kind of reuse that composition
  over duplication is supposed to buy later, paying off two prompts
  later than expected.
- **`StatusEffects` fully replaced its Prompt-3 stub** with 3 types:
  Poison (damage/turn), Stun (skip a turn), Empowered (flat damage bonus
  for Support to grant). Deliberately **not** a speed buff (Haste) --
  that would mean modifying `TurnScheduler`'s effective-speed
  calculation, touching already-tested Prompt-4 code; a damage buff only
  touches the monster-attack code being written fresh this prompt
  anyway.
- **`tickStatusEffects` is a separate free function** (`StatusEffectLogic.hpp`),
  mirroring `TalentEffects`' split from `TalentSet`: `StatusEffects`
  itself is pure bookkeeping (which effects, how long), the tick function
  is the one place poison damage/stun-detection/expiry actually happens.
  Called once per turn for *every* actor (player included) -- a stunned
  player doesn't get to act either, handled by a new
  `advanceTurnsUntilPlayerCanAct()` that skips forward (still consuming
  turns, still ticking effects) until the player genuinely has a turn to
  spend on input.
- **This prompt forced the "game state" refactor flagged as overdue
  since Prompt 5.** `Application` now owns
  `std::vector<std::unique_ptr<Monster>> monsters_` instead of one
  hardcoded `goblin_` -- there was no way to support a 6-enemy roster
  without it. Still not a full extraction into a dedicated class (window
  vs. simulation state stays combined) -- that remains deliberately
  deferred; nothing here demanded going further than this.
- **`DungeonGenerator` now exposes every room's center**
  (`otherRoomCenters`, replacing the old singular `monsterStart`), so
  `Application` can populate up to 6 rooms with different monster types
  -- deterministic given the seed, one type per room in roster order,
  gracefully partial if a layout has fewer non-player rooms than 6.
- **Real AoE, not faked.** Prompt 9's Fireball/Immolate only ever had one
  possible target to hit (the single goblin); `actorsWithinRadius()` now
  genuinely scans every living monster, so a player's AoE talent (or the
  Bomber's blast) can hit multiple actors at once for the first time.
  This was flagged explicitly in Prompt 9 as "will visibly matter once
  Prompt 10 adds more enemies" -- confirmed live (see verification below).
- **Player death is handled, but minimally.** hp <= 0 prints a message
  and calls `window_.close()` -- the same clean-exit path as pressing
  Escape. No game-over screen, no restart flow; building either is
  explicitly Prompt 11/12 territory ("integration pass," "final polish"),
  not something to improvise here.
- **Targeting for the player's own talents now searches the whole
  roster**, not a single hardcoded monster: `findAdjacentEnemy()` (first
  match; adjacency is binary, order doesn't matter) and
  `findNearestVisibleEnemy()` (genuine nearest-distance search, since
  multiple visible enemies at different ranges is now a real scenario).
  No targeting-cursor UI -- flagged explicitly as a real simplification,
  not hidden.
- **Monster AoE (Bomber) targets the player only, not other monsters.**
  No faction/friendly-fire system exists. A fuller implementation might
  have a Bomber's blast threaten its own allies too, relying on its
  positioning to avoid that -- more than this roster needs to demonstrate
  the archetype.
- **Verified with real rigor, including catching bugs in the test itself,
  not just the code under test:** `monster_ai_test` (17 checks) covers
  `tickStatusEffects` and the three new behaviors against hand-computed
  values. Two checks initially failed -- not because `Kiter`/`AoEBomber`
  were wrong, but because the test's "target too far" distance
  accidentally exceeded the *default sight radius* as well as the
  intended attack range, so the correct answer (can't see it, Wait) was
  being checked against the wrong expectation (should approach). Fixed
  by correcting the test's distances, not the behavior code -- worth
  recording as an example of validating the test's assumptions, not just
  trusting a first failure means the implementation is wrong.
- **Live verification went further than any prior prompt's:** real
  A*-computed walks (reusing `findPath`, already verified in Prompt 7) to
  multiple different enemies, played back via `xdotool` against the
  actual running window. Results: the Goblin's own `Chaser` AI closed
  distance and attacked *before* the player initiated combat (bidirectional
  combat confirmed, not just claimed); every damage number across two
  separate encounters matched `MonsterFactory`'s data exactly; Poison
  correctly applied, dealt its exact magnitude, and refreshed (not
  stacked) on repeated hits; a genuine multi-enemy encounter (Goblin +
  Spider simultaneously) killed the player, with the death correctly
  detected at exactly 0 hp; and the process was confirmed to exit on its
  own afterward, without needing to be killed, proving `window_.close()`
  actually fires rather than just compiling.

## Boss encounter and integration pass (decided Prompt 11)

- **Goblin Warlord: 3 phases, built from one new `BossBehavior` class,
  reusing existing mechanisms rather than inventing new ones.** Phase 1
  (>60% hp): aggressive melee, same shape as `Chaser`. Phase 2 (30-60%):
  a cooldown-gated AoE blast while trying to keep distance, reusing
  `TalentSet` exactly the way `AoEBomber`/`Support` already do. Phase 3
  (<30%): a *one-time* self-Empower (the same status effect Shaman
  already grants allies, from Prompt 10) before an all-in enraged final
  stand -- the "hits harder while enraged" story comes entirely from
  Empowered's existing damage-bonus mechanic in `executeAIDecision`, not
  a separate phase-3 power number.
- **Phase is always re-derived from current hp fraction, not tracked as
  state.** The only deliberate exceptions: `enraged_` (has the one-time
  buff trigger fired yet) and `announcedPhase_` (so transition flavor
  text prints once, not every turn in a phase) -- narrow, single-purpose
  state, not a departure from the stateless-by-default pattern the rest
  of the roster follows.
- **`AIDecision` gained `SelfBuff` and an optional `announcement`
  string.** `SelfBuff` applies `effectToApply` to the acting actor
  itself (using the `Actor&` `executeAIDecision` already receives, not a
  new mutable-self parameter on the `AIBehavior` interface -- considered
  changing `decideAction`'s `self` from `const Actor&` to `Actor&`
  instead, rejected as a broader, less-necessary signature change across
  every existing behavior for the same result). `announcement` is
  deliberately generic (any behavior could set it), not boss-specific --
  `Application` prints it without knowing or caring which behaviors are
  "bosses."
- **Boss room placement needed real empirical tuning, not just
  implementation.** The first design (max 7 regular rooms, boss sized
  9-12) found space for the boss only ~6% of the time across 100 seeds --
  discovered by testing, not assumed. Swept several room-count/size
  combinations empirically and landed on max 6 regular rooms sized 3-5,
  boss sized 7-9: ~97-98% success across 300 seeds, while still averaging
  5 of the 6 roster types per dungeon. Reusing `dungeon_test`'s
  established pattern (measure a property across many seeds, don't trust
  one layout looking right) is exactly what caught this before it shipped
  as a mostly-broken feature.
- **The boss room is placed *last*, after all regular rooms, connected to
  whichever regular room ended up last** -- not first (which would place
  it right next to the player's own starting room). This was a genuine
  design tension: placing it first would have had a far higher success
  rate (much more free space available), but would contradict "a
  set-piece at the end of the level sequence." Reliability was tuned
  through room size/count instead of through placement order, to keep the
  topology correct.
- **The room-placement logic was refactored into a shared `tryPlaceRoom`
  helper**, used by both the main loop and the boss-room attempt with
  different size ranges -- this is a pure refactor (verified: `dungeon_test`
  passed unchanged before and after), not new logic, done specifically
  so the boss room didn't need to duplicate the overlap/carve/connect
  logic a second time.
- **The integration-pass audit found a real, previously-unnoticed bug:**
  both `tryMovePlayer` and monster movement (in `executeAIDecision`) only
  ever checked `map_.isWalkable()` -- terrain -- never whether another
  actor already stood on the destination tile. Nothing in earlier testing
  caught this because every scripted verification (Prompts 9-10) computed
  a path to a tile *adjacent* to a target and stopped there, by design --
  never actually attempting to step onto an occupied tile. A real player
  moving freely, or two monsters converging from different angles, could
  trigger it. Fixed with a new `isOccupied()` check, applied consistently
  to player movement, monster movement, and Blink's destination
  resolution (which needed converting from a free function to a private
  `Application` method specifically to get access to actor positions).
- **Three duplicated geometry helpers (`isAdjacent`, `distanceSquared`,
  `sign`) were extracted into `ai/AIUtils.hpp`** once `BossBehavior`
  needed a third independent copy of logic already present in `Chaser`
  and `Kiter`/`AoEBomber` -- the "rule of three" that makes deduplication
  worth doing rather than premature. Verified behavior-preserving:
  `chaser_test` and `monster_ai_test` passed identically before and after
  the refactor.
- **Verified with real rigor, and one genuine surprise mid-verification:**
  `boss_test` (10 checks) hand-computes every phase threshold,
  transition-announced-once behavior, and per-phase decision, all
  matching exactly. Live testing via `xdotool` first confirmed the
  occupancy fix directly -- a scripted approach toward the Spider got
  physically blocked partway, and an immediate attack found a Goblin
  (not the intended target) adjacent, proving a real actor blocked the
  path, not a wall. A second live test then hit an unexplained block much
  further down the same corridor; rather than guessing, a fresh path was
  computed from the player's actual stuck position, which reported the
  route as clear -- meaning something *had moved into it*. Investigating
  properly (a ranged attack, safer than melee at low hp) revealed the
  Goblin Warlord itself had proactively closed distance once the player
  came within its sight radius, confirming Phase 1 is genuinely wired
  end-to-end in live play: Ember Bolt's 7 damage and the boss's 9-damage
  counter-attack both matched `MonsterFactory`'s configured numbers
  exactly, ending in an honest player death (already critically wounded
  from the earlier fight, not a bug) with the window closing cleanly.
  Phase 2/3 weren't observed live this session (would need a
  full-health playthrough reaching the boss), but are hand-verified via
  `boss_test`, and Phase 1's live confirmation exercises the same
  already-proven execution paths (attack/ability/self-buff handling)
  Phase 2/3 also use.

## Multi-class system and Marauder (decided Prompt 15, Phase 2)

- **`PlayerClassFactory` mirrors `MonsterFactory`'s shape exactly** --
  one factory, a switch over an enum, `statsForClass`/`talentSetForClass`
  returning fully-configured data. The same pattern that already proved
  itself across six monster types plus a boss, applied to player classes
  without needing to invent anything new.
- **`Player`'s constructor no longer hardcodes `TalentSet
  (spellbladeTalents())` internally.** This was a real, blocking
  coupling -- discovered by tracing exactly how `player_`'s talents got
  set up before writing a line of the class-selection system, not
  assumed. `Player` now takes a `TalentSet` parameter, the same shape
  `Monster` already had. Every test-local `Player` construction across
  the whole suite needed updating for the new signature -- an initial
  targeted grep found 2 sites; a second, more thorough pass (after the
  first full rebuild still failed to compile) found 14 more across
  `boss_test.cpp` and `monster_ai_test.cpp`. Worth noting as a real
  lesson: the first grep pattern was too narrow (it matched some
  constructions but not others using different local variable names),
  and only a broader pattern across the whole repo caught everything.
- **`GameMode` (ClassSelection/Playing), not a full scene/state-machine
  framework.** One enum, one member, a handful of `if (mode_ == ...)`
  branches in `processEvents()`/`render()`. Consistent with this
  project's established "minimal but real" UI philosophy (Prompt 13) --
  a second screen doesn't justify a general-purpose state machine when
  there are only ever going to be a small, known number of screens.
- **The class-selection screen is a plain text menu**, not a design
  requiring mouse support, hover states, or visual polish beyond what
  Prompt 13's `drawText` already provides. Two classes, two numbered
  options, a one-line instruction -- exactly as much UI as the decision
  actually needs.
- **`Talent::isHeal` (a single bool, added at Prompt 14) became
  `TalentEffectKind`, a proper three-value enum (Damage/Heal/SelfBuff),
  plus a `selfBuffEffect` field.** Rallying Cry needed a third kind of
  effect a talent could have; rather than bolting on a second bool
  (`isSelfBuff`) alongside the first, generalized into an enum once a
  third case made the pattern clear -- mirroring `AIDecision`'s own
  discriminated-by-enum shape (`AIActionType`), which already existed
  as precedent for "one struct represents different kinds of things" in
  this codebase. `effectKind` and `selfBuffEffect` both sit at the very
  end of `Talent`, following the exact positional-aggregate-
  initialization safety rule established when `damageType` was added
  (see "Attribute-driven combat" above).
- **Self-targeting needed a real resolution path for `SingleTarget`
  shape, reused from Renewal (Prompt 14's healing addition) rather than
  rebuilt.** `TargetingMode::Self` already existed but was never
  actually read by `tryUseTalent`'s anchor-resolution logic before
  Renewal added the `Self` + `SingleTarget` → "resolves to the caster
  directly" case; Rallying Cry reuses that exact same resolution rather
  than needing its own.
- **A real bug live testing caught that no unit test would have: Rallying
  Cry's Empowered buff did nothing to the caster's own damage.**
  `executeAIDecision` (monster attacks, Prompt 11) already checked
  `attacker.statusEffects().has(Empowered)`; `applyTalentDamage` (every
  player attack, since Prompt 9) never did -- a gap that existed from
  the very first talent system and was never exercised, because no
  earlier talent ever needed to check the attacker's own status effects.
  Every damage number in a live Marauder fight after casting Rallying
  Cry matched the *un-buffed* formula exactly (Slam: base 4 + strength
  bonus 5 = 9, not 13), which is what surfaced it. Fixed by adding the
  identical check to `applyTalentDamage`, folded in before the
  conditional multiplier (same reasoning as the attribute bonus: an
  execute amplifies the attacker's full output, buffs included). While
  writing the regression test for this fix, caught a *second*, smaller
  bug -- in the test itself: the attacker `Stats` used for the new check
  had been left at the default strength (10) instead of the Marauder's
  real 20, which made the fix's own test fail even though the fix was
  correct. Traced by hand-recomputing the expected arithmetic rather
  than assuming either the code or the test was simply wrong, and fixing
  the actual mistake (the test's setup) rather than adjusting the
  expected value to match whatever the buggy test produced.
- **`F9` (load) is reachable from both `GameMode`s, checked before the
  mode branch rather than only inside the Playing-mode key switch.**
  Found by literally trying to load a save from a fresh launch (which
  now starts at class selection, not auto-started as the Spellblade the
  way every earlier prompt's testing assumed) and getting silence --
  the key press was reaching `processEvents()` but nothing handled it
  outside Playing mode. `loadGame()` now sets `mode_ = Playing` on
  success explicitly, rather than assuming the caller was already there.
- **`Stats::intelligence` was missing from the save format entirely --
  a real latent bug from Prompt 14, caught only because this prompt
  needed to touch `SaveGame.cpp` again anyway for `playerClass`
  persistence.** Every save silently dropped the player's actual
  Intelligence value back to the `Stats` default (10) on load. The
  original `savegame_test` never caught this because it never set a
  non-default Intelligence value in the first place -- confirmed this
  directly by checking the old test, not assumed. Fixed alongside adding
  `playerClass`, with the save format version bumped to 2 (a version-1
  save is now simply rejected, not migrated -- see "Save/load" above for
  why that's an acceptable simplification at this project's scale).
- **Loading reconstructs the saved player's class -- and therefore the
  correct talent list -- before applying saved cooldowns.**
  `playerCooldowns` is a flat list of remaining-cooldown values applied
  positionally by index; without first setting `player_.talents()` to
  match `state.playerClass`, a loaded Marauder save's cooldowns could
  silently apply to whatever talents `player_` happened to already have
  configured (e.g. a Spellblade's, if the game hadn't been reset since
  launch) -- wrong values landing on the wrong talents with no error at
  all. Verified live, precisely: saved a Marauder mid-fight with Cleave
  on a real cooldown, loaded into a completely separate fresh process,
  and confirmed the HUD showed exactly 4 Marauder talents (never a
  Spellblade default) with Cleave correctly still on cooldown, hp/mana
  exactly matching what was saved.
- **Marauder's maxHp (45) is hand-tuned data, not a dynamic Strength-hp
  formula.** Strength giving a real hp bonus was explicitly deferred at
  Prompt 14 (documented there as real scope avoided on purpose, not an
  oversight) specifically because implementing it would have meant
  rebalancing every already-tuned hp value project-wide. "Tankier than
  the Spellblade" is expressed the same way every other hp value in this
  project already is -- a deliberate number in the class's own data --
  rather than reopening that deferred decision under this prompt's
  different scope.

## Healing spell: Renewal (player-requested, post-Prompt 14)

- **The Spellblade had no way to recover hp at all before this.** Mana
  regenerates per turn (the Prompt 13 follow-up fix), but there was no
  hp equivalent -- once damaged, the only paths were death or the run
  ending. A real, if honest, gap once a fight could genuinely outlast
  the player's starting hp pool.
- **A 9th talent, not a replacement for any of the original 8.** Key 9,
  joining the Flame tree (5 talents now, Blade stays at 4) since it's
  Intelligence-scaled -- there's no Strength-scaled equivalent for
  healing, the way there is for the two damage types.
- **A genuinely separate `applyTalentHeal` function, not a sign flip on
  `applyTalentDamage`.** The two have real behavioral differences beyond
  the arithmetic: healing never rolls dodge (avoiding your own
  beneficial spell doesn't make sense the way avoiding an incoming
  attack does), and it caps at `maxHp` rather than having no upper
  bound the way damage has no lower one. Branching on a new
  `Talent::isHeal` flag in `tryUseTalent` rather than trying to make one
  function serve both cases.
- **Self-targeting needed a real resolution path it didn't have.**
  `TargetingMode::Self` existed already (Blink, Immolate) but was never
  actually read by `tryUseTalent`'s anchor-resolution logic -- Blink
  works because `EffectShape::Movement` bypasses targeting entirely, and
  Immolate's `AreaAroundSelf` searches for nearby *enemies* via
  `actorsWithinRadius`, which is the wrong tool for "heal yourself."
  Added a direct `Self` + `SingleTarget` case that resolves the target
  to the caster, rather than trying to force this through either
  existing path.
- **`isHeal` was added to `Talent` at the very end, after `damageType`
  -- not because it's logically related to placement there, but because
  every existing talent is built with positional aggregate
  initialization (see "Attribute-driven combat" above), and appending
  is the only insertion point that can't silently shift an existing
  value into the wrong field.**
- **Balanced deliberately, not just "some reasonable number."** Base 8 +
  the Spellblade's own Intelligence bonus (4) == 12 hp, about 40% of the
  30 maxHp pool -- meaningful without being a full heal from empty. A
  6-turn cooldown, the longest of any Spellblade talent, paces it
  deliberately: sustain this strong needs rationing, not spamming every
  fight. Verified live, precisely: cast from 20/30 hp, the log showed
  the heal correctly cap at 30/30 rather than overshoot to the
  arithmetic 32 -- confirmed in the actual running game, not just the
  unit test (`talent_test` separately confirms the same cap in
  isolation, including a dedicated near-full-hp case).

## Attribute-driven combat (decided Prompt 14, Phase 2)

- **A new `AttributeFormulas` module, pure functions only.** Same
  separation-of-concerns reasoning as `TalentEffects` being kept
  separate from `Talent`: `Stats` stays plain data (its own header
  comment has said so since Prompt 3), the formulas that interpret it
  live outside it. `physicalDamageBonus`/`magicDamageBonus`/
  `dodgeChance`/`manaBonusFromIntelligence` are all pure and fully
  unit-tested (`attribute_formulas_test`, 25 checks). `didDodge(chance,
  roll)` is deliberately split out as its own pure comparison so the
  *logic* of a dodge check is testable with exact inputs even though
  `rollDodge()` -- the one function that actually draws a random number
  -- isn't tested in isolation, same precedent as Chaser's `onHitChance`
  roll (Prompt 10), which was never isolated for testing either.
- **A floating-point exact-equality bug, caught and fixed in the test
  itself, not the formula.** `dodgeChance(20)` computes exactly the 30%
  cap via `10 * 0.03f`, but that computed value isn't guaranteed
  bit-exact to the literal `0.30f` the way `dodgeChance(30)` is (which
  gets clamped and returns the cap constant directly, not a computed
  value) -- `dodgeChance(20) == 0.30f` failed on first run despite the
  formula being correct. Fixed with a tolerance-based comparison for
  that one boundary case, leaving the clamped-past-cap cases on exact
  equality since those genuinely do return the literal constant.
- **`damageType` was moved to the very end of `Talent`, not placed
  naturally alongside `power`/`areaRadius`, after finding a real
  landmine.** Every talent in `SpellbladeTalents.cpp` is constructed via
  positional aggregate initialization -- the `/*name=*/`-style
  annotations are plain comments, not C++20 designated initializers, so
  values are matched to struct fields by position alone. Inserting a
  new field mid-struct would have silently shifted every value after it
  in any construction listing that many fields: specifically,
  `conditionalMultiplier` (an int, meant for the second-to-last field)
  would have landed in `conditionalHpFraction`'s old slot -- which
  compiles perfectly cleanly via an implicit int-to-float conversion,
  no warning, just quietly wrong data. Checked `MonsterAttackProfile`
  and `AIDecision` for the same risk before adding `damageType` to
  either; both were confirmed safe (every construction site uses named-
  member assignment or empty-brace default-init, never a positional
  value list), so `damageType` sits in a natural position in both of
  those.
- **Every existing tuned damage number was recalibrated, not left to
  drift.** Each talent's and monster attack's base `power` was reduced
  by exactly its new attribute bonus, so the *total* (base + bonus)
  reproduces the exact value Prompts 9-11 already tuned and tested --
  e.g. Quick Strike's base dropped from 6 to 4, but the Spellblade's
  strength (14) contributes +2, landing back on 6. This was verified
  directly, twice: `talent_test` uses an attacker with the Spellblade's
  real configured stats and asserts the *exact original* damage numbers
  still come out (not just "some plausible number"), and live play
  showed every logged combat number matching those same hand-computed
  values exactly. The alternative -- leaving old flat values in place
  and just adding attribute bonuses on top -- was rejected specifically
  because it would have silently made every fight easier or harder by
  however much the new bonuses added up to, without that being a
  deliberate difficulty decision.
- **Dodge blocks the on-hit status effect too, not just the raw
  damage.** A dodged Spider bite shouldn't still poison the target --
  avoiding the hit means avoiding what rode in on it. Implemented as a
  single `dodged` flag gating both the damage branch and the
  effect-application branch in `executeAIDecision`, rather than two
  independent checks that could drift out of sync with each other.
- **The attribute bonus is folded into `power` *before* Execution's
  conditional multiplier applies, not added after.** `(power + bonus) *
  multiplier`, not `power * multiplier + bonus`. An execute is meant to
  amplify the attacker's full output including their inherent strength,
  not just the talent's flat listed number -- confirmed with an exact
  hand-computed value in `talent_test`: (8 base + 2 strength) * 3 == 30,
  matching the original tuned 10 * 3 == 30 precisely, not the 26 the
  other order would have produced.
- **Monster attributes are genuinely differentiated per type, not
  reused defaults.** Every monster had strength=10/dexterity=10
  (`Stats`' plain defaults) regardless of type before this prompt --
  meaningless once dodge and damage bonuses actually depend on them.
  Chosen per-type to match each monster's established identity rather
  than arbitrarily: Ogre leans Strength-heavy/Dexterity-light (hits
  hard, 0% dodge -- a lumbering brute), Spider and Archer lean Dexterity
  (18%/24% dodge respectively, "hard to pin down"), Shaman and Bomber
  lean Intelligence (matching their already-magic-coded kits from
  Prompt 10), the boss balances both offense stats across its melee and
  blast phases. Goblin stays fully baseline on purpose -- the roster's
  plain, undifferentiated mob, deliberately unchanged by this prompt.
- **The player recalibrated to Strength 14 / Dexterity 10 / Intelligence
  18** -- the Str+Int hybrid corner of the attribute triangle (see
  ROADMAP.md, "Phase 2"), Dexterity left at baseline since evasion isn't
  this class's identity. `maxMana` is now computed from
  `manaBonusFromIntelligence()` (12 base + 8 from Intelligence) rather
  than hardcoded -- lands on exactly 20, the same value this class has
  had since Prompt 9, confirmed live via a screenshot showing "20/20" at
  game start, not just asserted.
- **No live-forced dodge observation.** Getting an actual dodge to occur
  during scripted `xdotool` play is probabilistic (an 18-24% chance per
  hit against the roster's most evasive monsters) and there's no way to
  manually select a specific target to attack (targeting auto-picks the
  nearest adjacent/visible enemy) -- an attempt to specifically farm a
  Spider encounter ended up hitting the adjacent Goblin instead every
  time. Rather than spend many more turns chasing a probabilistic
  confirmation, treated this the same as `onHitChance` (Prompt 10):
  the deterministic formula is exhaustively tested, the zero-chance
  case is proven to never dodge (a real integration proof, not just a
  formula-in-isolation one), and the dodge-branch code itself is simple
  enough to trust by inspection once both of those hold.

## Combat pacing: bump-turn-starvation and mana regen (player-reported, post-Prompt 13, confirmed on the person's machine)

Two issues reported directly from play, not from the drafted roadmap --
investigated and fixed with the same rigor as any other bug, including
constructing a direct test before touching any code.

- **"Monsters won't attack unless I attack first" -- confirmed as a real,
  reproducible bug, not a misunderstanding.** Diagnosed by loading a save
  with the player standing directly adjacent to two monsters at full hp
  (confirmed via `savegame.txt` inspection, not guesswork) and testing
  precisely what made them act: using a talent made both attack
  immediately, proving the AI/FOV/adjacency logic itself was correct.
  The actual defect was in `tryMovePlayer`: bumping into an
  actor-occupied tile was treated identically to bumping into a wall --
  a complete no-op, `processMonsterTurns()` never called, no turn
  consumed. Walked through `TurnScheduler`'s energy math by hand first
  (confirmed: with equal speeds, a full round-robin naturally emerges,
  so turn frequency itself isn't the problem) before concluding the bug
  was specifically in what counts as a "turn" at all. A monster that
  just moved into range needs its *own* next turn to notice it's now
  adjacent and attack -- its current decision was already computed
  using its pre-move position (`Chaser::decideAction`, Prompt 10) --
  and a player's natural instinct after walking up to a monster is
  often to press the same direction again, expecting *something* to
  happen. With no bump-to-attack in this game, that repeated input was
  being silently swallowed forever, so the monster's overdue turn never
  came. Fixed by giving `tryMovePlayer` two genuinely different
  outcomes for "can't move here": a wall remains a pure no-op (an input
  mistake, nothing gained by consuming a turn over it), but bumping into
  an actor now logs "You bump into X." and runs the same
  turn-processing as a successful move, without moving the player or
  dealing damage -- this still isn't a bump-to-attack (combat stays
  exclusively through talents, not a new mechanic scope wasn't asked
  for), it just stops silently discarding turns the player has clearly
  earned. `isOccupied` was refactored to be built on top of a new
  `actorAt()` helper (returns the actor, not just a bool) since the fix
  needs to know *who* was bumped, to name them. Verified live, twice:
  once reproducing the exact original scenario end-to-end (10 pure
  movement turns to reach the identical adjacent position, then one bump
  with no talent used at all -- both monsters attacked immediately), and
  again with clean single-bump isolation to confirm it's exactly one
  turn's worth of consequence, not a repeat-fire bug.
- **Mana regen: +2 per player turn, capped at max.** The player had no
  way to recover mana at all before this -- a fight that outlasted the
  starting pool left talents visibly off cooldown but permanently
  unaffordable for the rest of that encounter. Applied once per player
  turn inside `advanceTurnsUntilPlayerCanAct()`, alongside where status
  effects already tick -- the natural single place that already
  represents "a round has passed for the player," ticking even through
  stun-skips (a stunned player is still in the round, just unable to
  act). Verified live with exact arithmetic, not just "it went up": cast
  Blink (3 mana cost) then checked mana after every subsequent turn via
  the save file -- landed at 19/20 immediately after Blink itself
  (20 - 3 cost + 2 regen from that same turn's own end-of-turn
  processing == 19, matching exactly), then 20/20 after the next
  successful move (19 + 2 == 21, correctly capped to the 20 max), then
  held flat at 20/20 across further *blocked* moves (walls remaining a
  correct no-op, not a hidden extra regen source).

## Text rendering + HUD (decided Prompt 13, Phase 2)

- **DejaVu Sans Mono, sourced via apt, bundled with its license file.**
  Checked the license *before* bundling anything, not after: Bitstream
  Vera-derived, explicitly permits reproduction/distribution including
  within a larger software package (the only real restriction -- can't
  sell the font by itself -- doesn't apply to bundling it as a project
  asset). `assets/fonts/LICENSE-DejaVu.txt` ships alongside the font
  for that reason.
- **Confirmed SFML 3's text API by searching before writing any code,**
  same discipline as every other SFML-version-specific decision in this
  project (see `sf::Event`, Prompt 2). Two real breaking changes from
  SFML 2: `sf::Font::loadFromFile` was renamed `openFromFile`, and
  `sf::Text` lost its default constructor entirely -- it now requires a
  valid `sf::Font&` at construction, the same "can't hold a null
  resource pointer" reasoning that also removed `sf::Sprite`'s default
  constructor. Both confirmed correct on the first real build attempt.
- **One `log()` template method replaces all 23 scattered `std::cout`
  call sites in `Application.cpp`.** A variadic template
  (`template<typename... Args> void log(Args&&... args)`, C++17 fold
  expression to build the string) rather than converting each call site
  into a verbose `std::ostringstream` block -- call sites read almost
  exactly like the `std::cout <<` chains they replaced. `log()` does
  two things: prints to `std::cout` exactly as every direct call used
  to (every prior console-based verification technique in this project
  -- redirecting stdout, grepping for expected strings -- still works
  completely unchanged), and appends to a capped rolling buffer
  (`logMessages_`, a `std::deque`, capped at 6) that `render()` draws
  on-screen. Two messages were deliberately left as raw `std::cout`,
  not routed through `log()`: the dungeon-generation summary and the
  font-load warning. Both are one-time diagnostics that fire before or
  outside normal play (at launch/regenerate, and at startup
  respectively) -- putting them in the *combat* log would mean a stale,
  disconnected first line sitting in the on-screen log for the rest of
  the run.
- **A genuinely new verification technique for this prompt: screenshot
  the live window and look at it.** Every prior prompt's rendering work
  was verified by console output, hand-traced geometry, or trusting the
  draw calls -- reasonable for colored rectangles, insufficient for
  confirming *text actually renders correctly and looks right*.
  Installed ImageMagick's `import`, captured the real SFML window
  running under Xvfb via `xdotool`-scripted play, and inspected the
  images directly. This is what caught a real, if minor, issue no
  amount of code review would have: the talent-list text initially had
  no visual separation from the dungeon tiles it happened to render
  over -- legible, but genuinely unfinished-looking. Fixed with
  semi-transparent backing panels behind the talent list and combat
  log, then re-screenshotted specifically to confirm the fix looked
  better, not just assumed it would from reading the diff.
- **Talent list and combat log both use the same drawText() helper**
  as the boss's new name label -- a thin, direct wrapper around
  `sf::Text` (setString/setCharacterSize/setFillColor/setPosition/draw),
  deliberately not a new abstraction layer. The boss label wasn't
  separately screenshotted -- it uses the identical, already-proven
  code path as everything that *was* screenshotted, so re-verifying it
  live would only prove the same mechanism twice.

## Save/load (decided Prompt 12)

- **A hand-rolled, human-readable text format, not JSON or binary.**
  Consistent with every other format decision in this project (talent
  data, monster data, the ASCII test maps): this code is the only reader
  or writer of its own format, so a general-purpose serialization
  library would be new build complexity for no real benefit. Uses plain
  `operator>>` extraction throughout, including for the map and
  fog-of-war grids -- each row is a contiguous, whitespace-free string of
  characters, so `>>` reads a whole row as one token without needing
  manual line-splitting.
- **`SaveGameState` and the `saveGame`/`loadGame` free functions live in
  their own module (`core/SaveGame.hpp/.cpp`), entirely independent of
  `Application`/SFML.** This is what makes `savegame_test` possible at
  all -- constructing a `SaveGameState` by hand, round-tripping it, and
  checking every field, without needing a window. `Application` only
  gathers state into the struct and applies a loaded one back; it doesn't
  know anything about the file format itself.
- **Deliberately does not save `TurnScheduler`'s exact energy levels or
  any `AIBehavior`-internal state** (the boss's one-time enrage trigger,
  phase-announcement tracking). Both are designed to be self-correcting:
  turn order re-settles within a few turns regardless of exact energy
  values, and `BossBehavior`'s phase is re-derived from hp fraction on
  every single `decideAction()` call, never trusted as stored state. At
  worst, a loaded boss already past a phase threshold re-announces that
  phase and re-applies its one-time buff once more -- harmless, since
  `StatusEffects::apply()` refreshes rather than stacks. Serializing
  either would be real complexity for a difference nobody would notice
  in play.
- **`MonsterType` moved out of `MonsterFactory.hpp` into its own header.**
  `Monster` needed to remember its own type (so a loaded monster can be
  reconstructed via `MonsterFactory::createMonster()` rather than
  needing its own parallel deserialization path for every `AIBehavior`
  subclass), but `MonsterFactory.hpp` already includes `Monster.hpp` --
  defining the enum there would have made `Monster.hpp` need it back,
  a circular include. A monster's hp/maxHp/status effects are saved and
  restored as explicit values *after* reconstruction, not re-derived from
  `MonsterFactory`'s current defaults -- so a later rebalance of, say,
  Goblin's max hp doesn't retroactively change what an old save file
  means.
- **Loading does *not* run the same
  `processMonsterTurns()`/`advanceTurnsUntilPlayerCanAct()` step
  `regenerateLevel()` runs after setup.** Initially it did, copying that
  pattern directly -- caught via live testing that this silently applied
  an extra, unintended status-effect tick to an already-poisoned loaded
  player (a freshly *generated* player never has any status effects yet,
  so the same step is harmless there). Fixed by having `loadGame()` just
  resolve whose turn it is and stop -- any monster whose turn is
  technically still pending gets caught up naturally the moment the
  player next moves or casts, the same mechanism (`tryMovePlayer`/
  `tryUseTalent` already call `processMonsterTurns()` themselves) that
  handles it in ordinary play, not a special case invented for loading.
- **Saves to a single fixed relative path** (`savegame.txt`, wherever the
  executable is launched from) -- same reasoning as avoiding `data/` file
  loading elsewhere in this project: resolving the executable's own
  directory needs platform-specific APIs this project has deliberately
  avoided needing so far. One save slot, not a save-file browser/manager
  -- not asked for, and would be real scope beyond "save/load for game
  state."
- **Verified with real rigor, including a bug caught by live testing that
  static reasoning alone hadn't surfaced:** `savegame_test`'s 15 checks
  confirm exact round-trip fidelity for every field in isolation. Live
  verification went further -- saved a real in-progress fight (a landed
  Power Strike, real hp loss, an active poison effect) in one process,
  loaded it in a completely separate fresh process, and confirmed hp and
  cooldown state matched exactly, catching the extra-tick bug in the
  process rather than after the fact.

## Open items / things to revisit later

- Save/load has a single fixed slot (`savegame.txt`), not multiple slots
  or a save browser -- not asked for; real scope beyond "save/load for
  game state" if ever wanted.
- No autosave -- save/load is entirely manual (F5/F9). Reasonable for a
  vertical slice; would matter more with real playtime at stake.
- Save files aren't versioned beyond a single format-version tag that
  currently only guards against loading a completely incompatible
  format (mismatched version number is rejected outright, not migrated).
  No real concern yet with one format version ever having existed.

- **Font rendering now exists (Prompt 13) -- sprite/tile art still
  doesn't.** Tiles, monsters, and the player are still flat colored
  rectangles; text (names, damage, status effects, talent list, combat
  log) now renders for real, which was the more limiting half of this
  gap. Real sprite/tile-atlas art remains a from-scratch pipeline
  (texture loading + atlas slicing) with no immediate need driving it
  -- revisit if/when visual fidelity, not readability, becomes the
  constraint.
- No camera/viewport/scrolling. Not urgent yet (the generated dungeon
  still fits the window), but this is now the clear next sharp edge, not
  a vague someday -- a real dungeon will exceed one screen eventually.
- Talent and monster content are both real data now (`SpellbladeTalents`,
  `MonsterFactory`), but both are still C++ tables, not loaded from
  `data/` -- revisit once real content needs authoring outside code
  (same reasoning as the ASCII test maps).
- Sight radius is a hardcoded constant, not a per-actor stat -- revisit
  once anything (talents, equipment) might plausibly modify it.
- No general combat formula system (armor, resistances, damage types) --
  every attack/talent/ability still carries its own flat damage as data.
  Two-way combat itself now exists (Prompt 10); a real damage-resolution
  formula is a different, larger thing, only worth building if the
  numbers-as-data approach stops feeling sufficient.
- No speed-altering status effect (Haste) -- deliberately avoided in
  Prompt 10 specifically to not touch `TurnScheduler`'s effective-speed
  calculation. Empowered (damage buff) covers "support/buffer" instead.
  Revisit if a real reason to modify turn frequency shows up.
- No targeting-cursor UI. Player talent targeting uses simple heuristics
  (nearest visible enemy for ranged, first adjacent for melee) rather
  than letting the person choose among multiple valid targets. Fine for
  the current roster size; would matter more with denser encounters.
- No faction/friendly-fire system -- Bomber's AoE only ever targets the
  player, never other monsters, even ones standing in the blast area.
- No game-over screen or restart flow -- player death (and victory
  against the boss) currently just leave the window as-is (death closes
  it, victory doesn't). Building a proper screen/flow is explicitly
  Prompt 12 territory ("final polish").
- No indication in the game window of *which* phase the boss is in
  beyond the console announcement and its health bar's fill level -- no
  visual phase-change effect (a flash, a color shift). Minor, but a
  natural target once any rendering polish pass happens.
- Boss room placement isn't guaranteed (~97-98% of seeds, not 100%) --
  pressing R again resolves it if a given layout happens to lack one.
  Could be made fully guaranteed with a smarter placement strategy
  (reserving a spatial region before placing regular rooms) if ever
  worth the added complexity.
- `Application` owns `map_`/`player_`/`monsters_`/`boss_`/`scheduler_`/
  `exploredMap_` directly, and resolves targeting/combat/occupancy for
  all of them. The monster-collection part of the long-flagged "game
  state" refactor happened in Prompt 10 (single `goblin_` →
  `vector<unique_ptr<Monster>>`), and this prompt added `boss_` as a
  tracked pointer into that same collection rather than pulling anything
  further out -- the fuller extraction (window vs. simulation state as
  genuinely separate classes) still hasn't happened, still not forced by
  anything built so far.
- Pathfinding (`findPath`) is still not actor-aware -- it computes routes
  based on terrain only. Combined with the new occupancy check at
  move-commit time, this means a monster (or the player's Blink) can
  occasionally have its computed next step blocked by another actor that
  moved into it since the path was computed, resulting in a "wasted"
  turn that self-resolves the next turn once that actor moves. A known,
  accepted minor rough edge, not a crash or incorrect state -- making
  `findPath` itself occupancy-aware would be a larger, separate change to
  already well-tested Prompt 7 code.
- `SFML_BUILD_AUDIO` / `SFML_BUILD_NETWORK` are off -- flip back on when
  sound is wanted.
