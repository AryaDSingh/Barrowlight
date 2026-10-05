# Roguelike Engine

A custom C++17 roguelike engine (portfolio project), built with a
composition-over-inheritance architecture and SFML 3 for windowing/rendering.

See `ARCHITECTURE_DECISIONS.md` for what's been decided and why, and
`ROADMAP.md` for the planned build sequence and current progress.

## Current gameplay: talent trees (Prompt 35)

Classes retain their starting attributes and define the first tree's access pool.
Start with **1 tree point and 3 ability points**. Each level gained gives **2
attribute points and 1 ability point**; levels **5/7/9** also give a tree point.
There are seventeen trees (thirteen core/armour plus four hidden trees), four talents per tree, and three ranks per talent.
Cloth/Unarmoured, Light Armour and Heavy Armour join the ten original trees as
optional level-5+ unlocks. Their effects require matching equipment; see
[armour talents](ARMOUR_TALENTS_DESIGN.md) for rank values and interactions.

- **T** opens the tree browser. Up/Down selects a tree; Left/Right selects a talent.
- **Enter** unlocks or specializes the tree; **A** learns/upgrades the talent.
- **1-9** assigns the selected learned active to hotbar page 1; **Shift+1-9** assigns page 2.
- **T/Escape** returns to play and banks unspent points. The initial choice requires one tree and one learned ability.
- In play: **1-9** casts, **PageUp/PageDown** changes hotbar page, **Space** waits,
  and moving into an enemy performs the free basic attack. **B** equipment,
  **G** pickup, **I** inspection, **F5/F9** save/load.

Runes and the borrowed-class selection screen have been removed. Starting
weapon trees supply stat-free training equipment. Shields occupy the new off-hand
slot; remove them before equipping a two-handed weapon or bow (and vice versa).
Gear changes retain their one-turn cost. Tree purchases preserve cooldowns and
consume no turns.

**Saves now use version 14. Version 9-13 tree saves upgrade automatically; version 8 and older require a new run.** The complete rules,
content list and implementation boundaries are in `PROMPT_35_DESIGN.md`.
The Debug game build succeeds; this iteration has not been playtested or run
through the test suite. Existing fixtures were adapted to the removed rune/state
APIs, without adding new tests. Old verification results below describe earlier
checkpoints, not this implementation.

Stealth uses internal detection rolls: rank, effective DEX and distance help;
enemy DEX increases risk. Checks require line of sight within four tiles. Hover a
visible enemy while concealed to see its detection chance. One enemy spotting
you breaks concealment. At rank 3 and 6 DEX, low-DEX adjacent enemies detect you
5% of their active turns; archers are safer to approach from three tiles away.

### Defensive actions and combinations (Prompt 36)

**C: Cleanse** removes Poison, Burn, Chill and Marked: one turn, no mana,
eight-turn cooldown. **Marked** adds 25% to the next landed direct hit; Shield
Bash, Volley and Arcane Bolt apply it. Dodges and damage-over-time preserve it.
Active stuns cannot be refreshed. After a stun ends, actors receive one turn of
stun immunity; bosses limit stuns to one skipped turn and receive two immunity
turns. Inspection explains these limits. Guard retains flat direct-hit reduction.
Details and verification limits: `PROMPT_36_DESIGN.md`.

## Historical implementation notes

The prompt-by-prompt descriptions below record earlier versions. For current
class progression, controls, equipment slots and save compatibility, use the
section above and `PROMPT_35_DESIGN.md`.

## Layout

