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

## Open items / things to revisit later

- No font or sprite/tile-atlas rendering yet -- flat colored rectangles
  and simple HUD bars stand in until real art (or a chosen font) exists.
  Now genuinely limiting (talent names/tooltips/cooldowns are
  console-only, not shown in the game window) rather than just cosmetic.
- No camera/viewport/scrolling. Not urgent yet (the generated dungeon
  still fits the window), but this is now the clear next sharp edge, not
  a vague someday -- a real dungeon will exceed one screen eventually.
- Talent content is now real data (`SpellbladeTalents`'s table), but
  still a C++ table, not loaded from `data/` -- revisit once real content
  needs authoring outside code (same reasoning as the ASCII test maps).
  Monster stat blocks are still not data at all -- Prompt 10.
- Sight radius is a hardcoded constant, not a per-actor stat -- revisit
  once anything (talents, equipment) might plausibly modify it.
- No general combat formula system (armor, resistances, damage types) --
  talents carry their own flat damage as data. `AIBehavior` still only
  decides movement; enemies still can't attack. Revisit once Prompt 10
  builds real two-way combat.
- No duration-based buffs/debuffs -- all 8 Spellblade talents are instant
  effects. `StatusEffects` is still a stub; Prompt 10's job.
- `Application` owns `map_`/`player_`/`goblin_`/`scheduler_`/
  `exploredMap_` directly, and now resolves talent targeting too.
  Flagged repeatedly as approaching the point where a dedicated "game
  state" concept earns its keep -- still holding as of Prompt 9, but a
  second monster (Prompt 10) is a reasonable line to finally do it.
- `SFML_BUILD_AUDIO` / `SFML_BUILD_NETWORK` are off -- flip back on when
  sound is wanted.
