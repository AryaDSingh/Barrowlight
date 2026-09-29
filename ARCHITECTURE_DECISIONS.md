# Architecture Decisions — Roguelike Engine

Living record of what's been decided on this project, why, and what to check
before contradicting it. Claude doesn't retain memory between separate
conversations — paste this whole file at the start of a fresh session to
restore context, and ask for it to be updated as new decisions get made.

Last updated: 2026-09-25 (Prompts 33-34). Earlier sections retain their historical
context; later decisions supersede earlier rules where explicitly stated.

---

## Loot and support runes (Prompts 33-34)

**Local follow-up verification:** Removed twelve unused legacy class files
(Marauder/Fighter/Archer/Sorcerer talent pairs and their tests), after reference
checks confirmed no active dependencies. Full active-target rebuild succeeds;
twenty console targets, 28 targeting integration checks and 35 reward checks
pass locally. Reward tests exercise seeded loot/affix constraints, chest and
rune ownership, actual modified casts, cooldown/save persistence, ordinary
drop caps and summon reward exclusion. Inspected rune/inventory screenshots.
The Widen display now labels base/current/with-selected-rune values, so an
already-applied bonus cannot be mistaken for a failed modifier. This update
supersedes the original build-only verification note below; balance playthrough
remains outstanding. The test initially assumed 1 mana regeneration per turn;
the implementation's existing value is 2, and the expectation was corrected.

### Loot identities, random stream and budget

- `LootGenerator` owns an explicitly serialized nonzero xorshift64* state.
  Only committed enemy/chest/boss rewards draw from it. Known seed and reward
  sequence reproduce rolls without depending on the combat RNG or previews.
  Item instances store actual affix IDs/values and roll tier; loading never
  rerolls them. Instance IDs are shared by equipment and rune copies.
- Normal/magic/rare means zero/one/two affixes. All affixes must support the
  item's slot; two affixes may not modify the same stat. Tier is
  `clamp((floor-1)/3 + quality, 0, 5)`: quality 0 for ordinary Base/chests,
  1 for Elite, 2 for Nightmare/bosses. The following base ranges increase by
  `tier * step`; values are inclusive. No physical mitigation was added.

| Affix ID | Stat | Slots | Base range | Step |
|---|---|---|---|---|
| might | Strength | weapon/charm | 2-4 | 1 |
| agility | Dexterity | all | 1-3 | 1 |
| knowledge | Intelligence | weapon/charm | 2-4 | 1 |
| vitality | maximum HP | armour/charm | 3-6 | 2 |
| reservoir | maximum mana | armour/charm | 3-6 | 2 |
| brawn | Strength | armour | 1-2 | 1 |
| insight | Intelligence | armour | 1-2 | 1 |
| vigor | maximum HP | weapon | 2-3 | 2 |

- Base rarity weights: rare `15+5*tier` percent, magic up to the 75th
  percentile, normal 25%. Chests promote normal to magic; bosses force rare.
  Floor one retains its three fixed starter items. Later floors replace the
  old entrance supplies with random rewards. One reachable chest is placed
  toward each floor's far side by BFS, without drawing loot RNG. G opens it
  once for one turn, grants to bag, and saves the claim flag.
- Ordinary drops: 35/50/65 percent for Base/Elite/Nightmare, maximum two per
  floor, with the cap persisted. Both bosses grant two rare items directly
  to the bag (including final-run rewards before victory). With six ordinary
  monsters per floor, expect roughly 16-20 ordinary drops plus 10 chests,
  four boss items and three starters: about 33-37 equipment candidates.
  Target 6-10 meaningful equipment changes per run, not a measured result.
- Summons are explicitly reward-ineligible and have zero XP. This flag
  persists alongside exact monster tier; loading no longer scales existing
  enemies to a new tier based on the player's latest level. Death processing
  claims each monster once before issuing rewards. R remains a development
  regeneration/healing shortcut; reward budgets assume ordinary door travel.

### Support rules and progression

- Talent definitions have literal stable IDs. Typed compatibility flags
  (melee, projectile, area, movement, damaging, pure movement) are derived
  from definition geometry once when entering a TalentSet. Compatibility
  checks use these flags and reject invalid combinations with explanations.
- Each talent supports one owned rune instance. TalentSet keeps immutable
  base definitions and creates temporary effective talents. Chain is limited
  to damaging single-target projectiles: primary hit unchanged, nearest
  reachable visible secondary within radius 3 at 50% damage. Coordinate ties
  are deterministic. Bounce paths cannot cross terrain, blocked corners,
  unseen cells or intervening actors. Primary/secondary independently roll
  dodge/crit; the secondary cannot be the primary or caster. No recursive chain.
- Widen adds one radius and `ceil(baseMana/2)` mana (minimum one). Venom deals
  80% direct damage and applies Poison(3 turns, magnitude 2) after a successful
  hit if the target survives. Existing on-hit talents are incompatible; poison
  on the victim refreshes through the existing StatusEffects rules rather
  than stacking. Swift Passage adds two movement tiles and two cooldown turns
  to pure movement only. Effective damage always retains base cooldown scaling.
- User-approved progression addition: Mage learns Blink at level 2, with
  distance 3, mana 4 and cooldown 4. This also joins the existing Mage hybrid
  pool. Seven hybrid options are supported by the choice screen and controls;
  six picks no longer exhaust every Mage option. Prompt 35's larger
  specialization redesign remains unimplemented.
- V manages runes: arrows select talent/rune, Enter attaches/swaps and U
  removes. Valid changes cost one turn and close the panel. They do not reset
  existing cooldown counters, which then advance normally with the paid turn.
  Displaced runes return to the rune bag; moving an attached rune clears its
  old attachment. Invalid/no-op changes and browsing are free.
- First-floor chest grants a deferred, free choice of one rune (V, keys 1-4)
  after paying the chest turn. Choice availability persists. Even-floor
  chests and both bosses each grant one random rune: eight total opportunities
  across a full run, including the initial choice. Rune rolls share the
  dedicated loot stream, never combat RNG.

### Persistence and verification boundary

Save format 8 supersedes 6 (inventory) and 7 (loot); old saves are rejected.
Learned talent IDs and cooldowns are saved together in learned order,
including borrowed talents. Enemy talent cooldowns also use IDs. Rune
ownership and attachments persist by instance/talent IDs. Restore validates
item affixes, IDs/slots, chest/drop state, rune attachment compatibility and
talent catalogs before committing the live game replacement. Current pools
are restored against the fully reconstructed equipment bonuses.

The Debug game target builds. Existing save/hybrid/targeting fixtures were
adapted to the new schema and Mage kit, but no tests were added or run and
no interactive playthrough was performed. Reward pacing, visual layout and
rune balance still need gameplay verification. Scheduler energies and AI
internal phase/summon counters remain outside serialization, as previously;
summons remain reward-ineligible even if an AI counter restarts after loading.

## Inventory and equipment (Prompt 32)

- **Ownership:** Application owns ground items; Inventory owns the bag and
  weapon/armour/charm slots through unique pointers. Equipping into an occupied
  slot swaps the old item into the selected bag row. Removing gear appends it
  to the bag. Nine C++ definitions have stable string IDs; every generated
  copy has a distinct monotonic 64-bit instance ID within the run.
- **Stats:** Player owns permanent `baseStats_`; Actor's existing `stats()` is
  the effective combat snapshot, including live HP/mana. Level growth and
  allocated attributes modify base stats, then refresh the snapshot. Equipment
  operations go through Player to refresh it. Combat still mutates current
  HP/mana directly. Base HP/mana current fields are not authoritative; saves
  combine base maxima/attributes with live current pools. Equipment attributes
  do not also grant pool bonuses; item definitions state those separately.
- **Pool policy:** recalculation retains current HP/mana, clamped to the new
  maximum. Equipping capacity never refills it, and removing/re-equipping cannot
  recover discarded points. Existing level/floor healing remains unchanged.
  This policy is simple and prevents swap healing, but taking off capacity
  while full can lose HP/mana; the comparison explicitly previews this cost.
- **Turn contract (chosen before scheduler integration):** browsing/selecting/
  comparing/cancelling and invalid actions are free. Each successful pickup,
  equip or removal costs one player turn, using normal cooldown/status ticks,
  mana regeneration and enemy responses. The inventory closes on commitment.
  This preserves tactical costs and visibility of enemy actions; equipping a
  whole outfit requires reopening the bag between actions.
- **UI/content:** B opens a keyboard inventory; G takes one item at the player's
  feet. Equipment rows precede a paginated bag. The comparison shows base, now,
  after and change, with pool previews before turn effects. Nine items grant
  only existing attributes and HP/mana. Ordinary floors place three rewards
  near the entrance through a deterministic walkable BFS; floor one offers
  items suited to the starting class, with variants rotating on later floors.
  Boss floors do not spawn these supplies. Placement consumes no RNG. R remains
  an explicit development regeneration shortcut, outside reward balancing.
- **Persistence:** version 6 rejects older files, as before. Save base stats
  plus current pools, unspent points, item definition/instance IDs, ownership
  locations, ground positions and next ID. Loading validates known definitions,
  unique IDs, compatible unique equipment slots, ground walkability and bounded
  counts; equip all saved items before deriving stats once. Never apply saved
  effective bonuses as new base stats. Floor transitions retain owned items
  and the ID counter; abandoned ground items are removed with the old map.
