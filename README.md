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
│   │   ├── Application.hpp/.cpp     # owns window, map, roster, scheduler, FOV, combat
│   │   ├── TurnScheduler.hpp/.cpp   # energy/speed-based turn order
│   │   └── SaveGame.hpp/.cpp        # save/load -- entirely SFML-independent
│   ├── entities/         # Entity/Actor/Item/Feature hierarchy + components
│   │   ├── (Entity, Actor, Player, Monster, Item, Feature, Stats,
│   │   │    Inventory -- header-only)
│   │   ├── AIBehavior.hpp                # abstract interface + AIDecision; see ai/
│   │   ├── Talent.hpp                    # a talent/ability's data
│   │   ├── TalentSet.hpp/.cpp            # known talents + per-talent cooldown tracking
│   │   ├── TalentEffects.hpp/.cpp        # generic damage application
│   │   ├── SpellbladeTalents.hpp/.cpp    # the Spellblade's 8-talent kit, as data
│   │   ├── StatusEffects.hpp/.cpp        # Poison/Stun/Empowered bookkeeping
│   │   ├── StatusEffectLogic.hpp/.cpp    # the tick function (damage, stun-detection, expiry)
│   │   ├── MonsterAttackProfile.hpp      # flat-damage attack data for simple attackers
│   │   ├── MonsterType.hpp               # the roster enum (own file -- see Prompt 12 notes)
│   │   └── MonsterFactory.hpp/.cpp       # the 6-enemy roster + boss, as data
│   ├── ai/
│   │   ├── AIUtils.hpp            # shared geometry helpers (isAdjacent, distanceSquared, ...)
│   │   ├── NullAIBehavior.hpp     # never acts -- a real behavior, and a test placeholder
│   │   ├── Chaser.hpp/.cpp        # melee rusher + attack -- Goblin, Spider, Ogre
│   │   ├── Kiter.hpp/.cpp         # ranged, maintains distance -- Archer
│   │   ├── Support.hpp/.cpp       # buffs allies, never attacks -- Shaman
│   │   ├── AoEBomber.hpp/.cpp     # ranged AoE on a cooldown -- Bomber
│   │   └── BossBehavior.hpp/.cpp  # 3-phase set-piece fight -- Goblin Warlord
│   └── world/
│       ├── Tile.hpp                 # a single grid cell (type/walkable/transparent)
│       ├── Map.hpp/.cpp             # grid of tiles + ASCII-art level parser (test maps)
│       ├── FieldOfView.hpp/.cpp     # recursive shadowcasting (pure function)
│       ├── ExploredMap.hpp/.cpp     # Hidden/Remembered/Visible tracking over time
│       ├── Pathfinder.hpp/.cpp      # A*, 4-directional (pure function)
│       └── DungeonGenerator.hpp/.cpp  # rooms + corridors + a boss room (pure function)
├── tests/
│   ├── entity_smoke_test.cpp        # entity hierarchy, no SFML linked
│   ├── turn_scheduler_test.cpp      # turn order by speed, no SFML linked
│   ├── fov_test.cpp                 # prints an ASCII FOV grid, no SFML linked
│   ├── pathfinder_test.cpp          # prints an ASCII path around a forced detour
│   ├── chaser_test.cpp              # traces Chaser's own decisions turn by turn
│   ├── dungeon_test.cpp             # connectivity across 10 seeds + boss room success rate
│   ├── talent_test.cpp              # hand-computed damage/cooldown/conditional values
│   ├── monster_ai_test.cpp          # StatusEffects tick logic + Kiter/Support/AoEBomber
│   ├── boss_test.cpp                # phase transitions + per-phase decisions, hand-computed
│   └── savegame_test.cpp            # full round-trip, field by field, + error handling
├── assets/              # textures, fonts (still empty -- see Prompt 5 notes)
└── data/                # data-driven content (empty -- see Prompt 9/10 notes)
```

There are thirteen build targets: `roguelike` (the real game, links
SFML), and twelve standalone console programs with zero SFML
dependency. Run them after building:

```bash
./build/bin/entity_smoke_test
./build/bin/turn_scheduler_test
./build/bin/fov_test
./build/bin/pathfinder_test
./build/bin/chaser_test
./build/bin/dungeon_test
./build/bin/talent_test
./build/bin/marauder_test
./build/bin/monster_ai_test
./build/bin/boss_test
./build/bin/savegame_test
./build/bin/attribute_formulas_test
./build/bin/roguelike
```
On Windows with the Visual Studio generator, substitute
`.\build\bin\Debug\<name>.exe`.

**Controls:** on launch, **1** or **2** to pick a class (Spellblade or
Marauder). Arrow keys / WASD to move, **R** to regenerate the level,
number keys to use talents (Spellblade: 1-9; Marauder: 1-4 -- see the
class-selection screen or the talent-list panel for what's currently
bound), **F5** to save, **F9** to load (reachable even before picking a
class, to resume a previous run). Combat/status feedback is shown
**both** in the console and as an on-screen log in the game window (see
Prompt 13).

As of Prompt 5, `roguelike` opens a window with a player tile you can
move using arrow keys or WASD, routed through the real turn scheduler.

As of Prompt 6, only tiles within the player's field of view are shown
at full brightness; seen-before-but-not-visible tiles render dimmed;
never-seen tiles render as nothing. Sight radius is 8 tiles.

As of Prompt 7, monsters can path toward the player via A*, gated by
their own line of sight.

As of Prompt 8, the level is a procedurally generated dungeon -- new
layout every time you press R.

As of Prompt 9, the player is a **Spellblade** with a full 8-talent kit
(hp/mana bars, top-left corner).

As of Prompt 10, the dungeon is populated with a 6-enemy roster (see
below), and combat is genuinely two-way.

As of Prompt 11, a **set-piece boss room** holds the **Goblin Warlord**:
a 3-phase fight, shown with its own prominent top-center health bar.

As of Prompt 12, **F5 saves, F9 loads** -- map, fog-of-war, player state
(position/stats/talent cooldowns/status effects), and the full monster
roster (including the boss, if present) all round-trip exactly. Saves to
`savegame.txt` next to wherever the game is run from.

As of Prompt 13 (Phase 2), the engine renders **real text** for the
first time -- a bundled DejaVu Sans Mono font (`assets/fonts/`, license
included). Hp/mana now show as readable numbers next to their bars, a
talent-list panel shows all of the player's talents' live names and cooldown status
(dimmed while on cooldown), and an on-screen combat log mirrors the last
6 console messages in the game window itself. Every message in the game
-- combat, status effects, save/load, deaths -- now goes through one
`log()` helper instead of scattered direct `std::cout` calls.

As of Prompt 14 (Phase 2), Strength/Dexterity/Intelligence do something
for the first time -- previously present on every `Stats` but never read
by anything. Strength and Intelligence each add real bonus damage to
Physical- and Magic-tagged attacks respectively; Dexterity gives a real
chance to dodge an incoming hit entirely (symmetric -- monsters roll it
too); Intelligence also scales max mana. Every monster type now has
genuinely different attributes reflecting its identity instead of
identical defaults (see the table below). Every previously-tuned damage
number was recalibrated to still deal exactly the same amount as before
-- this is a new formula layer underneath existing balance, not a
difficulty change.

Also added post-Prompt 14: **Renewal** (key **9**), the Spellblade's
first healing spell -- a real gap before this, since there was no way
to recover hp at all. Heals 12 (capped at max hp), costs 6 mana, and
has the longest cooldown of any Spellblade talent (6 turns) so it
paces out rather than trivializing danger.

As of Prompt 15 (Phase 2), the game opens on a **class-selection
screen** -- press 1 or 2 to choose. **Marauder** is the second playable
class: pure Strength (Str 20 / Dex 8 / Int 4), a tankier 45 max hp than
the Spellblade's 30, and a genuinely different resource rhythm -- a free
spammable basic attack (Slam, zero mana cost), a small 10-mana pool for
Cleave (hits everything adjacent) and Rallying Cry (a self-buff, not
another damage number), and Berserker's Fury, which spends the
Marauder's own hp for the hardest single hit either class has. Save/load
now remembers which class is active and correctly restores that class's
own talent list before applying saved cooldowns.

| Enemy | Color | Behavior | Str/Dex/Int | Dodge |
|---|---|---|---|---|
| Goblin | red | `Chaser` -- plain melee | 10/10/10 | 0% |
| Spider | green | `Chaser` -- melee, applies Poison on hit | 8/16/10 | 18% |
| Ogre | brown | `Chaser` -- melee, chance to Stun on hit | 18/6/10 | 0% |
| Archer | tan | `Kiter` -- keeps its distance, shoots from range | 8/18/10 | 24% |
| Shaman | purple | `Support` -- never attacks; buffs a nearby ally instead | 6/10/18 | 0% |
| Bomber | orange | `AoEBomber` -- ranged area attack on a cooldown | 8/10/16 | 0% |
| Goblin Warlord | gold | `BossBehavior` -- 3-phase set-piece fight | 16/8/14 | 0% |

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
