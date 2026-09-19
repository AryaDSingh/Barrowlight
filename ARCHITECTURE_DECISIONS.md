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

## Open items / things to revisit later

- No font or sprite/tile-atlas rendering yet -- flat colored rectangles
  stand in until real art (or a chosen font) exists.
- Level data is still a source-literal ASCII array, not loaded from
  `data/` -- revisit once real levels need authoring outside code.
- No camera/viewport/scrolling -- the test map is small enough to fit
  entirely on screen. Will matter once procedural levels (Prompt 8) are
  bigger than one screen.
- Sight radius is a hardcoded constant, not a per-actor stat -- revisit
  once anything (talents, equipment) might plausibly modify it.

- `SFML_BUILD_AUDIO` / `SFML_BUILD_NETWORK` are off — flip back on when
  sound is wanted.
- No tile-rendering abstraction yet — `Application` currently just clears
  and displays. Tile rendering is a future prompt.
- `entities/`, `ai/`, `world/` folders intentionally don't exist yet.
- The person's home PC lacked `winget`/App Installer for unclear reasons
  (not a locked-down machine) — CMake was installed via portable ZIP
  instead of an installer. Not a project decision, just an environment
  note in case install friction resurfaces.