- **Implementation boundary:** item definitions and ownership remain small
  header-only components; ApplicationInventory.cpp handles SFML/input/turn
  integration. No mitigation formula, procedural affixes, drops or runes yet.
  The Debug game target builds. No automated tests or interactive playthrough
  were run for this prompt.

## Manual targeting and enemy inspection (Prompt 31)

- **One pure resolver for preview and execution.** `world/TalentTargeting`
  receives the map, player visibility, caster, enemies, talent, and cursor.
  It returns a path, affected tiles/actors, movement endpoint, validity, and
  a reason. It never mutates state or rolls RNG. `Application::tryUseTalent`
  validates readiness/resources and recomputes that same result before the
  commit point. `estimateTalentDamage` supplies arithmetic shared with real
  hits; previewing cannot advance combat randomness.
- **Selection is transient UI state.** Store talent indexes and tile
  coordinates, never long-lived actor pointers. A committed action clears
  selection before combat; load/regeneration clears it before replacing the
  roster. Save files do not store hover, aim, or page state. A selected
  talent is copied for execution because kill XP can append talents and
  invalidate a reference into the talent vector during an area attack.
- **Mouse and keyboard have equal access.** Number keys/sidebar clicks
  choose a talent; mouse/arrows aim; Tab cycles visible valid targets;
  Enter/map click confirms; Escape/right-click cancels. I enables keyboard
  inspection. Pages of nine talents cover the complete hybrid kit. Self
  buffs/heals remain immediate. Hover details include the player's own
  ability costs, cooldowns, and targeting rules.
- **Player projectiles explicitly declare their geometry.** A trailing
  `Talent::projectile` field marks Quick Shot, Volley, Piercing Shot, Arcane
  Bolt, Ember Bolt, and Fireball. Rays stop at the first enemy or terrain
  and cannot cut blocked diagonal corners. Piercing Shot's name still
  describes its existing crit-focused mechanic, not actor penetration.
  Direct spells keep the visible-target rule. Enemy AI still uses its
  existing targeting; extending trajectory rules to it is separate work.
- **Visible information bounds player targeting.** Player splash includes
  currently visible walkable cells within the existing circular radius;
  it does not make a new line-of-effect check from the impact. Hidden
  enemies cannot leak through area highlighting, target counts, or damage
  estimates. This deliberately changes the previous unrestricted radius
  search. Aimed movement stops before blocked, occupied, or unseen cells.
- **Basic inspection is free; deeper information can be earned later.**
  `MonsterInspection` exposes visible living enemies' HP, attributes, speed,
  active effects, behavior, and abilities. Basic attack descriptions cover
  monsters whose attacks are AI profiles rather than TalentSet entries;
  actual talent descriptions come from their definitions. Live cooldowns
  require explicit `InspectionAccess::revealCooldowns` and use enemy-turn
  units. This update adds the access boundary, not a new perception talent.
  Access never bypasses field of view. Static behavior summaries will need
  updating alongside future AI changes.
- **A separate sidebar trades some map width for reliable readability.**
  Shared `PlayLayout` constants reserve the right 384 pixels for talent and
  inspection panels. The 896x504 map uses 28-pixel tiles, fitting the complete
  radius-8 FOV. Input converts window pixels through the SFML view and
  camera offset; HUD clicks cannot select the tile underneath them.
- **Verification includes real input/cast integration.** `targeting_test`
  is SFML-independent; `application_targeting_test` uses a hidden window,
  controlled scenes, and real Application handlers. It writes snapshots
  and a private save under the ignored build directory. Twenty-one test
  executables passed (nineteen existing plus two new). Interactive play
  feedback remains necessary for readability and combat feel.


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

## Sound effects (decided Prompt 25)

- **Sound effects are synthesized programmatically, not sourced
  externally -- a direct consequence of a real environment constraint,
  not a stylistic preference stated first and justified after.** This
  environment has no network access to royalty-free sound libraries.
  Generating simple tones and noise bursts with numpy sidesteps that
  limitation entirely, and as a side effect avoids any licensing
  question outright -- there's no external asset to have gotten the
  license wrong on.
- **`SFML_USE_SYSTEM_DEPS` is forced `OFF` explicitly, not left at
  SFML's own platform-dependent default.** SFML's audio module requires
  Vorbis/OGG to build; on Linux, SFML's own default looks for these as
  system libraries rather than fetching its own, which would have
  quietly broken this project's "one cmake command, no manual
  dependency installation" story (true since Prompt 2) for any Linux
  user, while happening to work on Windows/macOS where the default
  points the other way. Forcing it keeps behavior identical across
  platforms instead of depending on which platform's default happens to
  be convenient.
- **That fix was verified by actually removing the system libraries,
  not by reading what the flag does.** apt-installed the missing
  libraries first (reproducing the exact failure a Linux user building
  this project would hit), then uninstalled every one of them and
  confirmed a full clean rebuild still succeeds end to end -- SFML
  fetches and builds Vorbis/FLAC/Ogg from source itself, the same way
  it already fetches itself. The claim "this doesn't need system audio
  libraries" is a measurement here, not an assumption carried over from
  reading SFML's CMake logic.
- **`SoundManager` is the only class besides `Application` that touches
  `SFML::Audio` directly, deliberately mirroring the existing
  `SFML::Graphics`/`Window` boundary this project has kept since Prompt
  0.** One more SFML module, the same one-class boundary, rather than
  letting audio calls spread into `TalentEffects` or other logic files
  that have stayed engine-agnostic until now.
- **One `sf::Sound` per effect, not a pool for overlapping instances.**
  A real-time action game would need to play the same effect multiple
  times simultaneously; this is turn-based, so sound events are
  naturally spaced out enough that re-triggering a single instance
  (restarting it if still playing) is entirely adequate. Building a
  pool would have been solving a problem this project doesn't actually
  have.
