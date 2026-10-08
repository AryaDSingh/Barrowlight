# Barrowlight: Design and Decisions

This document explains how Barrowlight is designed, how the engine is built, and
why: the choices that shaped it, the alternatives that were weighed, and the
ideas that were tried and replaced. The [README](README.md) covers building
and playing; this is the reasoning behind it.

It is written in three parts:

1. [The game](#part-1-the-game): what the player does and why each system exists.
2. [The engine](#part-2-the-engine): how the code is organised and why.
3. [Decision log](#part-3-decision-log): the major decisions in one place,
   including the ones that were later reversed, then the
   [known limitations](#known-limitations-and-future-work).

---

# Part 1: The game

## 1.1 Vision

A turn-based, one-life roguelike RPG in a gothic world of layered dungeons.
The name joins the two things the game is about: the barrows you descend
into, and the light you carry (or put out) to survive them.
You build a character by discovering how talent trees, gear and ascendancies
combine, and you decide how deep you dare to go.

**Pillars.** Every system is judged against these:

1. **Risk and reward are the player's choice.** You pick the dungeon and the
   depth. Going in under-levelled is tempting, not forbidden.
2. **Builds are discovered, not prescribed.** Your origin is a direction,
   not a cage. Hidden trees, deep trees and ascendancies reward exploring
   the system.
3. **Combat is readable and tactical.** Dangerous attacks are telegraphed.
   You win by reading intent, using the ground, light and positioning, not
   by out-statting.
4. **One life matters.** Permadeath gives every "push on or go back?" its
   weight. (Adventure mode, with two spare lives, exists for people who
   want the world without the stakes.)
5. **Player knowledge, not character power, is what grows.** A returning
   player should survive longer because they understand the world better,
   not because a meta-progression bar filled up.

**Show, don't tell.** Mechanics explain themselves through what happens on
screen. A crystal you touch three times cracks and releases what is inside;
names describe looks ("Weeping Crystal"), not effects; the log narrates
("The crystal cracks.") and never instructs. The exceptions are control
hints and the arrival notice, which lists what is on a floor without
explaining it.

**Inspirations, and what each contributed:**

| Game | What it gave this one |
|---|---|
| Path of Exile | Ascendancy as a late identity; keyword hover definitions; breach and essence events |
| Tales of Maj'Eyal | Tree-based talents; separate class and utility point pools |
| Dark and Darker | Floors built from hand-made and procedural modules; dungeons as level bands |
| Divinity: Original Sin 2 | Surfaces that combine (oil, fire, water, lightning, ice) |
| Dungeon Crawl Stone Soup | Gods with conducts; the tile art |
| Golden Krone Hotel | Light as a weapon; the vampire curse |
| NetHack, Spelunky | Bones and nemesis ideas: a dungeon that remembers |

## 1.2 The run

```
Town ──► choose a dungeon and a depth ──► fight, loot, level ──► push deeper, or return
  ▲                                                                   │
  └──────────── sell, buy, rest, trials, prepare ◄────────────────────┘
                     (death ends the character)
```

- **Town** is a menu hub: the inn, the merchant, equipment, the Trial
  Obelisk and the dungeon menu. Town actions don't advance dungeon time.
- **Dungeons** are chosen from the dungeon menu, at any depth already
  reached. Floors you leave are kept exactly as you left them (see 2.8),
  so a half-cleared floor is still half-cleared when you return.
- **Leaving** is by the stairs (only when nothing is hunting you) or by the
  Waystone (H), which needs ten quiet turns first. Retreating is a real
  decision, never free.
- **The main line** is the Ruins (floors 1–10: the Goblin Warlord at 5, the
  Lich at 10) and the Deep Crypts (11–20, a stronger Lich at 20). Killing
  the Lich at 20 is a victory. The victory screen offers **Go on**: the run
  continues down the winter road into Rimeholt, and the level cap rises from
  20 to 30.
- **Side dungeons** branch off the main line, each opened by something you
  find there: the Drowned Cathedral (the Warlord's sigil), the Ashen Foundry
  (a goblin captain's key) and Thornwood Hollow (a map carried by the
  Crypts' warden).

| Dungeon | Floors | As deep as | Opened by | Boss |
|---|---|---|---|---|
| The Ruins | 10 | 1–10 | — | Goblin Warlord, the Lich |
| Deep Crypts | 10 | 11–20 | the Ruins | the Lich |
| Drowned Cathedral | 6 | 7–12 | the Warlord's sigil | the Sleeper Below |
| Ashen Foundry | 5 | 6–10 | the Foreman's key | the Forgemaster |
| Thornwood Hollow | 5 | 14–18 | a map drawn on bone | the Hollow Mother |
| Rimeholt | 10 | 21–30 | going on past the Lich | the Winter King |

Every floor has a single id (1–46), and `floorDepth()` maps it to how
dangerous it is. That keeps one save format and one floor cache for every
dungeon, while a side dungeon can sit at any difficulty.

## 1.3 Floors

**Layout.** A floor is a 3×3 grid of cells, each 21×13 tiles, joined by
3-tile-wide sockets. A cell is either a hand-made module (from a library)
or a procedural one (hall, rooms, ruins or cavern). This came after a
simpler rooms-and-corridors generator: modules give each floor memorable
set pieces, and procedural cells keep layouts from repeating.

**Landmarks.** At least half of all floors are built around a landmark,
a hand-made set piece with an event, in a random cell that is never the
start or the exit. There are fifteen kinds: shrines, fountains, a blood
font, a ritual circle, hoards, cages, a champion's pit, the Blood Altar,
and rarer events from depth 7 such as a sealed tomb, a pale peddler and a
chained demon. These and the bosses are the source of unique items. Event chances
chain: 90% a floor has one, then 40% a second, then 10% a third.

**Roaming threats.** On 60% of floors a patrol walks between the cells.
From depth 3, 35% have a wandering champion with an escort. Linger, and
the floor turns on you: at 800 turns you "feel watched", and at 900
hunters arrive who always know where you are, then again every 400 turns.
This is a soft clock that pushes you onward without a hunger meter.

**Encounter budgets.** Each floor gets a budget, spent on themed packs
(paired groups, capped numbers of elites and support monsters, nothing
inside a safe radius of the start). This replaced uniform per-level tiers,
which made every floor feel the same.

## 1.4 Combat

- **Turns** are energy-based (see 2.4). Speed matters: Hasted and Slowed
  change how often an actor acts, and Chill is a pure slow.
- **Telegraphs.** Heavy attacks are committed in advance: the tiles light
  up, and you get a fixed number of your own actions to react. A committed
  area never retargets; a stun or a push interrupts it. Every boss uses this.
  Monsters that only hit hard, without warning, made fights feel unfair, so
  telegraphs became the standard for anything big.
- **Statuses.** Sixty of them, from Poison and Burn to Encased and
  Vampirism. They are kept as data on the actor and resolved in one place.
  Stuns have recovery rules (no refreshing a stun, a turn of immunity after
  one, two for bosses), so stun-locking a boss isn't a strategy.
- **Guard, dodge and criticals.** Guard is flat damage reduction. Dodge
  and critical chance come from Dexterity (0.5% per point; criticals start
  at 5%). Marked makes the next landed hit deal 25% more.
- **Darkness.** Unlit tiles are only visible when you're next to them.
  You carry a torch you can put out. Goblins, beasts and the undead see in
  the dark; humans need light; Gloomstalkers are deadly in the dark and burn
  in light. Light is a resource and a weapon, not a setting.
- **Surfaces.** Oil, water, fire, ice, electrified water, blood, acid, gas
  and thorns, and they combine: fire ignites oil and gas, cold freezes
  water and blood, lightning runs through anything wet. Braziers and
  barrels can be knocked over, and shoves push creatures into hazards.
  A spell's element acts on the ground it hits, so the battlefield is part
  of every build.

## 1.5 The character

**Origins.** Warrior, Thief and Mage set starting attributes, gear, sprite
and the pool your first tree point can open, and nothing else. They began
as fixed classes with fixed kits; the move to origins came from the
classless design (see 3.3).

**Attributes.** Strength, Dexterity and Intelligence, with no "average"
baseline: origins start low (2 or 6), and every point counts at full value.
An earlier model, where points below 10 were penalties, made low stats feel
like a debt and was replaced. Each ability scales from exactly one
attribute: +1 damage per 5 points, multiplied by its cooldown tier (×1 for
fillers up to ×2.5 for signatures). That keeps long-cooldown abilities
important as a character grows. Flat numbers in passives grow by 1 per 5
points too, so a passive bought at level 3 still matters at level 25.

**Levels and points.** Each level gives 2 attribute points, an ability
point and a utility point (the split is from ToME). Tree points come at
level 1, then 5, 10, 15, 25 and 30. The cap is 20 until you go on past the
Lich, then 30. It is gated because, with a cap of 30 everywhere, characters
out-levelled the original game long before its end.

**Gear.** Weapons come in kinds and depth tiers, each with an implicit
trait and an attribute requirement. Armour is cloth (a ward that soaks hits
and refills when you rest), light (evasion) or heavy (a percentage cut,
capped at 60%). Magic items roll 1–2 affixes and rares 3–6, some of them
conditional ("+damage in darkness") and some cursed. Drops are scarce on
purpose: the user's own playtest found gear "too easy", so finding a good
item had to mean something.

## 1.6 Talents

Talents are the heart of the game, and the most rebuilt system in it.

**Trees.** There are 47 trees: base trees (Fire, One-Handed, Bow,
Shadow...), utility trees (armour, Acrobatics, Stealth, Alchemy, Traps),
hybrids, and deep trees. Every tree has the same shape: a root, a **fork**
between two actives, the passive on the side you took, then a fork between
two capstones. Actives have three ranks, and the third changes how they play
(Fireball's mastery leaves the ground burning, for example). Passives have
one rank. Forks exist because the first version of the trees, a ladder of
nodes, gave the user "no build identity": two Fire mages played the same.

**Colours.** Every node carries one of sixteen colours: Steel, Guard,
Motion, Hunt, Guile, Flame, Frost, Storm, Earth, Water, Light, Dark,
Arcane, Blood, Death and Rot. Each rank bought adds a point of its colour.
Colours are what everything else is gated on, so the question "what is my
build?" has a readable answer.

**Resonances.** When two colours are both held deeply enough, the
resonance between them wakes: a passive that only that combination can
have (Steel and Flame give Searing Edge, for instance). There are 34 of
them. They are hidden until you hold one of the two colours, then shown as
a silhouette, so you can see something is there before you know what.
Opposite colours (Light and Dark, Flame and Frost...) have the most
transformative resonances, not the strongest.

**Hybrid trees** (Spellblade, Shadow Archer, Animation, Saboteur and more)
open on colours: usually 6 of each parent colour. Blood Magic is the
exception: you open it by offering your blood at the Blood Altar, after
killing the Vampire Lord who guards it, because it should feel special.

**Deep trees** are the late game's discoveries: Warbanner, Forgeborn,
Slagcaller, Tempest, Bonewright, Rimeheart, Briarheart, Packmaster,
Wintermarch and Gravecold. Each needs three things:
- **colours**, such as Frost 10 and Guard 6 for Wintermarch;
- **a level**, from 10 to 24;
- **a piece of lore**, carried by the monsters whose powers the tree
  teaches. The first Rime Wight you kill drops its frozen oath.

Lore is kept in the **journal** (J). Each tree is a silhouette until you
find its lore. This is the classless design's answer to "how do players
find hidden builds?": going deeper and paying attention.

**The talent map** (M on the talent screen) lays every tree around a ring
of the sixteen colours, so the whole space can be seen at once. Base trees
sit on the outer ring, hybrids further in, and deep trees closest to the
centre.

## 1.7 Ascendancy and trials

An ascendancy is a late, permanent identity: six unranked nodes, and one per
run. Each great boss drops a **sigil** that opens a **trial** at the Trial
Obelisk in town, which is a stronger version of that boss in an arena. Each
trial won gives an ascendancy point, and the first lets you choose.

**Which ascendancy is decided by colours alone.** For example, the
Juggernaut needs Steel 8 and Guard 6, the Elementalist two elements at 6,
and the Plaguebringer Rot 6 and Dark 6. Any trial won lets you take any
ascendancy your colours allow. The first version tied each ascendancy to a
class, and later each new one was meant to belong to its own trial. Both were
dropped: colours already say what a build has become, so a second gate added
nothing but bookkeeping.

There are twelve ascendancies and five trials (Stone, the Fallen, the Forge,
the Hollow and Winter). Some highlights:
- **Beastwarden:** Thornmaw, a great hound that levels with you.
- **Plaguebringer:** sickness that walks from room to room.
- **Gravelord:** the dead you raise stand until they fall.

## 1.8 Other systems

- **Gods.** Four patrons, sworn at shrines: the Seraph of the Last Dawn,
  That Which Sleeps Below, the Ash Saint and the Whisperer in the Walls.
  Each likes certain deeds and hates others; favour opens a boon at 30 and
  a prayer at 60, and at -20 the god's wrath falls on you. The idea is
  Dungeon Crawl's conducts: a god as a way to play, not a buff.
- **The vampire curse.** The Vampire Lord's bite curses you for 200
  turns. You see in the dark and your melee hits drink blood, but light
  burns you, your own torch included. It flips every light-and-surface rule
  you have learned.
- **Nemeses.** Flee a floor while a foe that hurt you badly still hunts
  you, and it takes a name and title, follows you down and hunts you on the
  next floor, stronger each time you run. The dungeon remembers you.
- **Companions.** Packmaster hounds, Thornmaw, raised dead and slag
  golems. Hounds and Thornmaw follow you between floors; the dead don't.
- **Events.** Crystals you strike to release what is inside, breaches you
  open by touching them, rifts, mimics in chests, and the Encounter Lab: a
  fixed room sequence for studying how a build plays (L on the class
  screen).

## 1.9 Presentation

- **UI** borrows from Tales of Maj'Eyal and Diablo II: stone panels with
  bronze frames, life and mana orbs, and tooltips over panels. Every screen
  draws through one kit (`UiKit`), so the game has one look.
- **Keywords** in descriptions (Burn, Chill, Heat...) are highlighted and
  show their definition on hover, after Path of Exile. Rules text stays
  short because the definition is one hover away.
- **Art** is pixel art from CC0 and CC-BY packs (Dungeon Crawl Stone Soup,
  Calciumtrice and others), credited in `assets/sprites/CREDITS.txt`.
- **Sound** is built from CC0 recordings by `tools/sfx_build.py` into
  families of variants (a hit sounds like what dealt it, every monster kind
  has a voice), so repeated actions don't sound identical. The game never
  depends on sound: a missing file is skipped.

---

# Part 2: The engine

## 2.1 Goals

This is a portfolio piece, so the engine is written from scratch in C++17
rather than built on an existing engine. The point is to design the
architecture, not inherit one. **Godot was considered and rejected**: it
would have meant adopting its scene tree and renderer. Using it through
GDExtension wouldn't have solved the tooling friction that prompted the
question, and would only half-preserve the "I designed this" story.

**SFML 3** was chosen over SDL3 and raylib. SFML's RAII types fit a
composition-based, exception-safe design without hand-writing wrappers
around C handles, and SFML 3 requires C++17 itself. SDL3 has the larger
ecosystem but a C API. raylib is the fastest to prototype with, but its
global-context style works against encapsulation.

**CMake with FetchContent** pins SFML to a tag and builds it from source,
so one command builds everything on any platform with nothing to install by
hand.

## 2.2 Layers

```
            ┌────────────────────────────────────────────┐
  game      │ engine_app: Application, UI, sprites, sound│  SFML
            ├────────────────────────────────────────────┤
  rules     │ engine_core: actors, talents, monsters,    │  no SFML
            │ items, dungeon generation, FOV, pathfinding,│
            │ AI, the turn scheduler, saving             │
            └────────────────────────────────────────────┘
```

**Only the app layer may touch SFML, and the build enforces it.**
`engine_core` doesn't link SFML, and every console test links only
`engine_core`. If a rule ever reaches for an `sf::` type, those tests stop
compiling. That boundary is what makes the rules testable in plain
programs, with no window and no mocking.

## 2.3 Composition over inheritance

```
Entity: a position, a name, a glyph
├── Actor: takes turns; holds Stats, StatusEffects, TalentSet, Inventory
│   ├── Player
│   └── Monster: + an AIBehavior, a type, a tier
├── Item
└── Feature
```

**A new monster is data plus which behaviour objects are plugged in, never
a new subclass.** A Frost Bear is a `Monster` with its own stats, a `Chaser`
behaviour and one rule (ice forms where it walks). `MonsterFactory` builds
all 48 monster kinds from seven `AIBehavior` strategies: Chaser, Kiter,
Support, AoE Bomber, Boss, Lich, and a null behaviour for things that only
wait. Per-kind rules (a Rot Witch's thorns, a thrall shattering) live in
small hooks keyed on the monster's type.

The behaviours are strategies: `decideAction()` returns a decision (move,
attack, use an ability, summon), and `Application` carries it out. A
behaviour proposes and the game disposes. This keeps behaviours free of
side effects and testable in isolation (see `chaser_test`, `boss_test` and
`lich_test`).

## 2.4 The turn scheduler

An energy model, as in Angband and ToME. Every tick, each actor gains energy
equal to its speed. Whoever crosses 1,000 acts, and 1,000 (not everything)
is subtracted, so extra speed carries over. Speed 200 acts exactly four times
as often as speed 50 (`turn_scheduler_test` checks the ratio).

Energy lives in the scheduler, not on the actor. Scheduling is
simulation state, not an attribute, and keeping it out of `Stats` keeps
`Stats` as "what combat reads". The scheduler answers only "whose turn is
it", nothing about what that actor does.

## 2.5 Map, sight and paths

- **`Map`** is a grid of tiles that knows nothing about SFML or actors.
  Walkable and transparent are separate fields (a chasm is transparent but
  not walkable).
- **Field of view** is recursive shadowcasting, written as a pure function
  of map, origin and radius. `ExploredMap` separately remembers what has
  been seen. Pure-plus-stateful splits recur throughout: they keep the hard
  algorithm testable on its own (`fov_test` prints the result so it can be
  checked by eye).
- **Pathfinding** is A*, 4-directional like the player's movement. A
  monster that could cut corners the player can't would follow a rule the
  player can't see.
- **Light** is computed per turn from every source (torches, braziers,
  fire, the player's light) into a lit-tile map that sight then filters.

## 2.6 Talents as data

A `Talent` is plain data: power, cost, cooldown, targeting mode, shape,
scaling attribute, and about a hundred optional fields for specific effects. The
catalogue (`TalentCatalog.hpp`) is a C++ table. That is still data-driven:
the logic is fully separate from the data, so a file loader could replace
the table without touching the rules. A loader and a format would add
machinery the game doesn't need.

The pieces each have one job:
- `Talent` is the data;
- `TalentSet` holds an actor's ranks, cooldowns and hotbar;
- `TalentEffects` applies damage and healing;
- `TalentProgression` decides what can be bought;
- `TalentTargeting` decides who an ability would hit.

`TalentTargeting` is a set of pure functions, used by both the
on-screen preview and the cast itself, so **what the preview shows is
exactly what happens**.

## 2.7 Application

`Application` is the whole game: the world, the turn loop and every screen.
It is a large class split across many files, one per system
(`ApplicationSurfaces.cpp`, `ApplicationTravel.cpp`, `ApplicationRime.cpp`
and others).

Extracting a separate `GameState` was considered more than once and
deliberately deferred. Nearly every system reads and writes most of the
world (the map, monsters, surfaces, light and the player), so splitting
the state would mainly add plumbing between objects that would still need
each other. Splitting by file keeps each system readable on its own and
keeps the coupling visible in one place. It is the codebase's biggest
compromise; see [known limitations](#known-limitations-and-future-work).

## 2.8 Saving

- **A plain text format**, written and read by `SaveGame.cpp` alone. The
  game is the only reader and writer, so a general serialisation library
  would only add a dependency. The file stays readable, which made save
  bugs easy to diagnose.
- **Versioned.** The format version (currently 47) is bumped for every
  layout change, and new fields are guarded with `version >= N` on both
  sides. Saves from version 9 onward load, migrated where needed; older
  saves are rejected rather than half-understood.
- **Append-only enums.** Monster kinds, statuses, landmarks, props and
  surfaces are stored as integers, so values are only ever added at the
  end. The loader bounds-checks each against the version that wrote it.
- **Validated.** A loaded character must be one the game could have
  produced: points spent match points earned, every talent's prerequisites
  hold, every item is legal. A save that fails isn't loaded. The tests
  check that real generated games always pass.
- **What isn't saved, on purpose:** the scheduler's exact energy and
  monster AI's internal state. Both correct themselves within a turn or
  two. A boss re-derives its phase from its life every turn, so there is
  nothing to go stale.
- **Floors.** Leaving a floor stores a snapshot of it (with the same state
  structure as a save, minus the character), and returning restores it.
  Inactive floors are paused: no off-screen battles, no loot farming.

## 2.9 Randomness

All combat randomness draws from one seeded stream, and every floor and the
loot stream get their own seeds from it. A run is reproducible from its
seed: the playtest bot takes `--seed`, and a bug found in a bot run can be
replayed exactly.

## 2.10 Testing

```
ctest                      every test
ctest -LE slow             everything but the long integration suite
```

- **Console tests** (19 programs) check the rules without a window: the
  formulas, the scheduler, FOV, A*, the dungeon generator, talents, AI
  behaviours, levelling and saves. Expected values are worked out by hand.
  The generator test flood-fills many seeds and checks that every floor is
  connected.
- **Application tests** (2 programs, about 1,200 checks) drive the real game
  through a hidden window: real input, real turns, real saves. They
  also write render snapshots to `build/rewards-checks/` for checking the UI
  by eye. Every feature lands with its checks.
- **The soak test** sets every monster on the newest floors hunting you for
  150 turns while you can't die. It is cheap insurance against crashes and
  hangs, and it found a real bug: spiders whose eggs hatched without limit.
- **Randomness in tests** is handled deliberately: damage checks accept
  "normal or critical", and comparisons average several hits. A check
  that passes only on a lucky roll is a bug in the check.
- **The playtest bot** plays whole runs and writes a report. It is used
  only as a crash and stuck check, never to judge balance: a bot's
  strategy says nothing about how a person plays. Balance is tested by
  hand, and with the sandbox's Max out and Travel tools.

## 2.11 Tools

- **The sandbox** (a separate start option): F1 spawns any monster or item,
  grants levels and points, respecs, maxes a character out, and travels to
  any depth of any dungeon. It never saves.
- **The Encounter Lab**: a fixed sequence of rooms (a warned heavy attack,
  archers behind cover with oil and a brazier, an ogre with a doorway to
  retreat through), with build presets and a session log. It exists to
  study whether the game is readable.
- **`tools/sfx_build.py`** builds the sound families from source
  recordings.

---

# Part 3: Decision log

The decisions that shaped the project, and the ideas that were replaced.

### 3.1 Foundations

| Decision | Alternatives | Why |
|---|---|---|
| Custom C++17 engine | Godot, GDExtension | A portfolio of architecture, not of using an engine |
| SFML 3 | SDL3, raylib | RAII types fit composition; C++17-native |
| CMake + FetchContent, SFML pinned | system packages | One command builds anything, reproducibly |
| Rules never see SFML, enforced by the build | discipline alone | A boundary that can't silently erode |
| Composition: monsters are data + behaviours | a subclass per monster | 48 kinds from 7 behaviours |
| Energy-based scheduler | fixed turn order | Speed as a real stat; standard in the genre |
| Shadowcasting FOV as a pure function | FOV inside the map | Testable in isolation; reused for monster sight |
| 4-directional A* | 8-directional | Monsters obey the player's own movement rule |
| Talents as a C++ table | JSON files | Data and logic separate without a loader and format |
| Text save format, versioned, validated | binary, JSON | Readable, dependency-free, and impossible to cheat by editing |

### 3.2 Game design

| Decision | Why |
|---|---|
| Permadeath by default, Adventure mode optional | Stakes give choices weight; Adventure opens the world to everyone |
| Telegraphed heavy attacks with a reaction window | Danger you can read is fair; unread damage felt random |
| Darkness and light as mechanics | Light becomes a resource and a weapon; monsters differ by sight |
| Combining surfaces | The battlefield becomes part of every build |
| Encounter budgets per floor | Uniform tiers made every floor feel the same |
| Floors from modules + procedural cells | Memorable set pieces without repetition |
| Landmarks and roaming threats | A playtest found floors "too easy and predictable" |
| The hunt instead of a hunger clock | Pressure to move on, without a chore |
| Forked trees with mastery ranks | The first, linear trees gave "no build identity" |
| Colours as the universal gate | One readable answer to "what is my build?" |
| Resonances hidden until a colour is held | Discovery, with a silhouette as a promise |
| Lore drops for deep trees | Hidden builds are found by going deeper and paying attention |
| Ascendancies gated by colours only | Colours already describe the build; any trial counts |
| One ascendancy per run | A real choice, not a collection |
| Scarce, deeper gear | Playtests found gear "too easy" |
| Show, don't tell | Players should understand from what happens, not from banners |
| Level cap 20, then 30 past the Lich | A cap of 30 everywhere let characters out-level the original game |

### 3.3 Ideas that were replaced

These matter as much as what stayed: each was a reasonable first answer
that play proved wrong.

- **Fixed classes with fixed kits → talent trees → origins.** The game
  began with one class (Spellblade) and its nine talents. It then had three
  classes with four talents each that unlocked more at set levels, then a
  class-and-hybrid system. Trees replaced the fixed kits, and finally the
  classless design made classes into origins. The old kits survive as test
  fixtures.
- **Baseline-10 attributes → no baseline.** Below 10 was a penalty, so
  starting stats felt like debt. Every point now counts from zero.
- **Specialisation → forks.** Specialising in a tree was removed: forks
  give each tree its choices instead.
- **Runes → nothing.** A rune system on gear was removed when the trees
  arrived; it duplicated what talents do.
- **The codex → locked trees → lore and the journal.** A codex that
  revealed hidden-tree conditions was removed as too explicit. Lore found in
  play replaced it, and the journal keeps what you find.
- **Class-gated ascendancies → colour-gated.** See 1.7.
- **Uncapped Grown Guard.** A bone guardian that grew with every kill would
  have scaled without limit; it grows with your Intelligence instead.

## Known limitations and future work

- **`Application` is very large.** It's split by file but is one class.
  The world state could be extracted once a system needs to be reused
  outside the game (a server, a replay viewer); until then the plumbing
  costs more than it saves.
- **Balance past floor 20 is untested.** Monster scaling is formula-based,
  so Rimeholt is simply harder. It needs hand playtesting.
- **The two other paths beyond the Lich**, the Sunless Court and the
  Stormspire, and the Hollow Throne (floors 31–40, level 40), are designed
  but not built.
- **Crafting** (salvaging junk into currency that rerolls affixes) is
  designed and deferred.
- **The game must be run from the project root**, where `assets/` is.
  Finding the executable's own directory needs platform-specific code that
  hasn't been worth it yet.
- **One save slot.** A save browser was never needed for a one-life game.