```
roguelike/
├── CMakeLists.txt             # build config; fetches SFML via FetchContent
├── ARCHITECTURE_DECISIONS.md  # what's been decided, and why
├── ROADMAP.md                 # planned build sequence + progress
├── NEXT_STEPS.md              # latest development checkpoint
├── src/
│   ├── main.cpp               # entry point -- deliberately trivial
│   ├── core/
│   │   ├── Application.hpp/.cpp        # owns window, map, roster, scheduler; input + level setup
│   │   ├── ApplicationTurns.cpp        # monster turns, AI decisions, death handling
│   │   ├── ApplicationProgression.cpp  # XP, level-up sequence, hybrid choice
│   │   ├── ApplicationRender.cpp       # HUD, map and menu-screen drawing
│   │   ├── ApplicationSave.cpp         # F5/F9: Application state <-> SaveGameState
│   │   ├── ApplicationTargeting.cpp    # mouse/keyboard aiming and previews
│   │   ├── ApplicationInventory.cpp    # bag/equipment screen and pickups
│   │   ├── ApplicationLoot.cpp         # chests and monster rewards
│   │   ├── ApplicationRunes.cpp        # rune management screen
│   │   ├── GameRules.hpp / PlayLayout.hpp  # shared constants / screen layout
│   │   ├── TurnScheduler.hpp/.cpp      # energy/speed-based turn order
│   │   ├── SaveGame.hpp/.cpp           # versioned save format -- SFML-independent
│   │   └── SoundManager.hpp/.cpp       # optional sound effects
│   ├── entities/   # Entity/Actor/Player/Monster/Item hierarchy and components:
│   │               # Stats, Inventory, TalentSet, StatusEffects, class talent
│   │               # kits (Warrior/Mage/Thief/Spellblade), HybridSpec,
│   │               # PlayerLeveling, AttributeFormulas, MonsterFactory/Tier,
│   │               # Item/LootGenerator (equipment + affixes), Rune (supports)
│   ├── ai/         # AIBehavior strategies: Chaser, Kiter, Support, AoEBomber,
│   │               # BossBehavior (Goblin Warlord), LichBehavior, NullAIBehavior
│   └── world/      # Tile, Map, FieldOfView (shadowcasting), ExploredMap,
│                   # Pathfinder (A*), DungeonGenerator, TalentTargeting
├── tests/          # one console test per system; see the list below
└── assets/         # font (DejaVu Sans Mono) and sound effects
```

There are twenty-three build targets: `roguelike`, twenty console tests
with no SFML dependency, and `application_targeting_test` / `application_rewards_test`
(hidden-window SFML integration tests that need a working graphics session). Run them
after building:

```bash
./build/bin/entity_smoke_test
./build/bin/turn_scheduler_test
./build/bin/fov_test
./build/bin/pathfinder_test
./build/bin/chaser_test
./build/bin/dungeon_test
./build/bin/talent_test
./build/bin/warrior_test
./build/bin/thief_test
./build/bin/mage_test
./build/bin/player_leveling_test
./build/bin/monster_tier_test
./build/bin/talent_unlock_test
./build/bin/hybrid_spec_test
./build/bin/monster_ai_test
./build/bin/boss_test
./build/bin/lich_test
./build/bin/savegame_test
./build/bin/attribute_formulas_test
./build/bin/targeting_test
./build/bin/application_targeting_test
./build/bin/application_rewards_test
./build/bin/roguelike
```
On Windows with the Visual Studio generator, substitute
`.\build\bin\Debug\<name>.exe`.