- **Sound is designed, not just documented, to be a presentation
  detail the game never hard-depends on -- and this was confirmed
  directly against a real headless environment, not assumed from
  reading miniaudio's documentation.** This sandbox has no audio device
  at all. Tested actual playback against that directly: miniaudio (the
  library SFML's audio module is built on) logs to stderr and
  gracefully no-ops rather than throwing or crashing. Verified live
  through real gameplay covering every sound trigger path with the
  audio subsystem completely unavailable -- the exact scenario this
  environment presents, not a hypothetical "what if there's no sound
  card" caveat.
- **`loadFromFile()`'s `[[nodiscard]]` return value is explicitly
  discarded via `std::ignore`, not left as a silent compiler warning.**
  A failed load leaves that buffer empty, and playing an empty buffer
  is already a harmless no-op by design -- there's genuinely nothing to
  react to on failure beyond not crashing, which already holds
  regardless. Acknowledging that explicitly reads as a deliberate
  choice to a future reader instead of looking like an overlooked
  warning.

## FighterTalents/SorcererTalents renamed (decided Prompt 30)

- **Test files were renamed too, not just the two source files the
  request named -- following the Prompt 19 precedent rather than
  interpreting the request narrowly.** Prompt 19's own class rename
  renamed its talent files *and* its test files together, as one
  consistent unit; doing only the two headers/implementations named in
  this request and leaving `fighter_test.cpp`/`sorcerer_test.cpp`
  stale would have reintroduced exactly the inconsistency this prompt
  existed to close.
- **A rename is exactly the kind of change that looks obviously correct
  and can still be silently broken -- worth verifying by actually
  rebuilding, not by re-reading the diff.** Moving `FighterTalents.hpp`
  to `WarriorTalents.hpp` doesn't touch the `#include` line *inside*
  that same file referencing its own old name -- an easy mistake
  precisely because the file's own content doesn't visibly change
  when it's renamed. Caught immediately by attempting a real build
  right after the rename, not by inspecting the renamed files by eye.
- **A single grep pass was deliberately treated as insufficient, and a
  second, broader one was run before calling this done.** The first
  search (for `fighterTalents`, plural) missed
  `fighterTalentUnlockedAtLevel` (singular "Talent" plus a different
  suffix) -- not a fuzzy variant the same pattern could have caught,
  a genuinely different substring. A full clean rebuild surfacing the
  resulting compile errors is what actually caught it, which is itself
  the argument for treating "the grep came back clean" as a checkpoint
  to verify against a real build, not as sufficient evidence on its own.
- **Comments describing a class's own naming history were extended,
  not overwritten -- but comments only incidentally referencing an old
  file or function name were updated to the current one.** These are
  different situations: `WarriorTalents.hpp`'s own "originally
  Marauder, renamed to Fighter, then Warrior" lineage is a comment
  *about* the rename history itself, so it was extended to include this
  latest step. A stray mention like "see fighter_test.cpp's header
  comment" is just a cross-reference that happens to name a file --
  updating it to `warrior_test.cpp` doesn't lose any historical
  information, it just stops pointing at a file that no longer exists.
- **A genuinely unrelated staleness was found and fixed while already
  in the area, rather than left for a later pass to rediscover.**
  `MageTalents.cpp` still described the pre-Prompt-26 `damageType`
  field (removed during the attribute-system redesign, replaced by
  `scalingStat`) in a comment explaining a real bug that was caught
  live at the time. The comment was about this rename's own file, found
  during this rename's own verification search -- fixing it here, while
  the file is already open and the context is already loaded, cost
  far less than leaving it as a landmine for whoever next has reason to
  read that comment closely.

## The Lich (decided Prompt 29)

- **`AIActionType::Summon` follows the existing "AIBehavior decides,
  Application executes" split exactly, rather than carving out a
  special case for it.** LichBehavior only returns an `AIDecision`
  naming a position and a `MonsterType`/`MonsterTier` to summon;
  `Application::executeAIDecision()` is the only place that actually
  constructs the Monster, the same separation `Attack`, `UseAbility`,
  and every other action type already maintains. The alternative --
  letting an `AIBehavior` reach into `Application`'s own `monsters_`
  container directly -- would have broken that boundary for exactly
  one behavior, the first crack in a pattern this project has held
  consistently since Prompt 0.
- **Mid-turn `monsters_.push_back()` was confirmed safe by reasoning
  about the actual container type, not assumed.** `monsters_` is
  `vector<unique_ptr<Monster>>`; a `push_back` can reallocate the
  vector's own internal array, but the `Monster` objects it points to
  are heap-allocated and never move as a result. `processMonsterTurns()`
  only ever holds a single `Actor*` (`currentActor_`), never an
  iterator into `monsters_` itself, so nothing currently running during
  turn processing can be invalidated by a summon happening mid-loop.
  This is the kind of thing worth actually verifying against the real
  container type before relying on it, not just assuming "adding to a
  vector while iterating" is fine because it happened not to crash in a
  quick test.
- **No phase structure, unlike BossBehavior -- a deliberate omission,
  not a simpler first pass left unfinished.** BossBehavior's three
  phases exist because its escalation has to come from somewhere
  static (HP thresholds) -- it has no other mechanism for the fight to
  intensify. The Lich already has one: more attackers alive over time.
  Stacking an HP-threshold phase system on top would have been a
  second, redundant escalation axis rather than a complementary one.
- **The summon cap counts attempts made, not skeletons currently
  alive.** Tracking "how many are alive right now" would have let a
  sufficiently aggressive player farm the cap indefinitely by killing
  skeletons quickly and forcing more summons -- counting usage instead
  means the Lich has a genuinely finite number of reinforcements across
  the whole fight, regardless of how quickly each one is dealt with.
- **A blocked summon attempt (no open adjacent tile) does not consume
  the cap, and this was specifically tested, not just implemented and
  assumed correct.** The alternative -- charging the attempt to the cap
  regardless of whether it actually happened -- would let a player
  "waste" the Lich's summons for free by clever positioning, an
  exploit-shaped edge case worth closing deliberately rather than
  leaving to chance. `lich_test.cpp` verifies this directly: a fully
  boxed-in Lich falls back to a bolt, and a follow-up call with one
  tile freed still succeeds at summoning, confirming the earlier
  blocked attempt truly didn't count against the limit.
- **A new dedicated test file, matching `boss_test.cpp`'s own
  standard, rather than relying on live testing alone.** The summon
  mechanic has enough genuinely new logic -- cooldown gating, a cap
  held across multiple separately-ready cooldowns, and the blocked-
  tile edge case above -- that it warranted the same level of
  hand-computed, automated coverage every other AIBehavior in this
  project already has. Caught a real mistake in the test itself on
  first run (a target placed beyond the Lich's sight radius rather
  than just beyond attack range, producing an unexpected `Wait`) --
  exactly the value of writing the test rather than trusting the
  design review alone.
- **Live verification specifically targeted the more complex case the
  Prompt 28 victory-sequencing fix was built for, not just the simple
  one.** Defeating the Lich from level 5 crossed two level-up
  thresholds in one grant (4 attribute points, not 2) -- confirming
  every point and the resulting talent unlock still resolved correctly
  before Victory appeared is a meaningfully stronger check than
  confirming the single-threshold case alone would have been.

## Multi-floor dungeon progression (decided Prompt 28)

- **Staged deliberately: the full 10-floor structural skeleton first,
  the Lich as clearly separate later work.** The request named a
  concrete, specific boss ("a lich that spawns skeleton minions") --
  building that alongside floor-tracking, doors, and victory-gating
  in one pass would have meant a genuinely new AI mechanic (nothing in
  this engine lets a monster spawn others mid-fight) competing for
  attention with foundational plumbing that needed to be verified
  correct first. Floor 10 reuses the existing Goblin Warlord as an
  explicit placeholder so the entire 1-10 structure -- including the
  victory gating, which is exactly the kind of thing worth getting
  right before building content on top of it -- could be built and
  live-verified end to end before any Lich-specific work begins.
- **The door needs no "is it unlocked yet" state of its own -- a
  deliberate simplification enabled by an existing game rule, not a
  missed edge case.** On a boss floor, the door tile is placed exactly
  where the boss stands. The boss already blocks that tile like any
  other actor while alive (this game has no walking through monsters);
  once it's dead, the tile is simply walkable. The door "activates"
  as a side effect of a rule that already existed, rather than
  needing new tracking invented for it.
- **The original "boss in at least room 5" request generalized into a
  system-level protection, not a one-off numeric check.** The
  underlying goal was real exploration time before a floor's finale,
  whatever that finale is. Extending the existing boss-room shortcut
  protection (Prompt 21) to the last regular room too -- since that
  room now hosts the door on every non-boss floor -- solves this for
  every floor uniformly, rather than special-casing floor 5
  specifically and leaving every other floor's door open to a
  shortcut bypass.
- **A real bug in the victory sequencing was found by live-testing a
  scenario that combines two systems, not by reviewing either system
  alone.** The initial implementation set the GameOver transition
  unconditionally the instant the final-floor boss died. But
  `grantXpAndAnnounce()` can itself leave the game paused mid-choice
  (AttributeAllocation or AbilityChoice) if that same kill's XP crosses
  a level-up threshold -- overwriting `mode_` unconditionally would
  have silently discarded an attribute point or talent choice the
  player had just earned, without ever showing the screen for it.
  Neither the leveling system nor the victory system was wrong in
  isolation; the bug only existed at their intersection, which is
  exactly why it surfaced during live testing of a kill that crossed a
  level-up threshold rather than during either system's own test
  suite.
- **Fixed by extending the existing resumable state machine, not by
  adding a second, competing pause mechanism.** `pendingFinalVictory_`
  only records that a victory is waiting; the actual transition happens
  inside `resumeLevelUpSequence()`, at the exact point that function
  already represents "every pending choice from this XP grant has been
  resolved." This reuses the same machinery Prompts 23/24 built for
  sequencing attribute allocation against talent unlocks and hybrid
  choices, rather than introducing a parallel way to ask "is anything
  still pending" that could drift out of sync with the original one.
  Verified live twice, deliberately: once reproducing the bug (the
  Victory screen appearing with the attribute choice silently skipped),
  once confirming the fix (the attribute screen and the level-7 talent
  unlock both shown, in that order, before Victory finally appears).

## Monster attribute rebalance (decided Prompt 27)

- **Monster Str/Dex/Int were rebalanced by reproducing the existing
  tuned outcomes exactly, not by guessing new numbers from scratch.**
  Every monster's damage total and dodge percentage from Prompt 21 was
  treated as the fixed target; new attribute values were derived to hit
  those exact numbers under the new formula, the same "recalibrate the
  inputs, keep the tuned output" discipline the original Prompt 14
  attribute system and the Prompt 21 rebalance both already used. The
  actual gameplay balance this project has already validated wasn't
  thrown out just because the formula underneath it changed shape.
- **Monster attributes are explicitly not held to the same "genuinely
  low, hand-picked" philosophy player starting stats follow -- a
  deliberate scope distinction, not an inconsistency.** Players start
  at 2 or 6 specifically because the whole redesign's premise is
  growing from a low, honest starting point through real choices.
  Monsters are static and never grow through play at all -- there's no
  narrative reason to constrain them to the same range, and doing so
  would have meant sacrificing real, already-tuned gameplay identities
  (Spider and Archer's evasiveness) rather than preserving them.
  Spider's Dexterity of 36 and Archer's 48 exist purely as the inputs
  that reproduce 18%/24% dodge under the new linear, uncapped-below-25%
  formula -- not an implied claim that these monsters are "more
  developed" than a level-10 player capable of similar investment.
- **A real, positive side effect was noticed and named rather than
  left as an unexamined coincidence.** The same high Dexterity chosen
  purely to preserve Spider/Archer's dodge identity also gives them a
  meaningfully high crit chance under the new global crit system (23%
  and 29%) -- nobody separately designed "make the evasive monsters
  also precise," but the math worked out that way, and it reinforces
  rather than undercuts their existing character. Worth documenting as
  an observed consequence, since a future reader tuning these numbers
  further should know the dodge and crit values are coupled through
  the same Dexterity field, not independently adjustable.
- **Verified against the single case most likely to expose an error,
  not an arbitrary one.** The Ogre was specifically chosen for live
  verification because most of its damage total comes from its
  Strength-derived bonus rather than the flat power field -- exactly
  the scenario where a recalibration mistake would be most visible.
  Real combat produced "Ogre hits Player for 5" consistently and a
  single "Ogre critically hits Player for 7," both landing exactly on
  the hand-computed values, not just plausibly close to them.
- **The existing test suite needed zero changes for this rebalance,
  and that result itself was treated as a finding to confirm, not
  simply accepted at face value.** Every test passing unchanged after
  a full monster-stat rewrite could mean either "the rebalance is
  correct" or "nothing was actually testing these values in the first
  place" -- live-verifying the Ogre's exact damage output distinguished
  between those two possibilities rather than trusting the more
  convenient explanation.

## The attribute-system redesign (decided Prompt 26)

- **No baseline at all, replacing the old baseline-10 model
  outright, not extending it.** The original system (Prompt 14)
  compared every stat against an implicit "average" of 10 -- a point
  below it was a penalty, a point above it was a bonus. The new system
  has nothing to compare against: a class's starting spread is a
  genuinely low, hand-picked value (2 or 6), and every point counts at
  its full value from zero. This wasn't a refinement of the old model;
  it's a different one, chosen specifically because the redesign
  conversation wanted starting stats to read as "an origin, not a
  spreadsheet-optimal allocation."
- **Starting HP/Mana are hand-picked literals, deliberately independent
  of the attribute spread that determines everything else.** A Warrior
  with Strength 6 does *not* have its starting 30 HP computed from that
  6 -- the two are separate decisions on purpose, confirmed explicitly
  during design rather than assumed, specifically to avoid a class's
  identity-defining attribute silently double-counting into a stat it
  was never meant to touch.
- **Damage scaling moved from the actor to the ability.** Every talent
  now declares exactly one `ScalingStat` it scales from -- there is no
  attribute that universally boosts all of an actor's damage the way
  the old `physicalDamageBonus`/`magicDamageBonus` implicitly did for
  every Physical/Magic-typed talent regardless of what it actually was.
  This was a deliberate design requirement from the start ("I don't
  want attributes to automatically increase all damage. Skills should
  explicitly determine which attribute they scale from"), not a
  refactor discovered to be necessary later.
- **The damage formula scales by the ability's own cooldown tier
  (Filler/Core/Power/Signature), a decision explicitly delegated and
  made on this project's own judgment, not specified by the person.**
  Rationale: a talent used rarely should reward invested points more
  than one spammed every turn, or investment would trivially favor
  whichever ability has the lowest cooldown regardless of its actual
  role in a kit. Explicitly documented as a first-pass formula needing
  real playtesting, the same "reasoned guess, not derived-to-be-
  correct" honesty this project has applied to every rebalance since
  Prompt 21.
- **Crit is global -- every actor, player and monster, rolls the exact
  same `rollDodge`/`rollCrit` functions.** Considered and rejected: a
  separate, simpler monster-side crit check. Reusing the identical
  function for both sides means dodge and crit behave identically
  regardless of which side of a fight is attacking, the same "one
  function, not two copies" reasoning `rollDodge` has followed since
  Prompt 14.
- **Testing required a genuinely new pattern, not just updated
  numbers, and this was verified by actually running the suite
  repeatedly, not by reasoning that the pattern should work.** Global
  crit means any damage assertion now has a real (sometimes double-
  digit percentage) chance of landing an unplanned critical hit. An
  exact `==` would be flaky -- intermittently, unpredictably wrong, the
  worst kind of test failure since it erodes trust in the whole suite.
  Every affected check was rewritten to verify the result matches
  *either* the normal or the critical value, then the full suite was
  run 5+ times consecutively specifically to catch any check that
  still had a hidden flakiness risk the manual audit missed -- and one
  was found and fixed this way (`talent_test.cpp`'s Quick Strike check,
  which had coincidentally computed the same value under both the old
  and new formulas and so hadn't shown up as a failure, masking that it
  was still exposed to the same crit-flakiness as everything else).
- **`Stats`' default Dexterity was a live bug caught by reasoning about
  the new model's implications, not by a failing test.** It defaulted
  to 10, a leftover from the deleted baseline-10 model -- under the new
  formula, `dodgeChance(10)` is a real 5%, not the intended-neutral 0%
  it used to represent. Left unfixed, every test's default-constructed
  target (and any production code relying on the default) would have
  carried a hidden, unintended dodge chance. Fixed to 0, the only value
  that means "no investment, no bonus" under a system with nothing to
  offset against.
- **Piercing Shot's rework needed new per-talent fields
  (`bonusCritChance`/`bonusCritDamageMultiplier`) and overloaded
  formula functions, not a special case bolted onto the global crit
  constants.** `rollCrit(int)`/`critDamageMultiplier()` keep their
  original single-argument behavior completely unchanged for every
  other talent in the game; the two-argument overloads exist
  specifically so one talent's inherent bonus doesn't require every
  other call site to pass an explicit zero. Caught and fixed a real
  discrepancy in this exact rework via live testing: a code comment
  claimed the Thief's Strength (2) contributed "0 bonus at every tier,"
  which was true for Filler/Core/Power but not for Signature (Piercing
  Shot's own cooldown of 7) -- `2/5 * 2.5 == 1.0`, a real +1, not 0.
  Live testing surfaced the discrepancy (11 damage dealt, not the
  commented 10); a new, dedicated damage-application test was added
  specifically to catch this class of error automatically going
  forward, not just documented as a one-off correction.
- **The Vorbis/OGG system-dependency issue, caught while enabling audio
  for this same session's work, generalizes as a principle worth
  naming here too: verify a portability claim by actually breaking the
  environment and rebuilding, not by reading what a flag is supposed
  to do.** `SFML_USE_SYSTEM_DEPS` defaults differently per platform;
  forcing it off and then literally uninstalling the system libraries
  before confirming a clean rebuild still succeeds is the same standard
  of evidence applied throughout this redesign's own testing work.
- **Explicitly deferred, not silently dropped: monster attribute
  rebalancing, and renaming `FighterTalents.cpp`/`SorcererTalents.cpp`
  to match `Warrior`/`Mage`.** Monster Strength/Dexterity/Intelligence
  are still the old 6-20-range values, now interacting with a formula
  they were never tuned against -- functionally correct, not
  balanced. Given how much of this redesign already touched (classes,
  leveling, damage formulas, crit, dodge, one talent's mechanic, the
  entire test suite), a full monster rebalance pass and a cosmetic
  file/function rename were both deliberately scoped out rather than
  attempted in the same pass, and recorded in `ROADMAP.md` as real,
  tracked follow-up work rather than left implicit.

## Level-gated talent unlocks (decided Prompt 23)

- **New talents land at levels 4 and 7 specifically, to line up with the
  existing Elite/Nightmare tier bands, not chosen arbitrarily.** A
  growing kit arriving right as monsters get meaningfully tougher (3-6:
  Elite, 7-10: Nightmare) reinforces the pacing rather than being
  disconnected from it.
- **`TalentSet::learnTalent()` only ever appends, never inserts.**
  Cooldowns are tracked positionally by index throughout this entire
  project -- inserting a new talent anywhere but the end would silently
  shift every later talent's index, and by extension every place that
  refers to a talent by index (including saved cooldown state). Append-
  only was a hard requirement, not a convenience.
- **Every new talent is designed as an explicit upgrade on an existing
  one, not new mechanical ground.** Fighter's Whirlwind is Cleave with
  a wider radius and more damage; each class's level-7 pick is a
  stronger, longer version of its own level-2 self-buff. The one
  genuine exception -- Thief's Piercing Shot -- deliberately reuses the
  Spellblade's Execution mechanic (Prompt 9) rather than inventing a
  new one, the first time that specific mechanic has been used outside
  the class it was built for.
- **`talentUnlockedAtLevel()` checks every level actually crossed, not
  just the final level reached, via a loop -- the same reasoning
  `PlayerLeveling::grantXp()`'s own loop already established at Prompt
  20.** A single large XP grant landing on level 7 from level 3 must
  still trigger the level-4 unlock along the way, not skip it because
  the grant "landed past it."

## The Fighter/Sorcerer hybrid path (decided Prompt 24)

- **The hybrid pool is the opposing class's actual kit, not a separate
  pre-written talent list -- refined through direct conversation away
  from the original "permanent meta-unlock" assumption into something
  more interesting.** A Fighter specced into the hybrid path draws from
  Sorcerer's real abilities; the mechanical hybridization *is* the
  Spellblade concept, rather than a separately-maintained class
  description that has to be kept in sync with two other kits by hand.
- **Only Fighter and Sorcerer are hybrid-eligible, not Thief.**
  Spellblade's identity is specifically Str+Int; Fighter and Sorcerer
  each represent half of that combination, so either growing toward the
  other reads as a coherent hybrid. Thief (pure Dexterity) shares
  neither stat -- "a rogue suddenly gains both melee and magic
  aptitude" doesn't follow the same internal logic the other two do,
  so it was deliberately left out rather than included for symmetry's
  own sake.
- **Level 5 is simultaneously the spec-in decision and the first pick,
  not two separate steps.** Combining them means there's no
  intermediate state of "specced in but holding zero hybrid talents" to
  reason about -- `hybridSpecced()` is true if and only if at least one
  hybrid talent is known, with no separate bookkeeping needed to keep
  those two facts in sync.
- **The choice loop (`processLevelUpEffects`) is resumable, specifically
  because a hybrid choice can pause mid-multi-level-jump.** A big XP
  grant could cross both a hybrid-choice level and a later base-class
  unlock level in the same grant; pausing to let the person decide
  must not silently drop whatever comes after. Storing
  `pendingHybridChoiceLevel_` and resuming from `level + 1` once the
  person responds was worth the extra state to get this right, rather
  than accepting "a multi-level jump through a choice point loses
  track of anything past it" as a known gap.
- **A serious, real save/load bug was found and fixed within this same
  prompt, not left for later.** `loadGame()` had never accounted for a
  talent list longer than a class's default starting kit -- every
  level-unlocked and hybrid-picked talent was silently discarded on any
  save/load round-trip, for both this prompt and Prompt 23's talent
  unlocks. Caught specifically by trying to live-verify the hybrid
  path's own save/load behavior, not by a targeted audit -- a reminder
  that new state needs new save-path testing even when the save format
  itself hasn't obviously changed shape.
- **The fix re-derives what's derivable and only saves what's a genuine
  choice, rather than re-serializing the whole talent list.**
  Base-class level unlocks are deterministic from the saved player
  level (`loadGame()` just re-checks `talentUnlockedAtLevel()` against
  it, no new save data needed) -- the same principle monster tier
  reconstruction already established at Prompt 22. Hybrid picks are
  different in kind: which specific abilities were chosen is a real
  decision that can't be recomputed from anything else, so those
  genuinely needed a new save field -- a list of talent names, not full
  `Talent` structs, since a name is enough to look the talent back up
  against `HybridSpec::fullKitForClass()`.
- **Talent names are escaped (spaces to underscores) rather than
  switching this one field to a line-based read.** Every other field in
  this save format is a whitespace-delimited token; a name like "Arcane
  Bolt" would break that convention outright. Escaping keeps the whole
  format consistent rather than introducing a second, different reading
  strategy for just one field -- safe because no talent name in this
  project contains an underscore naturally, so the round-trip can never
  be ambiguous.
- **Verified live in both directions the fix needed to cover, not just
  the happy path.** Specced in, saved, and loaded into a genuinely
  fresh process to confirm both the re-derived base unlock and the
  explicitly re-learned hybrid pick showed up together correctly.
  Separately verified declining at the level-5 choice resumes play
  normally with the base-class unlock still granted and nothing hybrid
  added -- confirming the two code paths (accept vs. decline) diverge
  correctly, not just that the common case works.

## Elite/Nightmare tiers (decided Prompt 22)

- **Damage scales by recalibrating the total, not by multiplying the
  flat `power` field.** The same methodology every attribute-driven
  change has used since Prompt 14 (`base + attribute_bonus = target
  total`), applied here a second time on top of Prompt 21's already-
  rebalanced numbers. This mattered concretely, not just in principle:
  for the Ogre, most of its damage comes from `physicalDamageBonus`
  (Strength 18), not the flat power field -- scaling power alone would
  have left its real output barely changed at any tier.
  `monster_tier_test` specifically covers the Ogre for this reason, not
  just a zero-bonus monster like the Goblin where the distinction
  wouldn't show up.
- **`createMonster()`'s `tier` parameter defaults to `MonsterTier::Base`
  rather than requiring every caller to specify one.** Every pre-Prompt-
  22 call site -- production code and every existing test that
  constructs monsters through the factory -- keeps compiling and
  behaving completely unchanged. Tier is something spawning code opts
  into, not something every caller needs to know exists.
- **The boss deliberately ignores `tier` entirely, confirmed by testing
  the argument being explicitly passed and having no effect, not just
  relying on callers never passing one.** It's already a separately-
  tuned "hardest fight in the game" (Prompt 11); scaling it further at
  high character levels risked an absurd climax rather than a harder
  one. `monster_tier_test` explicitly requests Nightmare tier for the
  boss and confirms nothing changes, rather than only testing the case
  Application actually exercises.
- **One tier per dungeon, not mixed spawning within a level.** Computed
  once per `regenerateLevel()` call, applied uniformly to every regular
  monster spawned. Simpler to implement, test, and reason about than
  having each spawn slot independently roll a tier -- the tradeoff is
  less variety within a single dungeon, explicitly flagged in
  `ROADMAP.md` as a reasonable future refinement rather than a settled
  final design.
- **Tier isn't part of the save format.** Adding it would have meant
  another version bump; instead `loadGame()` re-derives tier from the
  already-saved `playerLevel` via `tierForLevel()`. This isn't a
  perfect reconstruction in every case (a level-up between a dungeon's
  monsters spawning and the save happening means the derived tier could
  differ from the original spawn tier), but it is a *consistent* one --
  a reconstructed monster's name, hp, and damage output always match
  some single real tier together, never a mismatched blend of two
  tiers' numbers, which is what would have happened by silently
  defaulting to Base on every load.
- **`monsterColor()` was refactored to key off `MonsterType` instead of
  the display-name string, caught as a real bug during this prompt, not
  a pre-existing one left alone.** The exact-string match
  (`name == "Goblin"`) would have silently failed for "Elite Goblin"
  and fallen through to the default gray -- breaking type-based color
  coding for every tiered monster the moment this prompt shipped, if
  left unfixed. `MonsterType` is stable regardless of tier or future
  display-name changes, removing the whole class of failure rather than
  patching around this one instance of it.
- **The Nightmare border color was corrected after actually looking at
  a screenshot, not left at the first reasonable-sounding choice.** A
  deep red was chosen initially and looked fine described in the
  abstract; checked against a real render, it nearly disappeared
  against the Goblin's own red tile. Replaced with plain white, which
  has to contrast against every color already in the roster (red,
  green, brown, tan, purple, orange) simultaneously, not just look
  distinct in isolation -- and re-verified visually, not just assumed
  fixed from the color values alone.
- **Verified live with exact, not approximate, numbers.** Crafted saves
  at level 4 and level 8 with an adjacent Goblin, confirming the
  combat log's exact tiered name and the exact rebalanced-then-tiered
  damage totals (4 -> 6 -> 7, matching `round(4 * 1.4)` and
  `round(4 * 1.8)` precisely) -- the same standard of precision applied
  to every other numeric verification in this project, not relaxed
  just because two rebalances are now stacked on top of each other.

## Corridor connectivity and monster rebalance (decided Prompt 21)

- **The "walled off" report was investigated with real proof, not
  reasoned about and dismissed.** Before touching any generation code,
  ran an actual BFS pathfind from the player's start to the boss room
  on the exact seed in question (confirmed reachable, 25 moves) and
  then walked that literal path live, screenshotting partway through --
  the camera visibly revealing a long corridor that hadn't been shown
  at spawn. This distinguished a genuine structural limitation (worth
  fixing) from a possible non-issue (which would have meant not
  touching working code) before deciding which one this actually was.
- **Extra connections are added *after* the chain-guarantee logic runs,
  never replacing or modifying it.** The existing linked-list room
  connectivity (Prompt 8) is what makes 100% reachability provable at
  all; layering additional shortcuts on top afterward means that
  guarantee stays exactly as strong as before, and `dungeon_test`
  re-confirms it rather than assuming extra code can't break anything.
- **Capped and probabilistic (max 6 extra connections, 40% chance per
  eligible nearby pair), not "connect every nearby pair."** A fully
  connected grid would solve "feels like one long hallway" by
  overcorrecting into "no sense of distinct areas at all" -- neither
  extreme was the goal. The cap and chance are deliberately
  unremarkable numbers, not deeply tuned; worth revisiting if the feel
  still isn't right once there's been more time to actually play it.
- **The boss room is deliberately excluded from extra connections.**
  Every other room can gain a shortcut; the boss room's entire
  character (Prompt 11: "the far end of the level, reached at the end
  of the sequence") depends on it staying reachable *only* through the
  full chain. Adding a shortcut to it would undercut the specific
  design intent that room was built around.
- **Monster damage was reduced by recalibrating each monster's base
  `power`, not by scaling down the final numbers directly or touching
  attributes.** Same methodology as every attribute-driven change since
  Prompt 14: `base_power` is chosen so that `base_power +
  attribute_bonus` equals the new target total, keeping the
  attribute-bonus system's own meaning intact rather than working
  around it. Attributes themselves (Strength/Dexterity/Intelligence)
  were left untouched -- this rebalance is specifically about *this
  vertical slice's* damage numbers being tuned for a pre-leveling
  world, not about the attribute formulas being wrong.
- **The boss was rebalanced less aggressively than the regular
  roster** (roughly -25/-30% versus -20/-33%) **on purpose, not by
  accident of rounding.** It's meant to remain the hardest fight in the
  game after this prompt, not become trivial -- the goal was "not
  instant-death for a level-1 character who reaches it early," not
  "beatable by a level-1 character," which is a meaningfully different
  bar and was confirmed live: the same rebalanced walkthrough that
  survived the Archer with real margin still died to the boss, which is
  the intended outcome.
- **Verified by re-running the literal same scripted encounter, not a
  fresh one.** Re-using the exact walkthrough that had nearly killed a
  level-1 Fighter before this prompt (rather than constructing a new
  test scenario) meant the *before* and *after* numbers were directly,
  precisely comparable -- the Archer's hits dropping from an observed 4
  to an observed 3, not just "presumably lower now."

## Leveling system (decided Prompt 20)

- **XP is tracked as progress toward the *next* level, not a cumulative
  lifetime total.** `Player::xp()` resets (carrying any excess) on
  every level-up, so a value like "12/40 XP" reads directly as
  "progress within this level" for the HUD without needing a
  cumulative-to-incremental conversion at display time. The tradeoff is
  that `grantXp()` has to loop rather than compare against a single
  cumulative threshold -- judged worth it for the simpler, more directly
  meaningful runtime representation.
- **`PlayerLeveling` is a separate module from `Player`, not methods on
  it.** `Player` stays plain data plus simple accessors; the curve
  (`xpForNextLevel`) and the level-up side effects (`grantXp`) live
  outside it. The same split this project has kept everywhere else --
  `Stats` stays plain data with `AttributeFormulas` interpreting it from
  the outside, `Talent` stays plain data with `TalentEffects` applying
  it. Consistency here specifically, not a one-off choice: a future
  reader who already knows this project's shape from the other three
  pairs should recognize this one instantly.
- **`grantXp()` loops specifically because a single grant can cross more
  than one level threshold at once -- verified as a real scenario, not
  a hypothetical edge case guarded against out of caution.** The boss's
  200 XP reward against a fresh level-1 character crosses four
  thresholds in one grant (20+40+60+80 == 200, landing exactly on level
  5) -- confirmed both in `player_leveling_test` (hand-computed) and
  live (an actual boss kill from level 1 landed on level 5 exactly,
  matching the test's prediction precisely, not approximately).
- **The level-up effect (+3 maxHp, +2 maxMana, full heal) is explicitly
  a placeholder, documented as such in three places** (`PlayerLeveling.hpp`'s
  header comment, `ROADMAP.md`'s Prompt 20 entry, and here) -- "unlock
  more abilities, change how abilities work" was explicitly deferred to
  a later design conversation when this prompt was scoped, not decided
  here. The stat growth exists so a level-up still *feels* like
  something happened even before that later design exists, not as a
  final balance decision.
- **`Actor::xpReward()` lives on the shared base, not `Monster`
  specifically, even though only monsters ever have a non-zero value.**
  `Application::checkAndHandleDeath` only ever sees a generic `Actor&`
  (it's called for the player, the boss, and every regular monster
  through the same code path) -- putting the accessor on `Actor`
  avoids needing a downcast to read it, the same reasoning `Stats`/
  `statusEffects()` already follow for being on `Actor` rather than
  `Monster` even though the player's copies see comparatively little
  use.
- **`MonsterFactory::xpRewardForType()` is set automatically inside
  `createMonster()`, via `Actor::setXpReward()`, not left for each
  caller to remember.** Exposed as its own function (not just inlined)
  specifically so it stays independently testable and callable by
  anything that only needs the reward number, not a whole `Monster` --
  but every actual caller of `createMonster()` gets a monster with the
  right reward already attached, with nothing extra to remember.
- **`selectClass()` resets level and XP to 1/0 on every fresh
  selection.** `player_` is a single long-lived object mutated across
  runs (via `selectClass()` and `loadGame()`) rather than reconstructed
  from scratch each time -- without an explicit reset, picking a new
  class after a previous run reached, say, level 6 would silently carry
  that level into what's supposed to read as a brand new character.
  `loadGame()` is the only path that should ever set these fields to
  anything other than the fresh-start defaults.
- **A stale comment fixed opportunistically, not left for later.**
  `checkAndHandleDeath`'s header comment still described the
  pre-Prompt-17 behavior (the player's death closing the window
  outright; victory leaving the window open with nothing further
  happening) -- found while adding the new XP-granting call sites in
  the same function, and corrected on the spot rather than left
  actively misleading a future reader who happened to look at the
  comment instead of the code.
- **Verified live with the exact multi-level-up scenario, not just a
  simple single-level case.** It would have been easy to test only "one
  kill grants XP, HUD updates" and call the feature done; specifically
  re-created the boss-kill-from-level-1 scenario live (via a
  hand-crafted save) to confirm the loop in `grantXp()` behaves
  correctly under the one condition -- a large single grant -- that
  actually exercises it, landing on the exact level `player_leveling_test`
  predicted rather than an approximately-right one.

## Three base classes: Fighter, Sorcerer, Thief (decided Prompt 19)

- **Enum ordinals preserved across the rename, on purpose.** Fighter
  (was Marauder) and Thief (was Archer) keep their old integer
  positions in `PlayerClass`; only Sorcerer, genuinely new, is appended
  at the end. This means any save file written before this prompt still
  decodes to the same class it always did -- a rename is a display and
  source-level change, not a data-format change, and treating it that
  way meant no save-version bump was needed for the rename itself.
- **Files renamed to match, not just a display string swapped.**
  `MarauderTalents.hpp/.cpp` became `FighterTalents.hpp/.cpp`,
  `marauder_test.cpp` became `fighter_test.cpp`, and so on for Archer/
  Thief -- every identifier, comment, and filename updated together.
  A file/class-name mismatch (a file called `MarauderTalents.cpp`
  defining `fighterTalents()`) would read as sloppy in a codebase meant
  to demonstrate care, even though a lighter-weight "just change the
  display string" approach would have touched far fewer files.
- **Historical `ROADMAP.md`/`ARCHITECTURE_DECISIONS.md` entries from
  Prompts 15/16 were NOT retroactively renamed.** They document what
  was decided under the names "Marauder" and "Archer" at the time --
  rewriting history to match a later rename would be revisionist and
  break from how every other change in this project has been recorded
  (new entries documenting a change, never edits erasing what was
  previously true). Source code comments that reference those prompts
  do note the rename explicitly, so a reader isn't left confused about
  why a historical section title doesn't match current code.
- **A real, repeated lesson about thorough searching, not a one-off.**
  The exact same failure mode that hit Prompt 15 (an initial targeted
  grep for `Player`-construction sites missing 14 of 16 actual sites,
  caught only by a broader pattern after the first build failed) hit
  again here: `savegame_test.cpp` referenced `PlayerClass::Marauder`
  directly and wasn't caught until a full build surfaced it as a
  compile error. Worth naming directly as a pattern rather than treating
  each occurrence as an isolated surprise: a rename or signature change
  needs a genuinely comprehensive sweep, not just the file-level renames
  that feel like they should be exhaustive.
- **Sorcerer's Strength and Dexterity are both true dump stats (6 and
  8), unlike the Thief's moderate Strength (14).** The Thief needed a
  real secondary investment because Dexterity carries no damage bonus
  in this project's formula system (see "Attribute-driven combat,"
  Prompt 14) -- its damage still has to come from somewhere. The
  Sorcerer's Intelligence already covers both its own damage *and*
  mana, so there's no equivalent gap to cover, and both non-primary
  stats could be left at genuine dump-stat values without the class
  becoming non-functional.
- **Mind Shatter needed a new `Talent::onHitEffect`/`onHitChance` pair,
  not a new `TalentEffectKind`.** Every existing Damage-kind talent
  already affects its target via damage; Mind Shatter needed to
  *additionally* affect the target with a status effect on a successful
  hit -- a supplementary property of an existing kind, not a
  fundamentally different kind of talent. Mirrors
  `MonsterAttackProfile`'s own `onHitEffect`/`onHitChance` fields
  exactly (Poison and Stun have worked this way for monster attacks
  since Prompt 10) rather than inventing new semantics. Appended at the
  very end of `Talent`, following the same positional-aggregate-
  initialization safety rule established when `damageType` was added.
- **A shared `rollChance()`, extracted specifically because a third
  duplicate was about to appear, not preemptively.** Before this
  prompt, two independent places each declared their own local static
  RNG for a "does this probability succeed" check: `Chaser`'s
  `onHitChance` roll (Prompt 10) and `rollDodge` (Prompt 14). Mind
  Shatter's on-hit roll would have been a third. Extracted into
  `AttributeFormulas::rollChance()` and refactored both existing call
  sites onto it, rather than adding a third copy -- the same "extract
  once a third copy would appear" discipline `AIUtils.hpp` followed at
  Prompt 11, applied to randomness instead of geometry helpers.
  `didDodge` (the pure comparison) stays independently tested; the
  actual dice roll remains deliberately untested in isolation, same
  precedent as before.
- **A genuinely new kind of test coverage for a probabilistic mechanic:
  forcing `onHitChance` to 1.0 on a local copy of a talent to get
  deterministic, repeatable coverage of the *application* logic.**
  `sorcerer_test` doesn't try to statistically verify a 60% hit rate (it
  wouldn't be reliable across test runs) and doesn't skip testing the
  mechanism entirely either (which would leave real coverage on the
  table) -- it isolates the one piece that's actually meant to be
  deterministic (does a successful roll correctly apply the effect) from
  the one piece that isn't (whether that roll succeeds), and only tests
  the former with real rigor.
- **A real bug in this session's own new code, caught by the new test
  itself on the first build.** `SorcererTalents.cpp` left every
  Damage-kind talent's `damageType` at its default, which is
  `Physical` -- not `Magic`, despite a comment in the same file
  initially (incorrectly) claiming otherwise. `sorcerer_test` failed 4
  checks immediately, including damage totals that didn't match the
  documented formula, which is what made the mistake obvious rather
  than a subtle live-only discovery.
- **A build-hygiene bug, not a logic bug, caught by a live test that
  didn't match a passing unit test.** After fixing the `damageType`
  issue above and confirming `sorcerer_test` passed cleanly, a live
  Mind Shatter cast still showed only 1 damage instead of the expected
  10. The fix was correct; the running `roguelike` binary simply hadn't
  been relinked with it yet, despite an apparently unrestricted
  `cmake --build build` having run in between. Resolved by forcing a
  fresh rebuild and reconfirming live with a build whose freshness
  wasn't left to assumption. Worth remembering going forward: a passing
  test suite for one target doesn't guarantee every other binary
  sharing that same source file has actually been relinked before the
  next live check depending on it.

## Camera/viewport and bigger dungeons (decided Prompt 18)

- **Camera is purely a rendering concern, computed fresh every frame,
  not stored game state.** `cameraX_`/`cameraY_` aren't part of
  `SaveGameState` -- they're fully derivable from `player_.position()`
  and `map_`'s dimensions, both of which already are saved. Recomputing
  each frame in `updateCamera()` means there's no separate "camera
  state" that could ever drift out of sync with where the player
  actually is, the same reasoning that's kept `TurnScheduler`'s exact
  energy values and other purely-derived state out of the save format
  since Prompt 12.
- **One `worldToScreen()` conversion function, not the tile/monster/
  player draw calls each computing their own offset.** Every place that
  used to compute `{x * kTileSize, y * kTileSize}` directly now calls
  the same helper instead. A single, obviously-correct conversion point
  is much easier to get right (and keep right) than three or four
  separate call sites each independently subtracting a camera offset.
- **Dungeon dimensions were increased and then re-verified empirically,
  not increased and assumed still safe.** The original 38x20/6-room
  tuning (Prompt 8/11) was itself the result of careful empirical
  sweeps documented at the time -- changing the numbers without
  re-checking would have thrown that verification away. `dungeon_test`
  already uses `DungeonGenerationParams`' defaults rather than
  hardcoding its own, so changing the struct's defaults was enough to
  get the new dimensions automatically re-checked by the exact same
  connectivity and boss-room-placement-rate tests that validated the
  original numbers -- no new test code needed, and the result (100%
  connectivity, 100% boss placement, both *better* than the original
  ~97%) is a real measurement, not an assumption that "bigger is
  probably still fine."
- **The tile render loop iterates only the camera-visible range, not
  the whole map, every frame.** A genuine performance consideration now
  that the map is meaningfully bigger than the viewport (60x32 tiles
  vs. roughly 40x22 visible at once) -- iterating every tile in a
  larger and larger map every single frame doesn't scale, and the
  visible-range restriction is also what makes the loop naturally
  correct without needing a separate per-tile "is this on screen" check
  bolted on afterward.
- **The HUD needed zero changes.** Every HUD element (hp/mana bars, the
  talent list, the combat log) was already drawn in absolute screen
  pixel coordinates from Prompt 13 onward, never tied to world/tile
  coordinates -- confirmed by checking, not assumed, before starting
  this prompt. Adding a camera only ever needed to touch the three
  things that actually live in world space: tiles, monsters, and the
  player.
- **Verified live for both the clamped and the following case, not
  just one.** A camera that only gets tested near the center of a map
  can look correct while still being wrong at the edges (or vice
  versa). Confirmed the player's actual starting position (10,8) on the
  new 60x32 map sits near enough to the top-left corner that the
  camera correctly clamps to (0,0) rather than centering and showing
  empty space past the boundary -- then moved well away from that edge
  and confirmed the camera genuinely follows and centers, visible
  directly in a screenshot showing previously-explored dimmed tiles
  now stretching well beyond anything the old fixed viewport could
  have shown at once.

## End-game screens (decided Prompt 17)

- **Chosen from an open-ended list rather than assumed.** `ROADMAP.md`'s
  Prompt 17+ item was explicitly "open-ended, lower priority... not
  sequenced precisely yet." Asked directly which of several genuinely
  different-shaped candidates (a UI fix, a rendering refactor, a new
  asset-driven system, more content) mattered most, rather than picking
  one and hoping it matched what was actually wanted.
- **A new `GameMode::GameOver`, not a special case bolted onto Playing
  or ClassSelection.** Reached from `checkAndHandleDeath` (player death
  or boss defeat), distinguished by a new `wonGame_` bool. Renders as a
  fully standalone screen -- the same "replace the view entirely"
  approach `renderClassSelection` already established -- rather than an
  overlay on the frozen game world.
- **Returning to class selection does the actual reset; the mode
  transition itself does nothing extra.** `selectClass()` already fully
  reconfigures `player_`, `map_`, `monsters_`, and `scheduler_` the
  moment a class is chosen -- there was no reason to duplicate that
  logic in the GameOver-to-ClassSelection transition, or to invent a
  separate "restart with the same class" path the person didn't ask for.
- **Defeating the boss now genuinely ends the run in victory -- this
  didn't exist as a game state at all before this prompt.** Previously,
  `checkAndHandleDeath`'s boss branch only logged a message and cleared
  `boss_`; play just continued with nothing actually won. Victory ends
  the run outright even if other regular monsters remain alive
  elsewhere in the dungeon, matching the boss's established role as the
  set-piece finale (Prompt 11), not one more kill among many.
- **A real, previously-invisible bug, found by removing `window_.close()`
  rather than introduced by it.** The first live death test logged "You
  have died!" twice. Cause: `advanceTurnsUntilPlayerCanAct` calls
  `checkAndHandleDeath` again on every loop iteration regardless of
  whether a prior call already handled that exact death -- hp stays at
  whatever negative value it ended on, so nothing about the old guard
  (`if (hp > 0) return`) caught the repeat. This bug's precondition has
  existed since `checkAndHandleDeath` was first written, but was
  completely unobservable for the entire life of this project until
  now: `window_.close()` used to make `window_.isOpen()` false
  immediately on the first call, and every later iteration of that
  same loop checks `if (!window_.isOpen()) return` before it would ever
  reach `checkAndHandleDeath` again. Removing the close-on-death
  behavior (the actual point of this prompt) is what finally let a
  pre-existing structural gap surface. Fixed with an explicit
  idempotency guard at the very top of `checkAndHandleDeath`: once
  `mode_ == GameOver`, return immediately, before even checking hp --
  applies uniformly to the player, the boss, and regular monsters
  alike, since none of their deaths matter anymore once the run has
  already ended.
- **Verified live with deterministic, hand-crafted save files for both
  paths, not natural play.** Real combat's dodge rolls made triggering
  an exact death or an exact boss kill on demand unreliable -- one
  scripted attempt to force a death via repeated attacks ended with the
  player surviving instead, purely from favorable RNG that run. Crafted
  save files placing the player at 1 hp next to a full-health monster
  (death path) and at full hp next to a 1-hp boss (victory path)
  instead, giving deterministic, repeatable outcomes to actually verify
  against -- confirmed exactly one death/victory message each (proving
  the idempotency fix), both screens rendering with the correct
  message and color, and Enter correctly returning to class selection
  from either.

## Archer and the vault mechanic (decided Prompt 16, Phase 2)

- **Archer (pure Dexterity) replaced the originally drafted Shadow
  (Dexterity + Intelligence) on direct request.** Fills the pure-Dex
  corner of the attribute triangle -- the mirror of Marauder's pure
  Strength -- rather than adding a second Str+Int-flavored hybrid
  alongside the Spellblade. A more legible three-class spread: one
  hybrid, two pure corners, each pure corner mechanically opposite the
  other (Marauder tanks damage with high hp and 0% dodge; Archer avoids
  it entirely with the lowest hp of any class and the dodge cap).
- **Dexterity does not scale damage in this project's formula system,
  and Archer's kit was designed around that rather than adding a new
  formula to work around it.** `AttributeFormulas` has exactly two
  damage-scaling functions, `physicalDamageBonus` (Strength) and
  `magicDamageBonus` (Intelligence) -- there is no
  `rangedDamageBonus(dexterity)`. Introducing one would have been new,
  untested formula surface for a single class's benefit; instead Archer
  keeps a moderate Strength (14, not a dump stat -- "someone has to
  actually draw the bow") so its Physical-typed attacks stay competent,
  while Dexterity governs the thing it already governs project-wide
  (dodge chance) and nothing else. A real design trade-off worth being
  explicit about, not a default nobody considered: a more "pure" design
  would tie ranged damage to Dexterity directly, but that's real scope
  this prompt didn't need to take on to deliver what was actually asked
  for.
- **`PlayerClass` collides in name, not in code, with the existing
  `MonsterType::Archer`** (the Kiter-AI enemy from Prompt 10). Separate
  enums, no compiler-level ambiguity anywhere, but worth documenting
  since a person could reasonably wonder "wait, am I playing the archer
  or fighting it" -- the answer is both are legitimately named that,
  independently, and the game doesn't need to disambiguate them any
  further than the enum system already does.
- **Vault Kick's knockback is a new `Talent::retreatDistance` field,
  not a new `EffectShape` or a special-cased talent.** Every other
  Damage-kind talent in the game only ever affects the target; Vault
  Kick needed to affect the *caster's own position* too, after the
  ordinary damage step. Adding one int field (0 for every existing
  talent, meaning "no change in behavior for anything but Vault Kick")
  was a smaller, more contained change than introducing a whole new
  shape that would need its own targeting-resolution branch in
  `tryUseTalent`. Placed at the very end of `Talent`, following the
  same positional-aggregate-initialization safety rule established when
  `damageType` was added at Prompt 14.
- **The retreat reuses `resolveBlinkDestination()` rather than new
  movement code.** "Walk up to N tiles in a direction, stopping early
  at a wall or another actor" is exactly what Blink already does; Vault
  Kick just computes a different direction (directly away from the
  target, rather than the caster's last-move direction) and feeds it
  into the same function. One retreat-capable movement primitive, not
  two independent implementations that could drift apart.
- **The retreat happens whether or not the kick's own damage was
  dodged.** Contrasted deliberately with how a dodged hit already
  blocks its on-hit status effect (Prompt 14: dodging a Spider bite
  means no Poison either) -- that precedent is about effects that ride
  in *on* a successful hit. The retreat here is different in kind: it's
  the caster's own follow-through motion, not something that depends on
  the kick connecting solidly. A player using Vault Kick to escape
  melee range shouldn't have that escape fail on an unlucky dodge roll
  from the *enemy's* side -- the utility half of the talent stays
  reliable even when the damage half doesn't land.
- **Live testing surfaced a wall-blocked retreat on the first real
  attempt, and it was correct behavior, not a bug -- confirmed by
  inspecting the map data directly rather than assuming either way.**
  The existing seed-1337 dungeon happened to put a wall directly behind
  the player at the position reached through normal play, so the
  retreat legitimately moved 0 tiles (identical to how Blink already
  behaves when fully boxed in). Rather than accept an undemonstrative
  test, crafted a save file by hand placing the player in known-open
  corridor space specifically to get a clean, visually clear
  demonstration -- confirmed both the exact retreat distance/direction
  and the exact damage dealt, numerically via the save file and
  visually via before/after screenshots showing the player and target
  newly separated by open floor.
- **`archer_test` covers every data-level property of the kit (and the
  ordinary damage math Vault Kick shares with every other Damage-kind
  talent) but explicitly cannot reach the retreat destination itself**
  -- that requires `Application::resolveBlinkDestination`, an
  `Application` method, unreachable from a standalone test the way
  `applyTalentDamage`/`applyTalentSelfBuff` (free functions) are. The
  test file says so directly in its own header comment, rather than
  silently having a coverage gap nobody documented.
- **End-game screens, the other half of the originally drafted Prompt
  16, were not part of this request and were not built.** Explicitly
  not silently dropped -- carried forward to Prompt 17 in `ROADMAP.md`,
  with the current "window just closes on death" behavior (Prompts
  10-11) noted as still unchanged.

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
- Camera/viewport now exists (Prompt 18) -- resolved, not open anymore.
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
- Game-over screens exist now (Prompt 17) -- what's still open is
  restarting *with the same class already selected* rather than always
  routing back through the class-selection screen first. A minor
  convenience gap, not a missing feature; picking the same number again
  is a one-keystroke cost.
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


## Prompt 35: tree progression replaces fixed player kits (2026-09-26)

User-approved direction: class defines initial access; attributes unchanged;
trees define builds; equipment enables/modifies actions. PROMPT_35_DESIGN.md is
canonical, superseding borrowed talents/native masteries and player runes.

TalentCatalog holds stable IDs and rank profiles. TalentProgression validates
purchases. Player owns access/balances; TalentSet owns ranks/cooldowns/hotbar.
Effective talents are shared by previews/casts; cooldown upgrades preserve their
original damage-scaling tier and running cooldowns. Monster kits stay independent.

Universal basic attack/wait, weapon categories and shield slot support tree
starts. Training gear has no stat bonuses. Conflicting equips fail without a
turn. Runes are removed, with generic chain targeting retained.

Version 9 rejects old saves and persists tree ownership, specialization, ranks,
point balances, cooldowns, hotbar and pending review/victory. IDs, budgets and
prerequisites are validated before live restore. Scheduler/AI-counter limitations
remain. Legacy kit adapters remain for isolated fixtures; live progression does
not grant their kits. Debug game build succeeds; no tests added/run or gameplay
verification this iteration. Existing fixtures migrated where removed APIs require
it; obsolete rune checks removed. Balance remains provisional.


### Stealth follow-up: contested concealment (2026-09-26)

Replaced static adjacent detection with a pure probability function in Stealth.hpp:
rank and capped effective player DEX reduce detection; capped enemy DEX increases
it; distance reduces it. Only enemies within four tiles and their actual field of
view can roll, once per non-stunned turn. Detection removes Concealment globally.
The pure probability is shared by enemy inspection, which never draws randomness.
Concealed magnitude stores source ability rank (legacy zero clamps to rank 1).
Conceal's mana/cooldown/duration stay fixed across ranks to budget for the stronger
hiding benefit. No type-specific archer exception: spiders and Lich benefit from
their existing DEX too. Formula/examples in PROMPT_35_DESIGN.md. Build-only evidence;
no new tests or runtime verification for this change.


## Prompt 36 - status combinations and defensive actions (2026-09-26)

Reused flat Guard instead of adding another absorption system. Marked is a single
refreshable charge (+25% next landed direct hit, three affected-actor turns);
dodges/DOT do not consume it. Universal Cleanse uses normal talent preflight and
commitment (C, no points/mana, one turn, cooldown eight) and removes only Poison,
Burn, Chill and Marked. StatusEffects centrally rejects stun refreshes and stuns
during recovery. The final skipped turn grants one turn of recovery, or two for
bosses whose maximum stun duration is one. Monster definitions restore these
rules; live recovery duration persists in the status vector. Player cooldowns
also tick during skipped stun turns.

Version 10 retains the prior serialization layout and migrates version 9 by
adding basic.cleanse without spending points or replacing bindings. Version 8
and older are rejected. Full ordering/examples: PROMPT_36_DESIGN.md. No tests
added/run; build evidence only, with runtime verification outstanding.

## Prompt 37 - Committed enemy intent and floor budgets

EnemyIntent is pointer-free Monster state, separate from speculative AIDecision.
Bomber/Ogre commitments resolve only after two/one voluntary player actions,
respectively, on the caster's next turn. Stun skips cannot spend this window.
Committed areas do not retarget; stun/displacement interrupt. Version 11 saves
persist intent; versions 9/10 load with no intent. Scheduler energy remains a
known persistence limitation, but cannot bypass the remaining reaction window.

EncounterPlan replaces player-level uniform tiers with floor budgets, atomic
pairs, a safe opening radius and capped elite/support/control populations.
Existing saves keep their monsters; later floors use the new plan. See
PROMPT_37_DESIGN.md for exact costs, caps and verification still pending.

## Prompt 38 - Optional sealed vaults

A vault is a leaf room added after all shortcuts, requiring untouched wall
space including its perimeter. One solid entrance opens only after the player
reads a warning and confirms. Guards are tagged Monster state; completion is
derived from surviving guards. Seven encounter points plus elite/Archer slots
are reserved, preserving floor budgets. The three reward instances are created
once with the floor; the UI chooses an existing instance instead of rerolling.
Version 12 persists gate/claim state, guard membership and offered items; 9-11
load with no vault in the current map. See PROMPT_38_DESIGN.md for full rules.

## Prompt 39 - Boss intent and persistent ritual budget

EnemyIntent now distinguishes stun, magic, heavy melee and summoning. Boss
recovery and new-summon readiness use the same voluntary-player-action clock.
LichBehavior only proposes summons; Application spends one of three lifetime
attempts when committing a visible warning. Occupied ritual tiles fizzle without
retargeting. Version 13 stores kinds, recovery, ritual count and Warlord phase/
enrage bookkeeping. Legacy Lich saves exhaust the unknowable ritual budget to
avoid load-based replenishment. Full rules: PROMPT_39_DESIGN.md.

## Town/travel - Persistent campaign snapshots

Application capture/restore is separated from disk I/O. Departed floor snapshots
reuse SaveGameState without recursive child worlds; travel imports world fields
only and retains current character/inventory/economy/RNG/ID state. Inactive floors
pause, avoiding offscreen battles and loot farming. Version 14 stores the active
floor plus at most nine cached snapshots, stairs, gold, town flag and quiet-turn
counter. Existing 9-13 saves have no recoverable earlier floors.

Universal H Waystone requires ten quiet completed actions and current safety.
R is bounded frame-paced waiting with danger/input interruption, replacing debug
regeneration. Stairs require safety but no ten-turn wait. Town services do not
advance dungeon time. See TOWN_TRAVEL_DESIGN.md for rules and pending checks.
