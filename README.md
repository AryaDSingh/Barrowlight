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
│   │   ├── Application.hpp   # owns the window + loop shell
│   │   └── Application.cpp   # the only files allowed to touch sf:: types
│   └── entities/         # Entity/Actor/Item/Feature hierarchy + components
│       (Entity, Actor, Player, Monster, Item, Feature, Stats, AIBehavior,
│        Inventory, TalentSet, StatusEffects -- all header-only so far)
├── tests/
│   └── entity_smoke_test.cpp  # console-only sanity check, no SFML linked
├── assets/              # textures, fonts (empty for now)
└── data/                # data-driven definitions: monsters, talents, etc. (empty for now)
```

`assets/` and `data/` are still placeholders for later prompts. `entities/`
and `tests/` exist now because Prompt 3 actually needed them.

There are two build targets: `roguelike` (the real game, links SFML) and
`entity_smoke_test` (a standalone console program with zero SFML
dependency, proving the entity hierarchy has no rendering-library coupling).
Run the smoke test after building:

```bash
./build/bin/entity_smoke_test          # Linux/macOS
.\build\bin\Debug\entity_smoke_test.exe   # Windows (Visual Studio generator)
```

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