**Controls:** on launch, **1**, **2** or **3** to pick a class (Warrior,
Mage, or Thief). Arrow keys / WASD to move, **R** to regenerate the
level, number keys to use talents (all three base classes start with
1-4, growing to 1-6 as talents are unlocked through play -- see the
talent-list panel for what's currently bound), **F5** to save, **F9**
to load (reachable even before picking a class, to resume a previous
run), **Enter** to return to class selection after death or victory.
When an ability-choice screen appears (the hybrid path), number keys
pick an option and **0** declines where offered. On leveling up,
**1**/**2**/**3** spend an earned attribute point on
Strength/Dexterity/Intelligence -- shown as its own screen, once per
point, before the game resumes. Combat/status feedback is shown
**both** in the console and as an on-screen log in
the game window (see Prompt 13).

### Inventory and equipment (Prompt 32)

**B** opens the inventory. Use **Up/Down** to select one of the three equipped
slots or a bag item; **PageUp/PageDown** scroll longer bags. **Enter** equips
a bag item or removes equipped gear (**E** and **U** also work respectively).
**B/Escape** closes the inventory. **G** picks up one item at your feet;
visible ground items appear as diamonds (normal: white, magic: blue, rare:
gold). Three starter items appear at the first floor's entrance. Later
rewards come from enemies and chests (Prompt 33).

Viewing, selecting, comparing and cancelling are free. Successful pickup,
equip and removal each consume one turn, including normal cooldown/status
ticks, mana regeneration and enemy actions. Committing closes the inventory
so the combat response is visible. Empty pickups/slots consume nothing.

Equipment has **weapon, armour and charm** slots, with three fixed items per
slot. All classes can equip all items. The comparison shows base/current/
resulting Strength, Dexterity, Intelligence, maximum HP and maximum mana.
Gear attributes affect existing combat formulas; HP/mana bonuses are listed
separately. Armour grants its listed stats, without an additional damage
reduction mechanic. Replacing gear returns the old item to the bag.

Increasing a maximum never refills its pool; reducing it clamps the current
pool. Comparisons show values before the action's normal turn effects.
Equipment bonuses are recalculated from permanent character stats, so they
do not accumulate when swapping. Level-ups retain their existing full heal.

**Save format 8:** ground items, bag order, equipped items, rolled affixes,
unique instance IDs, permanent stats, current pools, loot RNG, chest claims,
talent IDs, ranks, cooldowns, trees and point balances persist in version 9. Older saves are rejected.
Owned gear travels between floors; uncollected items stay behind and are
discarded with the old floor. Floors 5 and 10 have no fixed pickups. **R**
remains a development shortcut that regenerates/heals/repopulates the floor;
ordinary progression uses doors.

### Loot and rewards (Prompt 33)

Every floor has one reward chest, shown as a gold rectangular box. Stand
on it and press **G**: opening costs one turn and puts its item directly
into your bag. Chests guarantee at least magic gear. Normal items have no
affixes, magic items have one, and rare items have two different compatible
stat affixes. **B** shows the rolled values and their effect on your stats.

Ordinary enemies have a 35%/50%/65% drop chance at Base/Elite/Nightmare tier,
capped at **two equipment drops per floor**. Each boss awards **two rare
items directly to your bag**. Summons give no loot or XP. Affix strength
increases mainly with floor depth, with elite/boss bonuses. A complete run
offers about 33-37 equipment items including starters; the initial balance
target is 6-10 worthwhile equipment changes, pending playtesting.

### Skill runes (Prompt 34, superseded)

Removed by the approved Prompt 35 tree design. Chain targeting and shared
ability-resolution mechanics remain available to tree talents. Equipment chest
and boss rewards remain; rune rewards are not replaced with extra ability points.

### Targeting and inspection (Prompt 31)

| Control | Action |
|---|---|
| 1-9, or click a talent in the sidebar | Select a talent; targeted attacks and movement enter aiming mode |
| Mouse movement, or arrows/WASD while aiming | Move the targeting cursor |
| Tab / Shift+Tab | Cycle valid visible enemies forward/backward |
| Enter / left-click on the map | Confirm the aimed action |
| Escape / right-click | Cancel aiming or keyboard inspection, spending nothing |
| Hover an enemy | Inspect its HP, attributes, speed, statuses, and abilities |
| I, or Tab outside aiming | Enter keyboard inspection; arrows move its cursor |
| I again | Leave keyboard inspection |
| PageUp / PageDown | Change the talent page; keys 1-9 refer to the displayed page |

Self-buffs and self-healing activate immediately. Outside aiming/inspection,
Escape still exits when the inventory is also closed. Aiming, inspecting, changing pages, and invalid casts
consume no turn. Each key press is one action; holding a key does not repeat
casts or movement. A dedicated sidebar leaves the map unobscured, with
28-pixel tiles and an 18-row viewport that fits the radius-8 field of view.

Quick Shot, Volley, Piercing Shot, Arcane Bolt, Ember Bolt, and Fireball
are player projectiles: their arrows stop at terrain or the first enemy.
Piercing Shot retains its existing critical-hit identity; it does not pierce
multiple actors. Volley/Fireball splash around the first impact. Other
targeted spells select a visible enemy directly. Area previews show the
same visible, walkable tiles used to find victims; splash does not add a
separate line-of-effect check from its center. Hidden enemies are neither
revealed nor damaged by player splash. Blink follows the aimed tile up to
its movement limit, stopping before terrain, occupants, or unseen tiles;
Vault Kick previews its retreat too. Red indicates a blocked/invalid path,
cyan the affected area, and blue a retreat path. The damage estimate includes
normal/critical outcomes, but a hit can still be dodged.

Enemy ability descriptions are public; remaining cooldowns are **unknown**.
The inspection API has an explicit reveal permission for a future perception
talent, but no Analyze talent is granted in this update. Hidden or remembered
enemies cannot be inspected even with that permission. Enemy AI targeting
is unchanged; this prompt introduces player targeting and inspection.

Selections are transient: load/regeneration cancels them. No save-format
change is needed because projectile definitions are rebuilt with the class
kit. The integration test writes its own save and screenshots under
`build/targeting-checks/`, without touching the player's normal save.

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
screen** -- press 1, 2 or 3 to choose. **Marauder** is the second
playable class: pure Strength (Str 20 / Dex 8 / Int 4), a tankier 45 max
hp than the Spellblade's 30, and a genuinely different resource rhythm
-- a free spammable basic attack (Slam, zero mana cost), a small
10-mana pool for Cleave (hits everything adjacent) and Rallying Cry (a
self-buff, not another damage number), and Berserker's Fury, which
spends the Marauder's own hp for the hardest single hit either class
has. Save/load now remembers which class is active and correctly
restores that class's own talent list before applying saved cooldowns.

As of Prompt 16 (Phase 2), **Archer** is the third playable class --
pure Dexterity (Str 14 / Dex 20 / Int 6), the exact opposite defensive
philosophy from the Marauder: the lowest hp of any class (24) paired
with the highest possible dodge chance (30%, the game's hard cap).
Ranged basics (Quick Shot, Volley) keep it out of melee range in the
first place, and **Vault Kick** is the signature move when that fails
anyway -- kick an adjacent enemy for modest damage, then vault several
tiles directly away from them in the same motion. The first talent in
the game that moves the caster as part of a damage-dealing effect, not
just a pure-movement talent like Blink.

As of Prompt 17, dying or defeating the boss now leads to a real
**end-game screen** -- red "You Died" or gold "Victory!" -- instead of
the window just closing or nothing happening at all. Defeating the boss
is, for the first time, an actual win condition rather than one more
kill among many. Press Enter from either screen to return to class
selection.

As of Prompt 18, the dungeon is roughly quadrupled in size (60x32,
10 rooms, up from 38x20/6) and the view now **scrolls with a camera**
following the player, clamped at the map's edges -- the old fixed
viewport that showed the entire dungeon at once is gone, replaced with
real exploration across a space genuinely bigger than one screen.

As of Prompt 19, the playable classes are restructured around three
pure **base classes**, one per attribute: **Fighter** (pure Strength,
originally named Marauder), **Sorcerer** (pure Intelligence, brand
new), and **Thief** (pure Dexterity, originally named Archer). The
original **Spellblade** (the Str+Int hybrid) still exists in full --
save/load still round-trips it correctly -- but is reserved for a
future unlock rather than offered as a starting option. **Sorcerer**
has the largest mana pool and lowest hp of any class, and its signature
**Mind Shatter** can Stun a target -- the first player talent to apply
a status effect to its target rather than the caster (previously, Stun
could only ever be inflicted *on* the player, by the Ogre).

As of Prompt 20, characters **level up** -- 1 to 10, XP from kills
(Goblin 10 up to the boss's 200), shown live on the HUD as "Level N
(X/Y XP)". Each level-up grows max hp/mana a little and fully heals you
-- a deliberate placeholder for now; what levels actually *unlock* is
real future design work, not decided yet. A single big XP grant (the
boss, especially against an early character) can cross several levels
at once in one go.

As of Prompt 21, the dungeon layout gained a handful of extra
**shortcut corridors** between nearby rooms (not just the strict
placement-order chain), so bigger maps read as one connected space
rather than a single winding hallway -- the boss room stays reachable
only via the full chain, on purpose. Monster damage across the roster
is also reduced roughly 20-33% (the boss less aggressively, around
25-30%, since it's still meant to be the hardest fight in the game) --
the original numbers were tuned before any leveling system existed, and
needed rebalancing for a genuine level-1 start.

As of Prompt 22, monsters now spawn in one of three **tiers** depending
on your character level -- Base (1-2), Elite (3-6, amber border,
"Elite" name prefix, hp x1.5/damage x1.4), and Nightmare (7-10, white
border, "Nightmare" prefix, hp x2.2/damage x1.8). Tougher monsters are
also worth more XP (x2 for Elite, x4 for Nightmare). The boss doesn't
scale with tier -- it's already the hardest fight in the game on its
own terms.

As of Prompt 23, Fighter/Sorcerer/Thief each **unlock two more talents
through play** -- one at level 4, one at level 7 -- growing from 4
starting talents to 6, timed to land right as Elite and Nightmare
monsters start appearing. Each new talent is an explicit upgrade on an
existing one (a wider Cleave, a stronger self-buff), not a reskin.

As of Prompt 24, Fighter and Sorcerer can **spec into a hybrid path**
at level 5 -- pick one ability from the *opposing* class's kit (a
Fighter draws from Sorcerer's spells, and vice versa), then one more
every level through 10. A fully-committed hybrid character ends up
knowing their entire original kit plus the entire opposing kit by level
10. Thief has no hybrid path (it shares neither of Spellblade's stats);
the boss, XP rewards, and monster tiers are all unaffected by this.

As of Prompt 25, the game has **sound** -- five effects (hit, death,
level-up, dodge, a UI select blip), all synthesized programmatically
(`assets/sounds/`, generated by a small numpy script, not sourced from
an external library) rather than downloaded, since this project's build
environment has no access to typical royalty-free sound sites. Sound is
never a hard requirement: it fails silently rather than crashing if no
audio device is available, verified directly against a real headless
environment during development, not just assumed. The very first
`cmake -B build` after pulling this update will take noticeably longer
than usual -- SFML's audio module now also fetches and builds its own
Vorbis/FLAC/Ogg from source, the same one-command, no-manual-
dependency-installation approach this project has used since Prompt 2,
just with a few more libraries to build the first time.

As of Prompt 26, the attribute system is **fully redesigned** --
Fighter is now **Warrior**, Sorcerer is now **Mage** (Thief keeps its
name), and there's no more "baseline 10" model at all. Classes start
with hand-picked HP/Mana (Warrior 30/10, Thief 25/15, Mage 20/20) and a
genuinely low 6/2/2 Strength/Dexterity/Intelligence spread, kept
strictly separate from those starting pools. Leveling now grants +1 HP
automatically plus **2 free attribute points** every level, spent
through a new on-screen choice -- Strength adds max HP, Dexterity adds
dodge and crit chance, Intelligence adds max mana, and each also scales
whichever talents declare that attribute as their own. Every hit,
player or monster, can now **critically strike** (5% base chance, +0.5%
per point of the attacker's Dexterity, for 1.5x damage). Thief's
Piercing Shot was reworked to lean into this: an inherent +20% crit
chance and +50% increased crit damage, on a longer cooldown. Monster
attributes haven't been rebalanced against the new formula yet --
functionally correct, not yet tuned.

As of Prompt 27, monster attributes **are** rebalanced -- every
Strength/Dexterity/Intelligence value below was recomputed to reproduce
exactly the same dodge percentages and damage totals already tuned
back in Prompt 21, not guessed from scratch. Spider and Archer's high
Dexterity (36 and 48) exists purely to hit their original 18%/24%
dodge under the new formula -- monster attributes aren't held to the
same "start low" philosophy player stats are, since monsters are
static and never grow through play. A nice side effect: that same
Dexterity now also gives them a real crit chance (23%/29%), which
happens to reinforce their nimble, precise identity rather than fight
it.

As of Prompt 28, the game has **10 floors** instead of one dungeon.
Floors 1-4 and 6-9 are pure "clear it, find the door" dungeons -- walk
onto the **red door** tile in the final room to move on. Floor 5 has
the Goblin Warlord; defeating him opens the door instead of ending the
run. Floor 10 is the true final fight -- for now it's the same Goblin
Warlord again as a placeholder, since the actual final boss (a Lich
that summons skeleton minions) is separate, not-yet-built work. The
current floor shows in the HUD under your level.

As of Prompt 29, floor 10's boss **is** the Lich -- the placeholder
above is gone. It fights entirely at range (bolt attacks, retreating if
you close in), and periodically raises a **Skeleton** minion instead of
bolting, up to 3 times per fight. Skeletons are individually weak but
block your path and add up if ignored while you focus the Lich itself.

| Enemy | Color | Behavior | Str/Dex/Int | Dodge |
|---|---|---|---|---|
| Goblin | red | `Chaser` -- plain melee | 6/0/2 | 0% |
| Spider | green | `Chaser` -- melee, applies Poison on hit | 4/36/2 | 18% |
| Ogre | brown | `Chaser` -- melee, chance to Stun on hit | 15/0/2 | 0% |
| Archer | tan | `Kiter` -- keeps its distance, shoots from range | 4/48/2 | 24% |
| Shaman | purple | `Support` -- never attacks; buffs a nearby ally instead | 1/0/16 | 0% |
| Bomber | orange | `AoEBomber` -- ranged area attack on a cooldown | 2/0/14 | 0% |
| Goblin Warlord | gold | `BossBehavior` -- 3-phase set-piece fight | 14/0/10 | 0% |
| Lich | pale teal | `LichBehavior` -- ranged, summons Skeleton minions | 4/20/16 | 10% |
| Skeleton | bone white | `Chaser` -- plain melee, the Lich's summoned minion | 5/0/0 | 0% |

## Building

### Linux (Debian/Ubuntu)

SFML needs a few system dev packages for windowing, graphics, and text
rendering. Audio needs nothing extra: `SFML_USE_SYSTEM_DEPS` is off, so
SFML builds its own audio codecs, and the Network module is disabled:

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

### Enemy warnings and encounter groups (Prompt 37)

Orange Bomber blasts show a cross: take two actions to escape. Red Ogre stun
strikes mark one tile: take one action to escape. Numbers show actions remaining;
stun, push or kill the caster to interrupt. Menus and skipped stunned turns do
not use your reaction window. Committed targets do not follow you.

New floors use small mixed groups and occasional elites within a floor budget,
independent of player level. Pending warnings survive saves. See
[PROMPT_37_DESIGN.md](PROMPT_37_DESIGN.md) for rules and pending runtime checks.

### Floor themes

Floors **1-3: Barracks** (warm stone), **4-6: Ruined Sanctum** (violet, broader
rooms), **7-10: Crypts** (cold teal, smaller chambers and an undead opening group).
The HUD names the region. Existing saves receive its palette; new floor layouts
and populations apply on generation. Optional vaults can appear on ordinary floors from floor 3.

### Ground targeting follow-up

Offensive abilities now accept empty visible ground; self-centered AoEs can
cast with no enemies nearby. Empty casts spend the usual action, resource cost
and cooldown. Melee adjacency, terrain, visibility, equipment and ability
prerequisites still apply. Projectiles still stop at the first visible enemy.
Gold outlines mark visible enemies actually affected; ground areas stay cyan.
Vault Kick can kick empty adjacent ground and retreat. No save-format change.
Runtime checks and old enemy-required targeting assertions need review when
verification is requested; no tests were added or run.

### Regional chest rewards

Barracks chests favour martial gear, Sanctum chests casting gear, and Crypt
chests charms. Favoured item bases have twice the selection weight; every normal
base remains possible. Chest rarity and affixes follow the existing rules.

### Optional vaults

A purple **V** marks a sealed side room. Stand adjacent and press **G** to read
its warning; **Enter** opens it for one turn. You can skip it or retreat.
Defeat the Elite Goblin and Archer, then press G at its cache to choose one of
three rare items. Arrows select, Enter claims, Esc defers. Choices and guard
progress survive saving. See [PROMPT_38_DESIGN.md](PROMPT_38_DESIGN.md).

### Boss warnings

Warlord: escape orange blasts in two actions and red enraged strikes in one;
heavy attacks leave one action of recovery. Lich: dodge bolts in one action;
purple rituals allow two actions to interrupt or occupy the spawn tile. It has
three total attempts, with no replacement summons. Boss state persists in new
saves. A legacy (version 9-12) Lich loads with rituals exhausted because previous
summon history was not stored. See [PROMPT_39_DESIGN.md](PROMPT_39_DESIGN.md).

### Town, returning and previous floors

- **H:** Waystone to town after ten turns out of combat (all characters start with it).
- **Z:** auto-explore toward the nearest unexplored area using discovered routes.
  Stops for enemies, damage, attack warnings, newly visible loot/chests/stairs/vaults,
  stepping onto a point of interest, or no reachable frontier. Any key/click or
  losing window focus cancels. Press Z again to acknowledge visible discoveries
  and continue. It never attacks, picks up loot, opens chests, or changes floors.
- **R:** fast wait; stops for danger or a key/click. After ten quiet turns, recover 5% maximum HP per turn (rounded up); rest waits for full recovery.
- **G on blue < stairs:** go back a floor. Downstairs offers descend/town/stay.
- **Town:** D resumes, R fully recovers, B opens equipment, Tab switches buy/sell,
  arrows select and Enter trades. Basic shop gear costs 30 gold.

Visited floors preserve enemies, loot and rewards. Travel grants no healing or
encounter reset. F5/F9 save/load the campaign. Older saves cannot recover floors
they never recorded; newly visited floors persist. Details and pending checks:
[TOWN_TRAVEL_DESIGN.md](TOWN_TRAVEL_DESIGN.md). Prompt 40 remains deferred.

### Mana balance follow-up

Combat mana regeneration is 1 per player turn; safe regeneration stays 2.
Combat uses the shared danger check plus attacks during the current turn.
Fire, Ice, Lightning and Arcane active spell costs are doubled at every rank,
including Blink. Arcane Efficiency applies afterward. Martial, Bow, Stealth
and Acrobatics costs are unchanged; free universal actions stay free. New
costs appear in the tree browser and live previews and apply to loaded builds.
No save-format change. Balance remains provisional pending playtesting.


### Codex and lore

Press **J** to read collected lore from any screen; arrows select a page, J/Esc
returns. In town, **J then L** consults the selected page's book or rumour.
Archers, naturally spawned Crypt Skeletons, the Warlord and the Lich each grant
a unique fragment on their first defeat. Summons do not grant lore.

Knowledge saves automatically to `codex.txt` in the launch directory, separately
from `savegame.txt`, and survives death/new characters. Codex knowledge grants no stats; hidden trees are purchased separately after
the current character meets their requirements. Errors appear in the journal; S retries saving.
A damaged or unsupported profile is preserved rather than overwritten.


### Hidden trees and extended progression

The run now reaches floor 20 and level 20. The floor-10 Lich opens the Deep
Crypts; floor 20 contains a stronger Lich encounter. Tree points continue every
odd level from 5 through 19. See HIDDEN_TREES_DESIGN.md for the four unlocks
and their combat rules. Hidden trees appear in T only when this character
meets their requirements; the Codex remembers discovered conditions.

- Spellblade: specialize a martial and a magic tree. Learn Imbue once; V in its
  tree node chooses a variant; B then 1-9 binds it (Shift for page 2). Variants share rank/cooldown.
- Animation: Lich's Ossuary Seal + specialized Arcane. Cyan skeletons are allies;
  walk into them to swap. They dissolve when travelling.
- Blood Magic: Blood Testament (Shamans/Warlord) + a specialized magic tree.
  Deathless readiness is shown with player statuses.
- Shadow Archer: specialize Bow and Stealth; requires a bow.

Format-15 saves retain allies, relics and Deathless usage. Versions 9–14 migrate,
including adding an exit to legacy floor 10. New characters retain only Codex
knowledge. The extended band and new talents have not been playtested.


### Boss pressure and curses

The Warlord advances between blasts and uses a two-action area cleave while
enraged. Lich rituals summon skeletal guards and an archer, with three attempts
per fight and no summon rewards. New fights have increased boss HP.

The Lich's Grave Hex requires clear sight within six tiles. While healthy it
uses Mana Drain (2 mana per status tick); at 60% HP or less it uses Doom
(delayed HP damage). **C: Cleanse removes either curse.** Doom's HUD countdown
shows the remaining actions and pending damage; Guard and dodge do not prevent
that damage. Healing can prepare for it. Existing curses persist when sight is
broken and prevent rest/auto-explore until removed or expired.


### Mouse controls � first UI pass

- Left-click visible ground to take one cardinal step toward it (horizontal on
  equal diagonals). No queued movement or pathfinding; walls do not spend a turn.
- Left-click an adjacent enemy for Basic Attack. Distant enemy clicks do not
  move you into combat. Select a talent first to use its normal aiming rules.
- Hover enemies freely. I/Tab enters explicit free inspection; Esc/right-click
  leaves that mode. Move into the inspection panel and use the wheel to read
  longer descriptions. Hidden and dead enemies never expose details.
- Talent rows highlight on hover/selection. Click the < / > buttons to change
  the hotbar page without spending a turn; keyboard controls remain available.


### Bottom hotbar and status details

- Abilities now occupy nine bottom slots per page. Click a slot or press 1-9;
  < / > or PgUp/PgDn switches pages. Slots show a shape symbol, abbreviated
  name, rank, and cooldown or resource cost. Hover for the full ability details
  and any reason it cannot currently be used.
- Status badges sit directly below the map. Hover for effect, duration and
  Cleanse guidance. More than nine effects can be paged with the wheel or arrows.
- The combat log shows the latest two messages above the hotbar. Scroll over
  that strip to read earlier retained messages; hover a line for its full text.


### Expanded inventory controls (2026-09-27)

B opens the 50-item bag and eleven equipment slots. Hover compares item bonuses.
Right-click equips/removes; drag a bag item onto a compatible slot, or drag
worn gear into the bag to remove it. Rings fit either ring slot. Scroll the
wheel or click Previous/Next to change bag pages. Keyboard arrows and Enter
still work; D drops the selected bag item in the dungeon. Equipment changes
and drops consume one dungeon turn; town equipment changes are free.

Armour talents use the majority type across head, body, hands and feet.
Empty pieces count as cloth; tied majorities favour the body piece. Existing
characters with only heavy/light body armour can therefore change armour type.
Old bags and guaranteed boss rewards retain overflow rather than losing items.
Save format 18 retains compatibility with versions 9-17.


### Talent menu mouse controls (2026-09-27)

In T, click a tree or talent to select it without spending points. Use the
Unlock/Specialize or Learn/Rank up button to buy; costs and prerequisite reasons
are displayed. Scroll over the tree list or use Previous/Next tree to browse.
Assign hotbar shows both pages and their current bindings: click the slot to
replace, or Cancel to back out. Spellblade Imbue supports Next element before
assignment. Continue returns to play after the initial build choices are met.
Existing keyboard shortcuts remain available.


### Class and attribute mouse controls (2026-09-27)

Click one of the three class cards to start that class. During level-up, click
Strength, Dexterity or Intelligence to spend one attribute point. Both screens
retain their 1-3 keyboard shortcuts, and class attributes and stat bonuses are
unchanged.


### Town shop mouse controls (2026-09-27)

In town, click Buy or Sell, select an item row to preview it, then click the
separate trade button to commit. Previous/Next and the mouse wheel change stock
pages. The Inn, Equipment and Return to floor buttons mirror R, B and D. Prices
and trade rules are unchanged; keyboard controls remain available.


### Travel and vault mouse controls (2026-09-27)

The descent menu has clickable Descend, Return to town and Stay buttons. Vault
warnings offer Open and Leave sealed. Click a reward card to select it, then
Claim to take it, or Decide later to return for it. Opening and claiming cost
one turn; viewing and cancelling are free. Existing keyboard shortcuts remain.


### Codex mouse controls (2026-09-27)

Click Codex/books in town, or press J, to open the Codex. Click a family card,
use Previous/Next, or scroll the wheel to browse. The Close, Save knowledge and
Consult town book/rumour buttons mirror the existing keyboard commands.
Consulting a source is free and requires being in town.


### Dungeon action bar and restart (2026-09-27)

The buttons above the map open Bag, Trees and Codex; interact with nearby loot,
vaults or stairs; wait; rest; explore; return to town; cast Cleanse; and save or
load. Gameplay actions keep their normal turn costs and requirements. Aiming
shows Cancel. During rest or exploration, the first click stops the activity.
Death/victory screens offer New character and Codex, and class selection offers
Load and Codex. Keyboard shortcuts remain available.


### Selectable dungeon depths (2026-09-28)

In town, click **Choose dungeon** or press **M**. Pick The Ruins or Deep Crypts,
select depth 1-10, then Enter. Arrows also select; Esc returns to town. Enemy
strength and loot scale with dungeon depth, independently of character level.
Visited floors retain their enemies and loot. Choosing your current depth resumes
your location; another visited depth uses its entrance. No entry healing or turn
cost. Save version 20 accepts versions 9-19. See DUNGEON_SELECTION.md for balance
and migration details.

### Death modes (2026-09-28)

Before choosing a class, click the mode button or press **M**:

- **Roguelike** (default): one life.
- **Adventure**: two extra lives. After death, **R** or the revive button returns
  you to town with full recovery, spending one life. Keep your gear and progress;
  enemies and loot stay as you left them. Your return position moves to the floor
  entrance (or a nearby free tile). Deathless triggers before revival is needed.

The HUD shows mode and remaining extra lives. No lives are earned at milestones
in this first pass. Save version 21 preserves mode/lives and reads versions 9-20;
older characters remain Roguelike. This feature has not been built or tested yet.
