# Roguelike Engine

A custom C++17 roguelike engine (portfolio project), built with a
composition-over-inheritance architecture and SFML 3 for windowing/rendering.

See `ARCHITECTURE_DECISIONS.md` for what's been decided and why, and
`ROADMAP.md` for the planned build sequence and current progress.

## Layout

```
roguelike/
├── CMakeLists.txt      # build config; fetches SFML via FetchContent
├── ARCHITECTURE_DECISIONS.md  # what's been decided, and why
├── ROADMAP.md                 # planned build sequence + progress
├── src/
│   ├── main.cpp         # entry point -- deliberately trivial
│   ├── core/
│   │   ├── Position.hpp             # shared grid-coordinate type
│   │   ├── Application.hpp/.cpp     # owns window, map, actors, scheduler, FOV, talents
│   │   └── TurnScheduler.hpp/.cpp   # energy/speed-based turn order
│   ├── entities/         # Entity/Actor/Item/Feature hierarchy + components
│   │   ├── (Entity, Actor, Player, Monster, Item, Feature, Stats,
│   │   │    Inventory, StatusEffects -- header-only)
│   │   ├── AIBehavior.hpp           # abstract interface; see ai/ for implementations
│   │   ├── Talent.hpp                # a talent's data (cost, cooldown, effect shape)
│   │   ├── TalentSet.hpp/.cpp        # known talents + per-talent cooldown tracking
│   │   ├── TalentEffects.hpp/.cpp    # generic damage application
│   │   └── SpellbladeTalents.hpp/.cpp  # the Spellblade's 8-talent kit, as data
│   ├── ai/
│   │   ├── NullAIBehavior.hpp   # never moves -- a real behavior, and a test placeholder
│   │   └── Chaser.hpp/.cpp      # A*-pathfinds toward a target within its own sight
│   └── world/
│       ├── Tile.hpp                 # a single grid cell (type/walkable/transparent)
│       ├── Map.hpp/.cpp             # grid of tiles + ASCII-art level parser (test maps)
│       ├── FieldOfView.hpp/.cpp     # recursive shadowcasting (pure function)
│       ├── ExploredMap.hpp/.cpp     # Hidden/Remembered/Visible tracking over time
│       ├── Pathfinder.hpp/.cpp      # A*, 4-directional (pure function)
│       └── DungeonGenerator.hpp/.cpp  # random rooms + corridors (pure function)
├── tests/
│   ├── entity_smoke_test.cpp        # entity hierarchy, no SFML linked
│   ├── turn_scheduler_test.cpp      # turn order by speed, no SFML linked
│   ├── fov_test.cpp                 # prints an ASCII FOV grid, no SFML linked
│   ├── pathfinder_test.cpp          # prints an ASCII path around a forced detour
│   ├── chaser_test.cpp              # traces Chaser's own decisions turn by turn
│   ├── dungeon_test.cpp             # verifies connectivity across 10 seeds + prints a layout
│   └── talent_test.cpp              # hand-computed damage/cooldown/conditional values
├── assets/              # textures, fonts (still empty -- see Prompt 5 notes)
└── data/                # data-driven content: monster definitions, etc. (empty for now)
```

There are eight build targets: `roguelike` (the real game, links SFML),
and seven standalone console programs with zero SFML dependency. Run
them after building:

```bash
./build/bin/entity_smoke_test
./build/bin/turn_scheduler_test
./build/bin/fov_test
./build/bin/pathfinder_test
./build/bin/chaser_test
./build/bin/dungeon_test
./build/bin/talent_test
./build/bin/roguelike
```
On Windows with the Visual Studio generator, substitute
`.\build\bin\Debug\<name>.exe`.

**Controls:** arrow keys / WASD to move, **R** to regenerate the level,
**1-8** to use talents (1-4 are the Blade tree: melee, must be adjacent;
5-8 are the Flame tree: ranged/AoE/utility). There's no text rendering
yet, so talent feedback (damage dealt, cooldowns, why a cast failed)
prints to the console rather than the game window -- watch the terminal
you launched it from, not just the window.

As of Prompt 5, `roguelike` opens a window with a player tile you can
move using arrow keys or WASD, collision-checked against the map and
routed through the real turn scheduler.

As of Prompt 6, only tiles within the player's field of view are shown
at full brightness; tiles seen before but not currently visible render
dimmed; tiles never seen render as nothing. Sight radius is 8 tiles.

As of Prompt 7, a red goblin monster (A*-pathfinding `Chaser` AI) shares
the level. It only shows up once you can see it, and once it can see
you, paths toward you and stops when adjacent.

As of Prompt 8, the level is a procedurally generated dungeon instead of
a fixed room -- different every time you press R.

As of Prompt 9, the player is a **Spellblade** with a full 8-talent kit
(see `ARCHITECTURE_DECISIONS.md` → "Talent system" for the design). You
can now actually fight the goblin: hp/mana bars in the top-left corner,
a floating hp bar over the goblin when it's visible, and it can die --
at which point it stops being drawn, stops taking turns, and can no
longer be targeted. It still can't attack back (Prompt 10).

## Building

### Linux (Debian/Ubuntu)

SFML needs a few system dev packages for windowing, graphics, and text
rendering (this list omits Audio/Network deps since those modules are
disabled in `CMakeLists.txt` for now):

```bash
sudo apt update
sudo apt install build-essential cmake \
    libxrandr-dev libxcursor-dev libxi-dev libudev-dev \
    libfreetype-dev libharfbuzz-dev \
    libgl1-mesa-dev libegl1-mesa-dev
```

Then configure and build:

```bash
cmake -B build
cmake --build build
```

The first configure will take a few minutes -- CMake's `FetchContent`
downloads SFML 3.1.0 source and builds it alongside the project. Subsequent
builds are incremental and fast.

Run it:

```bash
./build/bin/roguelike
```

A dark window titled "Roguelike Engine - Dev Window" should open. Closing
the window (or pressing Escape) exits cleanly.

### Windows / macOS

Same two commands (`cmake -B build`, `cmake --build build --config Release`)
work via Visual Studio, CLion, or the command line -- FetchContent handles
fetching and building SFML the same way on every platform. No manual SFML
install needed.

**Note on the executable's path (Visual Studio / Xcode users):** these are
*multi-config* generators -- one `cmake -B build` can produce Debug, Release,
etc. from the same tree, so each config gets its own output subfolder.
The binary ends up at `build/bin/<Config>/roguelike.exe`, e.g.:

```powershell
.\build\bin\Debug\roguelike.exe
```

not directly in `build/bin/`. Single-config generators (Ninja, Unix
Makefiles -- what Linux/macOS command-line builds typically use) don't do
this; there it's just `build/bin/roguelike`.

## Requirements

- CMake >= 3.28
- A C++17 compiler (GCC 9+, Clang 10+, or MSVC 2019 16.11+)
- Git (needed by FetchContent to clone SFML)
