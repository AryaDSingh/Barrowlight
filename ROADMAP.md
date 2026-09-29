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

## ✅ Prompt 5 — Map and rendering
Tile/Map class rendering to screen, hardcoded test map (no procgen yet).
Basic player movement via keyboard input, to confirm rendering + input +
turn scheduler are integrated correctly. Tiles/player render as flat
colored rectangles (no art assets yet), 20×10 ASCII-art test room, arrow
keys/WASD move the player with wall collision, each move routed through
the real `TurnScheduler`. **Confirmed on the person's machine:** window
opens, grid + player render correctly, movement works, wall collision
(border and interior pillar) actually stops the player rather than
passing through. See `ARCHITECTURE_DECISIONS.md` → "Map, rendering, and
input."

## ✅ Prompt 6 — FOV
Field-of-view (recursive shadowcasting or similar); dimmed "remembered but
not visible" state for explored-but-out-of-sight tiles. `Tile` gained the
`transparent` flag deferred from Prompt 5. `computeFieldOfView()` is a
pure function (no Map mutation, no player/entity knowledge); `ExploredMap`
separately tracks Hidden/Remembered/Visible over time. Verified via a
standalone `fov_test` printing an ASCII grid against a known wall pillar
— confirmed correct shadow-casting behavior including top/bottom symmetry
and vision wrapping around obstacle corners, not just "it compiled."
**Confirmed on the person's machine:** darkness beyond sight radius,
dimmed remembered tiles, and pillar shadow all behaving correctly. See
`ARCHITECTURE_DECISIONS.md` → "Field of view."

## ✅ Prompt 7 — Pathfinding
A* pathfinding for monster movement toward a target, and a basic
AIBehavior strategy class (Chaser) that uses it to move toward the player
when in range. `AIBehavior` is now a true abstract class (`decideMove()`
pure virtual) -- the moment flagged back in Prompt 3. `findPath()` is
4-directional, matching player movement, no diagonals. `Chaser` reuses
`computeFieldOfView()` (Prompt 6) to gate whether it can even see its
target, rather than a raw distance check. One deliberate scope extension
beyond the bare prompt text: an actual goblin (Chaser AI) is now spawned
in the real game, not just proven via standalone tests -- Prompt 5 had
already left the exact hook for this. Verified with unusual rigor given
three interacting non-trivial algorithms: `pathfinder_test` (ASCII grid,
hand-traced a forced 21-tile detour around a wall, matched exactly),
`chaser_test` (hand-computed step-by-step chase sequence, matched
exactly, including correctly stopping at adjacency), plus the two earlier
tests fixed to use the new `NullAIBehavior` instead of instantiating the
now-abstract `AIBehavior` directly. **Confirmed on the person's machine:**
the goblin correctly chases and stops at adjacency in the live game.
See `ARCHITECTURE_DECISIONS.md` → "Pathfinding and Chaser AI."

## ✅ Prompt 8 — Procedural dungeon generation
Rooms + corridors generator producing a connected, playable Map. Quick
regenerate-with-a-keypress debug loop. Random non-overlapping rooms
connected in placement order by L-shaped corridors -- guarantees full
connectivity by construction (like a linked list), no separate graph
pass needed. Deterministic given a seed: fixed seed at launch, fresh
random seed on the regenerate key (**R**). `parseAsciiMap` deliberately
kept (not replaced) for the existing hand-crafted test maps in
`fov_test`/`pathfinder_test`/`chaser_test`, which need deterministic
layouts to hand-verify against -- procedural generation would undermine
that. Verified with real rigor: `dungeon_test` flood-fills from player
start across **10 different seeds**, confirming 100% of floor tiles
reachable in every one (the one property that actually matters for "a
connected, playable Map"), not just eyeballing one printed layout.
Verified end-to-end in the live game too, including the regenerate key
itself -- installed `xdotool` and sent real key events to the actual
SFML window under a virtual display, producing two consecutive
successful regenerations with different seeds/layouts each time, not
just trusting the code path. Map now exceeds the old fixed test room's
size (38x20 vs 20x10) but still fits the current window without
scrolling -- camera/viewport is flagged as the next real gap.
**Confirmed on the person's machine.** See `ARCHITECTURE_DECISIONS.md` →
"Dungeon generation."

## ✅ Prompt 9 — One playable class + talents
Design + implement one playable class, 6–8 talents across 2 talent trees,
with real trade-offs (cooldown vs damage, AoE vs single-target, risk vs
reward) rather than flat power increases. Talent/TalentSet system first,
then this class's talents as data. **Result:** the **Spellblade** -- 8
talents across Blade (melee, must be adjacent, cheap/efficient) and Flame
(ranged/AoE, mana-hungry) trees. `TalentSet` fully replaced its Prompt-3
stub. `AIBehavior`, `Talent`, `TalentSet`, and `TalentEffects` each stay
focused on one job (interface / data / cooldown bookkeeping / damage
application); Application resolves targeting since it's the one thing
that knows about every Actor in the level. One deliberate scope
extension, flagged explicitly: added minimal flat-damage application (not
a general combat formula system) so talents are actually testable in the
live game, not just inert data -- goblin now has real hp (25) and can
die. All 8 talents are instant-effect only; no ongoing buffs/debuffs
(`StatusEffects` stays a stub, still Prompt 10's job). Talent data is a
C++ table, not an external file -- same file-I/O-risk reasoning as the
ASCII test maps. Verified with real rigor: `talent_test` hand-computed
damage/cooldown/conditional-multiplier values, all matched exactly. Full
live end-to-end test via `xdotool`: computed a genuine A*-pathfound route
to the goblin for the fixed seed, walked it in the real window, and cast
three different talents in sequence -- damage numbers (6, then 16, then a
correctly-triggered 3x execute for 30) matched the talent data exactly,
followed by a correct death message and confirmed post-death behavior
(dead target correctly unattackable by both melee and ranged talents,
game stable afterward). **Confirmed on the person's machine:** the kit
feels right in actual play. See `ARCHITECTURE_DECISIONS.md` → "Talent
system."

## ✅ Prompt 10 — Enemy roster + status effects
5–8 enemy types with genuinely distinct AI (ranged kiter, melee rusher,
support/buffer, AoE threat) via the AIBehavior strategy pattern. Whatever
StatusEffect types their kits need (poison, stun, etc). **Result:** 6
enemy types from just 4 `AIBehavior` classes (`Chaser` extended with
attack + on-hit effects → Goblin/Spider/Ogre; new `Kiter` → Archer; new
`Support` → Shaman; new `AoEBomber` → Bomber) -- the founding "data +
which behavior gets plugged in, not a new subclass" philosophy from
Prompt 0, finally proven with a real roster. `StatusEffects` fully
replaced its Prompt-3 stub: Poison, Stun, Empowered, applied via a new
`tickStatusEffects` free function mirroring `TalentEffects`' separation.
`AIBehavior` gained its long-anticipated second verb (`decideAction`
returning `AIDecision`, not just a move) -- monsters can finally attack.
This forced the "game state" refactor flagged as overdue since Prompt 5:
`Application` now owns a `vector<unique_ptr<Monster>>`, not one
hardcoded goblin. `DungeonGenerator` now exposes every room's center so
the roster can populate a level (deterministic given seed, one enemy
type per room, up to 6). Player death is now handled (message + clean
window close; no game-over screen yet -- Prompt 11/12). Monster
abilities reuse the *existing* `Talent`/`TalentSet` from Prompt 9 rather
than a parallel system. Verified with real rigor: `monster_ai_test`
(17 checks) covers `tickStatusEffects` (poison damage/expiry, stun
detection) and Kiter/Support/AoEBomber decision logic against
hand-computed values -- all passed, including catching and fixing two
bugs in the *test's own* distance assumptions, not the code under test.
Live end-to-end via `xdotool`: real A*-computed walks to multiple
enemies produced a full two-way combat exchange with every damage number
matching the data exactly (including the Goblin's own AI closing
distance and attacking *before* the player did), correct Poison
application/refresh-not-stack behavior, a genuine multi-enemy encounter
that killed the player, and confirmed the window closes itself cleanly
on death. **Confirmed on the person's machine:** the full roster plays
correctly in real combat. See `ARCHITECTURE_DECISIONS.md` → "Enemy
roster and status effects."

## ✅ Prompt 11 — Boss + integration pass
One boss encounter as a set-piece finale. Full playthrough start to
finish, fix what's broken or feels bad. **Result: Goblin Warlord**, a
3-phase fight (aggressive melee → cooldown-gated AoE blast while keeping
distance → one-time self-Empower then all-in enraged melee) built from
one new `BossBehavior` class, reusing the existing `TalentSet` (Prompt 9)
and `Empowered` status effect (Prompt 10) rather than new mechanics.
`AIDecision` gained `SelfBuff` and an optional `announcement` string for
phase-transition flavor text, generic enough that `Application` needs no
boss-specific knowledge. Placed in a deliberately larger room at the end
of the room chain (not the start) -- required real empirical tuning, not
just implementation: the first parameter set only found space ~6% of the
time; swept parameters properly and landed on one that succeeds ~97-98%
across 300 seeds. The integration-pass audit -- genuinely reviewing the
whole system, not just adding the boss -- **found and fixed a real bug**:
movement only ever checked terrain walkability, never whether another
actor already occupied the destination tile, so players and monsters
could walk directly onto each other. Never surfaced earlier because every
scripted test happened to stop *adjacent* to targets, never step onto
them. Also refactored three duplicated small geometry helpers (Chaser/
Kiter/AoEBomber) into a shared `AIUtils.hpp` once a third independent
copy (for the boss) made the duplication worth removing. Verified
extensively: `boss_test` (10 checks) confirms every phase threshold,
transition-announced-once behavior, and per-phase decision against
hand-computed values. Live via `xdotool`: confirmed the occupancy fix
directly (a scripted approach got physically blocked by a Goblin that
had moved to intercept, then an immediate attack found it adjacent,
proving the block was a real actor, not a wall) -- and, investigating an
unexpected mid-test block, discovered the *boss itself* had proactively
closed distance once the player entered its sight radius, confirming
Phase 1 behavior is genuinely wired end-to-end in live play (not just
unit-tested): its Ember Bolt response and the boss's counter-attack both
matched `MonsterFactory`'s configured damage numbers exactly, ending in
an honest player death (already critically wounded from the earlier
gauntlet, not a bug) with the window closing cleanly as designed. Phase
2/3 were not observed live in this session (would need a full-health
playthrough reaching the boss) but are hand-verified via `boss_test`, and
Phase 1's live confirmation gives strong confidence the same
already-proven mechanisms (attack/ability/self-buff execution) carry
through correctly. **Confirmed on the person's machine.** See
`ARCHITECTURE_DECISIONS.md` → "Boss encounter and integration pass."

## ✅ Prompt 12 — Save/load + polish
Save/load for game state. Final polish pass (UI, menus, distributable
build) scoped to time remaining before publishing. **Save/load result:**
a hand-rolled, human-readable text format (`SaveGame.hpp/.cpp`) --
deliberately not JSON/binary, same one-dependency-only reasoning as every
other format decision in this project. Entirely SFML-independent, so
genuinely unit-testable without a window: `savegame_test` round-trips a
hand-built state and checks every field individually (map, fog-of-war,
all 8 talent cooldowns, status effects, a regular monster and the boss
with its own effects), plus missing/corrupt-file handling -- 15 checks,
all passing. Required a real prerequisite: `Monster` didn't remember its
own `MonsterType` after construction, so the enum moved into its own
header (`MonsterType.hpp`, avoiding a circular include with `Monster.hpp`)
and every construction site across the codebase was updated. Bound to
**F5**/**F9**. **Live cross-process testing caught a real bug before it
shipped**: loading initially copied `regenerateLevel()`'s turn-advancing
step, which would have silently applied one unintended extra
status-effect tick to an already-poisoned loaded player (fine for a
freshly generated player, who never has any status effects yet -- not
fine for a loaded one who might). Fixed, then confirmed live: hp restored
as exactly 20/30 (not 18), and a cooldown showed exactly "3 turns left,"
matching the established tick-immediately-after-start semantic from
Prompt 9 precisely. **Confirmed on the person's machine.**
**Polish-pass prioritization:** no hard time limit before publishing, so
this became a genuine "next steps" planning exercise rather than a
scoped-down triage -- see "Phase 2" below, drafted and partly decided
(class naming, symmetric dodge) rather than executed yet. See
`ARCHITECTURE_DECISIONS.md` → "Save/load."

---

# Phase 2 — Beyond the vertical slice

The original 12-prompt sequence (above) is complete: a working vertical
slice with one class, a 6-enemy roster, a boss, and save/load. This
section is a newly drafted continuation, not yet executed -- started
because there's real time available and a genuine interest in going
further, specifically: a proper UI, and multiple playable classes on a
Path of Exile-style Strength/Dexterity/Intelligence triangle.

Two things worth being upfront about, since they reshape what "just add
classes" actually means:

1. **There's no text/font rendering anywhere in this engine.** Flagged
   as a deliberate simplification since Prompt 5, re-flagged as
   "genuinely limiting" since Prompt 10. A class-selection screen,
   talent tooltips, a readable HUD, end-game screens -- none of it works
   with colored rectangles alone. This is the actual prerequisite for
   everything else in this phase, not a side item.
2. **`Stats::strength` and `Stats::dexterity` have existed since Prompt 3
   but nothing has ever read them.** Every talent/monster attack is a
   flat number baked into its own data (see ARCHITECTURE_DECISIONS.md,
   "no general combat formula system... only worth building if the
   numbers-as-data approach stops feeling sufficient"). A triangle of
   classes that are supposed to feel different *because of* their
   attribute spread only works if attributes actually do something --
   so this phase means finally building that formula layer, not just
   adding more talent tables.

The existing **Spellblade already occupies the Str+Int hybrid corner**
of the triangle (melee Blade tree + magic Flame tree -- what PoE calls a
Templar). No need for a redundant second hybrid; the new classes should
fill the other two corners.

## ✅ Prompt 13 — Text rendering + HUD upgrade
Source and bundle a permissively-licensed monospace font (likely via
apt's `fonts-dejavu-core` or similar, copied into `assets/`); build
minimal SFML text-drawing capability -- a small helper, not a new
abstraction layer. Replace the current colored-bar-only HUD with real
readable labels: hp/mana as "23/30" text, talent names + cooldowns as a
readable list instead of only console output, an on-screen combat log
(last few messages) instead of console-only. No new gameplay, no new
classes yet -- purely the infrastructure + presentation upgrade
everything else in this phase depends on, matching the "prove the
pipeline first" discipline of Prompts 2 and 5. **Result:** DejaVu Sans
Mono, sourced via apt (`fonts-dejavu-core`), copied into
`assets/fonts/` with its license file -- confirmed via search first
(Bitstream Vera-derived, explicitly permits bundling within a larger
software package). Confirmed SFML 3's exact text API before writing
code (`sf::Font::openFromFile`, not `loadFromFile`; `sf::Text` requires
a font reference in its constructor, no default) -- matched on the
first real build attempt. Every one of the 23 existing `std::cout` call
sites scattered through `Application.cpp` now goes through one new
`log()` template method instead, which both prints to console exactly
as before (so every prior console-based verification technique in this
project still works unchanged) and feeds a capped 6-message rolling
on-screen log. New HUD: hp/mana as readable numbers next to their bars,
a talent-list panel showing all 8 talents' live names and cooldown
status (dimmed while on cooldown), the boss bar gained a name label.
**New verification technique introduced this prompt**: used
ImageMagick's `import` to screenshot the actual live SFML window under
Xvfb and visually inspected the images directly -- the first time in
this project's history that rendering correctness was confirmed by
actually looking at it, rather than inferred from console output or
hand-traced geometry. This caught a real (if minor) issue no amount of
code review would have: the talent-list text initially had no visual
separation from the dungeon tiles behind it -- legible, but genuinely
unfinished-looking. Fixed with semi-transparent backing panels behind
the talent list and combat log, then re-screenshotted to confirm the
fix actually looked better, not just assumed it would. Screenshots also
confirmed, precisely: hp/mana text tracked real combat state exactly
(e.g. "15/30" hp, "11/20" mana matching the log), a talent correctly
shown dimmed with its exact remaining cooldown while others showed
`[Ready]`, the on-screen log's last 6 lines exactly matched the console
tail in the right order, and a fresh game start showed no stray
empty-log panel with every talent correctly `[Ready]`. **Confirmed on
the person's machine.**

Playing on the new HUD surfaced two real combat-pacing issues, fixed
the same session, both **confirmed on the person's machine**: bumping
into an adjacent monster was a silent no-op identical to bumping a
wall, starving monsters of turns they were owed (see
`ARCHITECTURE_DECISIONS.md` → "Combat pacing"); and the player had no
mana regeneration at all, now +2 per turn, capped at max.

## ✅ Prompt 14 — Attribute-driven combat formulas
Add `Intelligence` to `Stats` (joining the existing `strength`/
`dexterity`, both currently decorative). Design and implement real
formulas connecting attributes to combat: Strength → bonus physical
damage (+ some hp), Dexterity → dodge/evasion chance, Intelligence →
bonus magic damage + max mana scaling. **Decided: dodge is symmetric --
monsters roll it too, using the same formula, not just the player.**
That only means something if monsters have real attributes behind it,
so `MonsterFactory`'s stat blocks need genuine per-type Strength/
Dexterity/Intelligence, not left at `Stats`' undifferentiated defaults
(every monster currently has strength=10/dexterity=10, identical,
regardless of type) -- e.g. a tanky Ogre leaning Strength-heavy/
Dexterity-light (hits hard, rarely dodges), an Archer leaning Dexterity
(dodges more, fitting an "hard to pin down" identity), Shaman/Bomber
leaning Intelligence to match their already-magic-coded kits. Tag both
`Talent` *and* `MonsterAttackProfile` with a `DamageType`
(Physical/Magic) so the right attribute bonus applies on either side,
not just the player's -- the same symmetry principle as dodge. Exact
numbers for all of this aren't decided yet, on purpose (see below).
Recalibrate the Spellblade's existing 8 talents' numbers under the new
formula -- this changes established, tested balance, so it needs its
own careful pass, not a silent side effect. Foundational systems work,
not new content -- closes the long-flagged "no combat formula system"
gap from Prompt 9.

**Result:** a new `AttributeFormulas` module (pure, fully unit-tested):
Strength/Intelligence each give ±1 damage per 2 points from a 10
baseline (can go negative below baseline -- a genuinely weaker hit, not
just "no bonus"); Dexterity gives +3% dodge per point, floored at 0%,
capped at 30%; Intelligence gives +1 max mana per point, floored at 0.
`physicalDamageBonus`/`magicDamageBonus`/`dodgeChance`/
`manaBonusFromIntelligence` are all pure and exhaustively tested
(`attribute_formulas_test`, 25 checks including boundary/cap behavior
and a floating-point-comparison bug caught and fixed mid-session: a
computed 0.3 isn't guaranteed bit-exact to the literal `0.30f`, unlike a
value that hits the cap and gets clamped to that same literal). The
actual dice roll (`rollDodge`) is deliberately not unit-tested in
isolation -- same precedent as Chaser's `onHitChance` (Prompt 10),
which was never isolated either; `didDodge`, the pure comparison it's
built on, gets the exhaustive testing instead. **A real landmine caught
before it shipped**: `SpellbladeTalents.cpp` constructs every `Talent`
via positional aggregate initialization (the `/*name=*/`-style comments
are just comments, not designated initializers), so inserting
`damageType` in the natural spot mid-struct would have silently shifted
`conditionalMultiplier` (an int) into `conditionalHpFraction`'s old
slot -- compiles cleanly via an implicit int-to-float conversion, no
error, just quietly wrong. Moved `damageType` to the very end of
`Talent` instead, after confirming `MonsterAttackProfile` and
`AIDecision` were safe (all their construction sites use named-member
assignment or empty-brace init, never positional). **Every existing
tuned damage number was recalibrated, not just left to drift**: each
talent/attack's base `power` was reduced by exactly its new attribute
bonus, so live damage output is bit-for-bit identical to what Prompts
9-11 already tuned and tested -- confirmed directly, not assumed:
`talent_test` uses an attacker with the Spellblade's real recalibrated
stats (strength 14, intelligence 18) and asserts the *exact* original
damage numbers (Quick Strike 6, Execution's tripled 30, Ember Bolt 7)
still come out unchanged through the new formula, plus a second check
using a baseline-attribute attacker to confirm the bonus genuinely
scales rather than being a fixed pass-through. Live play confirmed the
same thing end-to-end in real combat: every logged damage number
(Power Strike 16, Ember Bolt 7, Quick Strike 6, Goblin's 5, Spider's 3)
matched hand-computed expectations exactly. Player recalibrated to
Strength 14 / Dexterity 10 / Intelligence 18 (the Str+Int hybrid corner
of the triangle); `maxMana` is now computed from the real formula (12
base + 8 from Intelligence) rather than hardcoded, landing on exactly
20 -- confirmed via a live screenshot showing "20/20" at game start.
Every monster type got genuine, thematically distinct attributes
instead of universal defaults: Ogre (Str 18/Dex 6, hits hard, 0%
dodge), Spider (Dex 16, 18% dodge) and Archer (Dex 18, 24% dodge, the
roster's most evasive) leaning into evasion, Shaman/Bomber leaning
Intelligence (18/16) to match their already-magic-coded kits, the boss
balanced across both offense stats (Str 16/Int 14) for its melee and
blast phases respectively. `monster_ai_test`/`boss_test` needed zero
changes -- confirmed, not assumed, by building and running them
unmodified -- since both construct their `AIBehavior`s directly with
test-local parameters rather than through `MonsterFactory`. See
`ARCHITECTURE_DECISIONS.md` → "Attribute-driven combat."

## ⏳ Prompt 15 — Multi-class system + Marauder (pure Strength)
`PlayerClass` enum + `PlayerClassFactory`, mirroring `MonsterFactory`'s
already-proven pattern exactly. A class-selection screen (using Prompt
13's text rendering) shown before the dungeon generates. **Marauder**:
pure Strength, no meaningful mana pool -- resource identity is HP, not
mana (leaning further into the blood-magic precedent Reckless Lunge
already set). Concept sketch, not final numbers: a Cleave (AreaAroundSelf,
hits everything adjacent -- meaningfully AoE now that there's a full
roster to hit), a self-Empower "Rallying Cry" (reusing the existing
Empowered status effect), a high-risk guaranteed-hit "Berserker's Fury"
that costs hp for massive damage, and a no-cost spammable basic strike
(cooldown-only, no resource at all -- a genuinely different economy from
Spellblade's mana-juggling).

**Result:** `PlayerClass` (Spellblade/Marauder) + `PlayerClassFactory`
(`statsForClass`/`talentSetForClass`), the exact `MonsterFactory` shape
applied to player classes. A real class-selection screen -- a plain text
menu via Prompt 13's `drawText`, keys 1/2 -- shown on launch; the
constructor no longer auto-starts as the Spellblade. **Marauder's
4-talent kit**, deliberately smaller than the Spellblade's 9: Slam (0
mana, 1-turn cooldown, spammable), Cleave (AreaAroundSelf, hits
everything adjacent), Rallying Cry (SelfBuff -- applies Empowered, the
same status effect the boss's own enrage already uses), Berserker's Fury
(8 hp cost, 0 mana, the hardest single hit in either kit). Str 20 / Dex
8 / Int 4, hand-tuned maxHp 45 (tankier than the Spellblade's 30 --
Strength giving a dynamic hp bonus was deliberately deferred at Prompt
14, so this is expressed as direct data instead), maxMana 10 from the
real formula (10 base + 0 from Intelligence, floored).

**A real `Talent` refactor**: the lone `bool isHeal` (Prompt 14's
healing addition) became a proper `TalentEffectKind` enum (Damage/Heal/
SelfBuff) once a third kind -- needed for Rallying Cry -- made
generalizing worth it rather than bolting on a second flag, mirroring
`AIDecision`'s own discriminated-by-enum shape.

**Three real bugs this prompt, all found through live play, not code
review:**

1. **Rallying Cry granted Empowered correctly but did nothing to the
   caster's own damage.** `executeAIDecision` (monster attacks) already
   checked for Empowered; `applyTalentDamage` (every player attack)
   never did. Every damage number in a live Marauder fight matched the
   *un-buffed* formula exactly, which is what surfaced it -- not a
   passing unit test, since no existing test exercised an attacker with
   an active status effect. Fixed, then caught a second bug while
   writing the regression test for the first: the test's own attacker
   Stats had been left at default strength (10) instead of the
   Marauder's real 20, which would have made the fix look wrong when it
   wasn't -- traced by hand-checking the arithmetic before assuming the
   code was broken.
2. **`F9` (load) was only reachable once already in Playing mode** --
   meaningless for a person who launches fresh and wants to continue a
   previous run, since the game now starts at class selection instead of
   auto-starting as the Spellblade. Fixed by handling `F9` before the
   mode branch, reachable from either screen; `loadGame()` now sets
   `mode_ = Playing` on success instead of assuming it already was.
3. **`Stats::intelligence` (added Prompt 14) had never actually been
   added to the save format** -- caught while touching save/load again
   for `playerClass` persistence. A real latent gap, silently dropping
   back to the default on every load until now.

Save format bumped to version 2 for both fixes above (old saves rejected
outright, not migrated -- see `ARCHITECTURE_DECISIONS.md`). Loading now
reconstructs the saved player's class -- and therefore the *correct*
talent list -- before applying saved cooldowns positionally, since a
Marauder's cooldowns applied to a Spellblade's talent list (or vice
versa) would silently land on the wrong talent. Verified live and
precisely: saved a Marauder mid-fight with Cleave on a real cooldown,
loaded into a completely separate fresh process, and confirmed the HUD
showed exactly 4 Marauder talents (not 9 Spellblade ones) with Cleave
correctly still on cooldown, hp/mana exactly matching the saved values.
`talent_test`, `monster_ai_test`, and `boss_test` all needed real fixes
too -- `Player`'s constructor no longer hardcodes the Spellblade
internally (a real coupling this prompt had to break), so every
test-local `Player` construction across the whole suite needed an
explicit `TalentSet` argument; caught via a thorough repo-wide grep
after an initial pass missed 14 sites across two files. New
`marauder_test` (24 checks) mirrors `talent_test`'s rigor for the new
kit, including a dedicated regression check for the Empowered bug. See
`ARCHITECTURE_DECISIONS.md` → "Multi-class system and Marauder."

## ⏳ Prompt 16 — Archer (pure Dexterity) + Vault Kick
Originally drafted as "Shadow (Dexterity + Intelligence)" -- redirected
on direct request to **Archer**, pure Dexterity instead of a third
hybrid. Fills the pure-Dex corner of the attribute triangle the same
way Marauder fills pure-Str, rather than stacking a second hybrid
alongside the Spellblade's existing Str+Int. Also requested: a melee-
range **Vault Kick** that damages an adjacent enemy and launches the
caster backwards, away from it -- a genuinely new mechanic, since
nothing in the talent system previously combined "deal damage" with
"move the caster" in one action (Movement-shape talents like Blink
move but never damage; every Damage-kind talent before this only ever
affected the target, never the caster's own position).

**Result:** `PlayerClass::Archer` + `ArcherTalents` (4 talents, matching
the Marauder's "deliberately smaller than the Spellblade's 9"
precedent). Str 14 / Dex 20 / Int 6 -- Dexterity exactly hits the 30%
dodge cap (the maximum possible in the game), Intelligence sits below
baseline as a real, if modest, penalty reinforcing "not a caster."
Lowest hp of any class (24, versus Spellblade's 30 and Marauder's 45) --
survives by not getting hit, not by soaking hits. Smallest mana pool
(8). Kit: Quick Shot (ranged filler, mirrors Slam/Ember Bolt's role),
Volley (ranged AoE, reuses Fireball's AreaAroundTarget shape re-themed
as arrows), Steady Aim (SelfBuff, applies Empowered -- the same status
effect Rallying Cry and the boss's own enrage already use), and **Vault
Kick**.

Vault Kick's knockback is implemented as a new `Talent::retreatDistance`
field (0 for every other talent in the game): after the ordinary damage
step, the caster moves that many tiles directly away from the target,
reusing `resolveBlinkDestination()` (the exact "walk N tiles, stop
early at a wall or another actor" logic Blink already uses) rather than
writing new movement code. The retreat happens whether or not the
kick's own damage was dodged -- it's the caster's own follow-through
motion, not an on-hit effect a dodge would block, unlike Poison or Stun.

Live verification caught something worth noting, not a bug: the first
live attempt (in the existing seed-1337 dungeon) retreated exactly 0
tiles, because the tile directly behind the player happened to be a
wall -- correct behavior (identical to how Blink already handles being
fully blocked), just not a demonstrative test. Crafted a save file
placing the player in open corridor space specifically to get a clean
demonstration, confirmed both numerically (exact expected damage and
retreat destination via the save file) and visually (before/after
screenshots showing the player and target now clearly separated by open
floor). `archer_test` (22 checks) covers every data-level property of
the kit and the ordinary damage math Vault Kick shares with every other
Damage-kind talent; the retreat destination itself needs
`Application::resolveBlinkDestination` and isn't reachable from a
standalone test, so it's verified live instead. See
`ARCHITECTURE_DECISIONS.md` → "Archer and the vault mechanic."

**End-game screens (the other half of the originally drafted Prompt 16)
were not part of this request and remain undone** -- rolled forward to
Prompt 17. "Window just closes on death" (Prompts 10-11) is still the
current behavior.

## ⏳ Prompt 17 — End-game screens
Chosen from the open-ended Prompt 17+ list below (asked directly rather
than assumed -- see the conversation) as the highest-value item: closes
a gap flagged since Prompts 10-11 and carried forward through the
original Prompt 16 draft, uses infrastructure that already exists
(Prompt 13's text rendering, no new systems or assets), and directly
affects the core gameplay loop's polish -- the thing most likely to be
noticed in a demo.

**Result:** a new `GameMode::GameOver`, reached from `checkAndHandleDeath`
on either the player dying or the boss falling, distinguished by a new
`wonGame_` bool. Both render as a standalone screen (same
"replace the view entirely" approach `renderClassSelection` already
uses) -- red "You Died" or gold "Victory!", matching the boss's own
color scheme -- with Enter returning to class selection, where picking
a class does the actual reset (`selectClass()` already fully
reconfigures `player_`/`map_`/`monsters_`/`scheduler_`, so the mode
transition itself needs no extra cleanup).

**Defeating the boss is now the first time "victory" has existed as a
real game state at all** -- before this, killing the boss just logged a
message and let play continue with nothing actually won. Ends the run
outright even if other regular monsters are still alive elsewhere in
the dungeon, matching the boss's role as the set-piece finale (Prompt
11), not one more kill among many.

**A real bug caught live, not assumed away:** the very first death test
logged "You have died!" twice. Traced to `advanceTurnsUntilPlayerCanAct`
calling `checkAndHandleDeath` again after `processMonsterTurns` already
had -- since hp stays at whatever negative value it ended on, the full
death branch re-ran a second time. This was invisible for the entire
life of this project until now, because `window_.close()` used to make
`window_.isOpen()` false immediately, short-circuiting every later call
in that loop before it ever reached `checkAndHandleDeath` again --
removing that call was what finally exposed a pre-existing structural
gap. Fixed with an explicit idempotency guard: once `mode_ ==
GameOver`, the function returns immediately, before even checking hp.
Verified live for both paths with deterministic, hand-crafted save
files (RNG-driven combat made a natural death/boss-kill unreliable to
trigger on demand) -- confirmed exactly one death message and one
victory message, both screens rendering correctly, and Enter correctly
returning to class selection from either. See
`ARCHITECTURE_DECISIONS.md` → "End-game screens."

## ✅ Prompt 18 — Camera/viewport + bigger dungeons
Chosen from the open-ended list below: a known gap flagged since
Prompt 5, pure code with no new asset-sourcing/licensing risk (unlike
sound), and a clear, demonstrable payoff -- once a camera exists,
dungeons can actually be bigger and more explorable, delivering on a
core roguelike expectation the fixed-to-window size never fully did.

**Result:** `Application::updateCamera()` recomputes the viewport's
top-left corner (in tile units) every frame from the player's current
position, centering on them and clamped so the camera never scrolls
past the map's own edges. A new `worldToScreen()` helper replaces every
draw call that used to compute `{x * kTileSize, y * kTileSize}`
directly (tiles, monsters, the player) -- one conversion function, not
several places that could drift out of sync with each other. The tile
render loop now only iterates the camera-visible range instead of the
whole map, both a real performance win and a side effect of the
approach that naturally avoids needing a separate "is this tile on
screen" check per tile. The HUD (bars, talent list, combat log) needed
no changes at all -- it was already drawn in absolute screen pixels,
never tied to world coordinates.

**Dungeon size roughly quadrupled** (38x20/6 rooms -> 60x32/10 rooms),
re-verified empirically rather than assumed safe just because the
original tuning was careful: `dungeon_test` (which already uses default
params, so it automatically re-checked the new values) reports 100%
connectivity and 100% boss room placement at the new size -- *more*
reliable than the original's ~97%, since a bigger map gives the boss
room proportionally more free space to land in, not less.

Verified live: confirmed the camera correctly clamps to the map's
top-left corner when the player starts near that edge (rather than
centering and showing empty void past the boundary), then confirmed it
genuinely follows and centers once the player moves away from an edge
-- visible directly in a screenshot showing previously-explored,
dimmed "remembered" tiles now stretching well beyond what the old
fixed viewport could ever have shown at once. See
`ARCHITECTURE_DECISIONS.md` → "Camera/viewport and bigger dungeons."
**Confirmed on the person's machine.**

## ⏳ Prompt 19 — Three base classes: Fighter, Sorcerer, Thief
Requested as Stage 1 of a larger redesign: restructure around three
pure "base classes," one per attribute -- rename Marauder to Fighter
and Archer to Thief, and design a new pure-Intelligence Sorcerer to
complete the trio. Spellblade (the original Str+Int hybrid) is reserved
for a future unlock rather than removed or kept as a starting option --
the plan beyond this prompt is a leveling system (character levels
1-10, XP from kills), monsters rebalanced for a level-1 start, and
Elite/Nightmare monster tiers introduced at level bands 1-2/3-5/7-8;
"how level-ups actually change abilities" is explicitly deferred to a
later conversation, not designed here.

**Result:** `PlayerClass` enum ordinals deliberately preserved across
the rename (Fighter keeps Marauder's old value, Thief keeps Archer's --
only Sorcerer, genuinely new, is appended) so any existing save file's
encoded integer still means the same class it always did. Files
(`FighterTalents`/`ThiefTalents`, both `.hpp`/`.cpp`), the enum value,
every comment, and both renamed test files (`fighter_test`/
`thief_test`) all updated together -- not just a display-string
change, since file/class-name mismatches would read as sloppy in a
portfolio codebase. Caught via a genuinely useful lesson in thorough
grepping: an initial targeted search for `Player player_(Position`-
style construction found only 2 sites when `Player`'s constructor
signature changed at Prompt 15; a broader pattern later found 14 more
across two files the first pass had missed entirely -- the same
lesson applied here, sweeping every file for stale `Marauder`/`Archer`
references rather than trusting the first pass caught them all (it
hadn't -- `savegame_test.cpp` referenced `PlayerClass::Marauder`
directly and wasn't caught by the file-level renames).

**Sorcerer**, the new class: Str 6 / Dex 8 / Int 24 -- both non-primary
stats are true dump stats, unlike the Thief's moderate Strength (14,
needed since Dexterity carries no damage bonus in this project's
formula system; Sorcerer's Intelligence already covers both damage and
mana, so there's no equivalent need). The lowest hp of any class (22,
even below the Thief's 24) and the largest mana pool by a wide margin
(28). Kit: Arcane Bolt (filler), Arcane Storm (AoE, reusing Fireball's
shape), Arcane Focus (self-buff, reusing Empowered), and **Mind
Shatter** -- modest damage with a 60% chance to Stun the target, the
first player talent to apply a status effect to its *target* rather
than the caster. Required a new `Talent::onHitEffect`/`onHitChance`
pair (mirroring `MonsterAttackProfile`'s existing fields), appended at
the very end of the struct following the same positional-aggregate-
initialization safety rule established when `damageType` was added.

**A shared `rollChance()` utility, extracted while designing Mind
Shatter's roll:** adding a third independent "local static RNG"
pattern (after `Chaser`'s `onHitChance` roll and `rollDodge`, both
already existing) would have been exactly the kind of duplication this
project has consolidated before (`AIUtils.hpp`, Prompt 11) -- extracted
into `AttributeFormulas` instead, and refactored both existing call
sites onto it rather than just adding a third copy alongside two
already there.

**Two real bugs caught in this prompt, both through actually running
things, not code review:**

1. `SorcererTalents.cpp`'s own author (this session) assumed
   `DamageType::Physical` -- the struct's real default -- was Magic,
   and never explicitly set it on any of the three Damage-kind talents.
   `sorcerer_test` failed 4 checks on the very first build, cleanly
   pointing at exactly this. Fixed by explicitly setting
   `damageType = DamageType::Magic` on each, the same "construct then
   assign trailing fields" pattern the Flame-tree Spellblade talents
   already use.
2. A live Mind Shatter test showed only 1 damage dealt where 10 was
   expected, even *after* the fix above and a `sorcerer_test` run that
   passed cleanly. Traced to a stale `roguelike` binary: an apparently
   unrestricted `cmake --build build` after the fix didn't actually
   relink `roguelike` (only `sorcerer_test` had been explicitly
   rebuilt beforehand). Forcing a fresh rebuild resolved it --
   confirmed by observing exactly 10 damage and a successful Stun on
   the very next attempt with a build known to be current. A genuine
   build-hygiene lesson: a passing test suite doesn't guarantee every
   *other* binary sharing that source file was actually relinked
   before the next live check.

Verified live throughout: the new 3-class selection screen renders
correctly with accurate per-class descriptions; Fighter and Thief,
selected through the actual keypress flow (not just their renamed unit
tests), show hp/mana/talent lists exactly matching their pre-rename
values; Mind Shatter's damage and its on-hit Stun both confirmed with
a freshly-verified-current build, including watching the stunned
Goblin actually skip its next turn. `sorcerer_test` (25 checks)
includes a deterministic on-hit-effect test -- forcing `onHitChance` to
1.0 on a local copy of the talent to get guaranteed, repeatable
coverage of the *application* mechanism without unit-testing the RNG
roll itself, consistent with how `rollDodge`'s own roll has always
been treated. All 14 tests pass on a genuinely from-scratch rebuild.
See `ARCHITECTURE_DECISIONS.md` → "Three base classes."

## ⏳ Prompt 20 — Leveling system
Character levels 1-10, starting at level 1, XP gained from kills, some
XP-to-level curve. Deliberately minimal scope for this stage: XP/level
tracking, a curve, a basic (placeholder, not final) level-up effect --
"unlock more abilities, change how abilities work" is explicitly
deferred to a later design conversation, not decided or built here.

**Result:** `Player` gained `level()`/`xp()` (xp tracked as progress
toward the *next* level specifically, resetting on level-up, not a
cumulative lifetime total -- reads directly as "12/40 XP" for the HUD
without a conversion step). The actual curve and level-up logic live in
a new `PlayerLeveling` module, kept separate from `Player`'s plain data
-- the same split this project has kept everywhere else (`Stats`/
`AttributeFormulas`, `Talent`/`TalentEffects`). Curve: `20 *
currentLevel` XP per step (20, 40, 60, ..., 180 from level 9 to 10) --
deliberately simple, explicitly a placeholder alongside the level-up
effect itself (+3 maxHp, +2 maxMana, a full heal -- enough for a
level-up to *feel* like something happened even before the "unlock
abilities" design exists).

`Actor` gained `xpReward()`, 0 by default, set per monster type by a
new `MonsterFactory::xpRewardForType()` (Goblin 10 up to the boss's
200) and wired into `createMonster()` automatically -- callers never
need to know the function exists. XP granted via a new
`Application::grantXpAndAnnounce()`, called from `checkAndHandleDeath`
for both regular monster kills and the boss; also detects and announces
a level-up by comparing level before/after the grant. `selectClass()`
resets level/xp to 1/0 -- a fresh class pick is a new character, not a
continuation of whatever level a previous run on the same long-lived
`Player` object reached.

**`grantXp()` loops rather than checking once, specifically because a
single large grant can cross multiple level thresholds at once** -- the
boss's 200 XP reward against a fresh level-1 character does exactly
this (20+40+60+80 == 200, landing precisely on level 5). Capped at
level 10: XP stops accumulating and is zeroed once there, rather than
piling up toward a level that will never come. `player_leveling_test`
(22 checks) covers the curve, a non-crossing grant, an exact-threshold
grant, a grant with carryover, the exact boss-reward multi-level-up
scenario, and the level-10 cap including "more XP at the cap does
nothing further."

Save/load: `playerLevel`/`playerXp` added to `SaveGameState`, save
format bumped to version 3 (old saves rejected outright, not migrated,
same policy as the two prior version bumps). `savegame_test` updated
with deliberately non-default level/XP values (4 and 37) -- the same
"a round-trip bug that always left this at its default would go
uncaught otherwise" reasoning already applied to `playerClass` and
`intelligence` in earlier prompts.

While in the area: fixed a stale `checkAndHandleDeath` comment that
still described pre-Prompt-17 behavior (the player's death closing the
window outright, victory leaving it open with nothing further
happening) -- found and corrected while adding the XP-granting call
sites, not left to confuse a future reader.

Verified live and precisely, not just via the unit tests: killed two
low-hp monsters in sequence via a hand-crafted save (a Goblin for 10 XP,
then a Spider for 12 XP), confirming the exact log messages, the HUD
updating to "10/20 XP" after the first kill, and a correct "Level up!
You are now level 2" plus hp/mana both growing and fully healing (45/45
-> 48/48, 10/10 -> 12/12) after the second. Separately confirmed the
exact multi-level-up scenario live: a level-1 character killing a
1-hp-remaining boss jumped straight to level 5, exactly matching
`player_leveling_test`'s hand-computed prediction. Confirmed a genuine
save-then-load round-trip (not just loading a hand-crafted file) by
reaching level 2 through real kills, saving, and loading into a
completely fresh process. See `ARCHITECTURE_DECISIONS.md` → "Leveling
system."

## ⏳ Prompt 21 — Corridor connectivity + monster rebalance for a level-1 start
Two pieces, discovered and requested together: while confirming the
bigger dungeon (Prompt 18) actually felt bigger, real testing surfaced
that the single-chain corridor structure (every room connects only to
the next one in placement order, a design choice from Prompt 8) reads
as "one long hallway" rather than "one connected space" once the map is
large enough for that to matter -- not a connectivity bug (proven via
an actual BFS pathfind and a live walkthrough that reached the boss
room), but a real structural limitation worth fixing. Also: the entire
monster roster was tuned assuming no leveling system at all; a level-1
character needs monsters nerfed to still be a fair fight.

**Corridor result:** after the existing chain-guarantee logic runs (so
100% connectivity stays exactly as provable as before), an additional
pass adds up to 6 extra "shortcut" corridors between nearby *regular*
rooms that weren't already chain-adjacent -- capped and probabilistic
(40% chance per eligible pair within 15 tiles), not "connect
everything," since a fully open grid would remove the sense of
distinct areas just as much as a single corridor removes the sense of
one connected space. The boss room is deliberately excluded -- it
should stay reachable only via the full chain, preserving "the far end
of the level, reached at the end of the sequence" (Prompt 11).
`dungeon_test` (which re-checks whatever `DungeonGenerationParams`'
defaults currently are, same mechanism that verified Prompt 18's bigger
dimensions) confirms connectivity and boss placement are both exactly
as reliable as before -- extra connections can only add reachability,
never remove it, but re-verified rather than assumed.

**Monster rebalance result:** every regular monster's damage reduced by
roughly 20-33% (Goblin 5->4, Spider 3->2, Ogre 7->5, Archer 4->3,
Bomber's blast 10->7), the boss slightly less aggressively (melee 9->6,
blast 14->10 -- it's meant to still be the hardest fight in the game,
just not instant-death for a level-1 character who reaches it early).
Same recalibration methodology as every attribute-driven change since
Prompt 14: each monster's base `power` was reduced by exactly enough to
hit the new target total once its own attribute bonus is added back in,
not just an arbitrary new number. `boss_test`/`monster_ai_test` needed
no changes -- confirmed, not assumed, since both construct their
`AIBehavior`s directly with test-local values rather than through
`MonsterFactory`, the same reason they were unaffected by the original
Prompt 14 recalibration too.

Verified live, precisely, re-running the *exact* same scripted
walkthrough that had nearly killed a level-1 Fighter before this
prompt: the Archer's hits dropped from 4 to exactly 3 damage each
(matching the new target precisely), and the same character survived
that encounter with real margin this time (9/45 hp left instead of
dying) before continuing on into an unprepared encounter with the boss
itself -- which hit for exactly 6, the new rebalanced value, not the
old 9. The character still ultimately died to the boss, which is the
correct outcome, not a bug: a level-1 character stumbling into the
climactic fight unprepared should still lose, just not to a single
regular monster encounter along the way. See
`ARCHITECTURE_DECISIONS.md` → "Corridor connectivity and monster
rebalance."

## ✅ Prompt 22 — Elite/Nightmare monster tiers
Tougher variants of the existing roster, introduced at specific level
bands: base monsters at 1-2, Elite introduced at 3-5, Nightmare
introduced at 7-8. Needs monster spawning to become level-aware for the
first time (currently the same fixed roster spawns regardless of
anything) -- the most architecturally involved of the three staged
follow-on prompts, as flagged in advance.

**Result:** a new `MonsterTier` enum (Base/Elite/Nightmare) and
`tierForLevel(int)`, mapping player level to tier. The original spec
left levels 6, 9, and 10 unstated (1-2/3-5/7-8 doesn't cover the whole
1-10 range) -- each gap was filled by extending the nearest
neighboring tier forward: Elite through 6, Nightmare through 10, rather
than left as an arbitrary undefined case.

`MonsterFactory::createMonster()` gained a defaulted `tier` parameter
(`= MonsterTier::Base`), so every pre-Prompt-22 call site -- including
every existing test -- keeps compiling and behaving completely
unchanged. hp scales by a flat multiplier (Elite x1.5, Nightmare x2.2);
damage scales differently and more carefully: **not** by multiplying
the flat `power` field directly, but by scaling the monster's *total*
damage (power + attribute bonus) and back-deriving a new base power --
for a monster like the Ogre, most of its damage comes from the
Strength-bonus formula, not the flat field, so a naive "multiply power
alone" approach would have barely changed its actual output. Same
"base + bonus = target total" recalibration discipline every
attribute-driven number in this project has used since Prompt 14, now
applied a second time on top of Prompt 21's already-rebalanced base
numbers. XP reward scales independently (Elite x2, Nightmare x4) via
an extended `xpRewardForType(type, tier)`.

**The boss deliberately ignores tier entirely** -- it's already a
separately-tuned "hardest fight in the game"; scaling it further at
high character levels risked making the climactic encounter absurd
rather than harder, not a more satisfying finale.

**Monster spawning is uniform per dungeon, not mixed.** One tier is
computed once per `regenerateLevel()` call (from the player's current
level) and applied to every regular monster in that dungeon -- simpler
to implement, test, and reason about than having each spawn slot
independently roll a tier, at the cost of some variety within a single
level. Flagged as a reasonable future refinement if more variety is
wanted later, not silently decided as final.

**Visual identification needed real iteration, not just a first
attempt.** A monster's tier is genuinely visible two ways: the name
prefix ("Elite Goblin", "Nightmare Goblin" -- clearly visible in the
combat log) and a colored border drawn behind the monster's own tile.
The first border color chosen for Nightmare (a deep red) looked fine in
isolation but nearly disappeared against the Goblin's own red tile
color when actually checked against a live screenshot -- not caught by
reasoning about the color in the abstract. Replaced with plain white,
which contrasts against every color in the roster at once, and
re-verified visually before considering it done.

**A real correctness issue caught while implementing, not shipped
silently:** the monster color lookup (`monsterColor()`) matched by
exact display-name string ("Goblin", "Spider", ...) -- a tier-prefixed
name like "Elite Goblin" would have silently failed that match and
fallen through to the default gray, breaking type-based color coding
for every tiered monster. Refactored to key off `MonsterType` instead
(more robust regardless of what a monster is later renamed to) before
this could ship as a visible bug.

**Save/load needed a real design decision, not an afterthought.**
Monster tier isn't stored explicitly in the save format (no version
bump needed) -- `loadGame()` instead reconstructs each monster at
`tierForLevel(state.playerLevel)`, the tier matching the *saved* player
level. hp/maxHp are always overwritten with the exact saved values
regardless (already explicit save fields), so what actually needed
fixing was that a loaded monster's *name and damage output* -- baked
into its `AIBehavior` at construction time, not stored directly --
would otherwise have silently reverted to Base tier on load, producing
a monster whose displayed hp didn't match its actual damage output. Not
a perfect reconstruction in every edge case (a level-up mid-dungeon
before saving means the derived tier may not exactly match the
original spawn tier), but a *consistent* one -- every field on a
reconstructed monster matches some real tier's numbers together, never
a mismatched blend of two different tiers.

Verified live and precisely: crafted saves at level 4 (Elite band) and
level 8 (Nightmare band) each with an adjacent Goblin, confirming exact
combat-log names ("Elite Goblin", "Nightmare Goblin") and exact
rebalanced-then-tiered damage (base 4 -> Elite 6 -> Nightmare 7,
matching round(4 * 1.4) and round(4 * 1.8) precisely) -- not
approximately right, but the literal predicted numbers. `monster_tier_test`
(32 checks) covers the level-band logic including the filled-in gaps,
every multiplier, `createMonster()`'s actual tiered output for both a
zero-attribute-bonus monster (Goblin) and a bonus-dominated one (Ogre --
the case that actually exercises the "scale the total, not just power"
logic), and confirms the boss ignores tier under all three explicitly-
requested values. See `ARCHITECTURE_DECISIONS.md` → "Elite/Nightmare
tiers." **Confirmed on the person's machine.**

## ⏳ Prompt 23 — Level-gated talent unlocks
Requested as "unlock more abilities, change how abilities work" -- the
future design work explicitly deferred back when Prompt 20's leveling
system shipped with only a placeholder level-up effect.

**Result:** each base class (Fighter/Sorcerer/Thief) gains 2 more
talents beyond its starting 4 -- one at level 4, one at level 7,
chosen to land inside the existing Elite (3-6) and Nightmare (7-10)
tier bands so a growing kit lines up with growing danger. Each new
talent is a genuine upgrade on an existing one, not a reskin: Fighter's
Whirlwind hits a wider radius than Cleave for more damage; Thief's
Piercing Shot reuses the Spellblade's Execution conditional-multiplier
mechanic (Prompt 9) for the first time outside that class; every
class's level-7 talent is a stronger, longer version of its own
level-2 self-buff. `TalentSet::learnTalent()` appends without
disturbing any existing talent's index -- critical, since cooldowns are
tracked positionally by index throughout this project.
`PlayerClassFactory::talentUnlockedAtLevel()` dispatches to each
class's own per-level lookup, mirroring `talentSetForClass()`'s own
per-class dispatch shape. Wired into a new `Application::processLevelUpEffects()`,
called for every level actually crossed (not just the final level
reached) so a multi-level jump -- the boss's XP against an early
character -- can't skip an unlock sitting at an intermediate level.
`talent_unlock_test` (22 checks). Verified live: a scripted kill
crossing level 4 correctly announced "New talent unlocked: Whirlwind!"
and the HUD's talent list grew from 4 to 5, immediately usable.

## ⏳ Prompt 24 — The Fighter/Sorcerer hybrid path
A follow-up design question to Prompt 23: what does reaching level 10
actually unlock, and does it fulfill the promise made when Spellblade
was reserved for "a future unlock" back at Prompt 19? Refined through
direct conversation into a genuinely different, more interesting shape
than the original assumption (a permanent meta-unlock for future
characters) -- an in-run hybrid-spec mechanic instead.

**Result:** at level 5, a Fighter or Sorcerer (the two classes that
each represent half of Spellblade's own Str+Int identity -- Thief,
pure Dexterity, shares neither and has no hybrid path) can spec into
the hybrid path. The "pool" isn't a separate pre-written talent list --
it's literally the *opposing* class's own kit. A Fighter who specs in
picks from Sorcerer's abilities; a Sorcerer picks from Fighter's. One
pick per level from 5 through 10 (level 5 is both the spec-in moment
and the first pick), so a fully-committed character ends up with their
entire original kit *and* the entire opposing kit by level 10 -- a
genuine mechanical hybrid, without needing the original hand-written
Spellblade talent list at all. The character's class and stats never
change; this is an added status, not a class swap. New `HybridSpec`
module (`isHybridEligible`, `hybridPoolClass`, `fullKitForClass`,
`availableHybridPicks`/`pickedHybridTalents`) and a new `AbilityChoice`
game mode with its own choice screen, wired through a resumable
`processLevelUpEffects()` that can pause mid-multi-level-jump for a
real decision and correctly pick back up afterward rather than losing
track of a later level's content. `hybrid_spec_test` (14 checks).

**A real, serious bug found and fixed during this same prompt, not
shipped:** save/load didn't persist learned talents at all --
`loadGame()` rebuilt the talent list from each class's default starting
4, silently discarding every level-unlocked *and* hybrid-picked talent
on any save/load round-trip. Fixed without a full re-serialization:
base-class level-4/level-7 unlocks are fully deterministic from the
saved player level, so `loadGame()` just re-derives them (the same
"save the minimum that's a genuine choice, re-derive everything else"
approach monster tier reconstruction already used); hybrid picks are a
real, non-derivable player decision, so those genuinely needed saving
-- added as a list of talent names (save format bumped to version 4),
escaped with underscores in place of spaces since talent names like
"Arcane Bolt" don't fit this format's whitespace-delimited-token
convention and no talent name contains an underscore naturally.
`savegame_test` extended with deliberately non-default hybrid data,
specifically including a name with a space to exercise the escaping
round-trip. Verified live end-to-end, not just via the unit test:
specced into Meteor, saved, and loaded into a completely fresh process
-- the talent list correctly showed both the auto-derived Whirlwind and
the explicitly re-learned Meteor together. Also verified the decline
path live (staying on the current path resumes play normally, with the
base-class unlock still granted and no hybrid talent added) and a
caught-live visual bug (one talent description overflowed the choice
screen's width, fixed after actually checking a screenshot rather than
trusting the layout math).

## ⏳ Prompt 25 — Sound effects
`SFML_BUILD_AUDIO` has been off since Prompt 2, deliberately, until
there was a reason to want it -- there's a reason now.

**A real constraint shaped this prompt from the start, not an
afterthought:** no network access to typical royalty-free sound
libraries (freesound.org, opengameart.org, etc. aren't reachable from
this environment). Rather than being blocked by that, every sound
effect is synthesized programmatically -- tones and noise bursts
generated with numpy and written out as plain WAV -- sidestepping any
licensing question entirely, the same way many retro/8-bit-style games
handle audio. Five effects: hit, death, level-up (a rising 4-note
arpeggio), dodge (a quick whoosh), and select (a UI blip) -- covering
combat, progression, and menu interaction with a deliberately small,
well-chosen set rather than trying to cover every possible event on the
first pass.

**A real portability problem was found and fixed before it could ship,
not discovered later.** SFML's audio module requires Vorbis/OGG
libraries to build at all. On Linux, SFML's own default is to look for
these as *system* libraries rather than fetching and building its own
copies -- which would have quietly broken this project's "one cmake
command, no manual dependency installation" story (true since Prompt 2)
for any Linux user, while working fine by accident on Windows/macOS
(where SFML's own default points the other way). Forced
`SFML_USE_SYSTEM_DEPS OFF` explicitly so the behavior is the same on
every platform, then verified this wasn't just correct in theory: apt-
installed the system audio libraries, hit the exact failure a Linux
user would have hit, then *uninstalled every one of them* and confirmed
a full clean rebuild still succeeds -- SFML fetches and builds its own
Vorbis/FLAC/Ogg from source, the same way it already fetches itself.

**Result:** a new `SoundManager` class -- the only class besides
`Application` that touches `SFML::Audio` directly, mirroring the
existing `SFML::Graphics`/`Window` boundary. One `sf::SoundBuffer`/
`sf::Sound` pair per effect (not a pool for overlapping playback) --
deliberately simpler than a real-time game would need, since this is
turn-based and sound events are naturally spaced out enough that
re-triggering a single instance is entirely adequate. Wired into real
game events: a hit or dodge on either side of a fight, any death
(player, monster, or boss), a level-up, and UI selections (class pick,
the AbilityChoice screen).

**Explicitly designed so sound is a presentation detail, never a hard
requirement -- confirmed directly, not assumed.** miniaudio (which
SFML's audio module is built on) was tested in a genuinely headless
environment with no audio device present at all: it logs errors to
stderr and gracefully no-ops rather than throwing or crashing.
Confirmed live through real gameplay covering every trigger path (class
selection, a kill crossing a level-up with a talent unlock immediately
after) -- the game ran the entire sequence correctly with the audio
subsystem completely unavailable, exactly the scenario this environment
actually presents, not a hypothetical one.

## ⏳ Prompt 26 — The attribute-system redesign
Requested as a from-scratch rework of starting classes, leveling, and
attributes -- designed collaboratively across an extended conversation
(see the transcript) rather than handed over as a single spec: an
initial "smaller PoE tree" idea was pushed back on and reshaped several
times before settling on a concrete, buildable design. The core shift:
no more baseline-10 model (where every stat implicitly compared against
an assumed "average" of 10) -- classes now start at genuinely low,
hand-picked values (2 or 6), every point counts at full value from
zero, and a talent's damage scales from exactly one attribute the
talent itself declares, not a universal per-actor bonus.

**Classes renamed again:** Fighter -> Warrior, Sorcerer -> Mage (Thief
keeps its name) -- hand-picked starting HP/Mana per class (Warrior
30/10, Thief 25/15, Mage 20/20), explicitly *not* derived from the
6/2/2 Strength/Dexterity/Intelligence spread at all. That spread is
used purely for ability-scaling damage and secondary effects (Dexterity
-> dodge/crit) from the moment of character creation onward.

**Leveling reworked:** +1 max HP automatically every level (down from
the old flat +3), no more automatic mana growth at all -- mana only
grows from Intelligence points a player actually chooses to spend.
Every level also grants 2 free attribute points, spent through a new
`AttributeAllocation` screen. This needed a real state machine
(`resumeLevelUpSequence()`) to sequence correctly against the *existing*
talent-unlock and hybrid-choice systems (Prompts 23-24) without losing
track of a later pause if a big XP grant crosses multiple decision
points at once -- attribute allocation always resolves first, then
talent unlocks, then hybrid choices, for whatever levels a single XP
grant actually crossed.

**Damage scaling is now per-ability, not per-actor.** Each talent
declares one `ScalingStat` (Strength/Dexterity/Intelligence, replacing
the old two-way Physical/Magic `DamageType`) and scales by `+1 damage
per 5 points in that stat, multiplied by the ability's own cooldown
tier` (Filler x1.0, Core x1.5, Power x2.0, Signature x2.5) -- a
deliberate first-pass formula, not a final tuned one, chosen so a
talent used rarely rewards investment more than one spammed every turn.

**Crit is new and global:** every actor, player and monster alike,
rolls a 5% base chance, +0.5%/point of the attacker's own Dexterity,
for 1.5x damage on a hit. Dodge also lost its baseline: +0.5%/point of
current total Dexterity, capped at 25% (down from the old 30%). Both
apply to monsters exactly as they do to players -- the exact same
`rollDodge`/`rollCrit` functions, not a separate monster-side copy.

**Piercing Shot (Thief's level-4 unlock) was reworked mid-conversation**
from its original conditional-triple-damage-on-low-hp mechanic into an
inherent +20% crit chance and +50% increased crit damage (a Piercing
Shot crit deals 2.0x, not the global 1.5x), cooldown raised from 4 to
7 to match. Needed new per-talent `bonusCritChance`/
`bonusCritDamageMultiplier` fields and overloaded `rollCrit`/
`critDamageMultiplier` functions, rather than hijacking the global crit
numbers for one talent.

**The test suite needed a genuinely new testing pattern, not just
updated numbers.** Global crit means every damage check now has a real
chance (as high as ~28% for a Thief using Piercing Shot) of landing an
unplanned critical hit -- an exact `==` assertion would be flaky, not
wrong. Every damage-value check across `talent_test`/`fighter_test`/
`thief_test`/`sorcerer_test`/`talent_unlock_test` now verifies the
result matches *either* the normal or the critical value, confirmed
non-flaky by actually running the full suite 5+ times in a row, not
just reasoning that the pattern should work. `Stats`' own default
Dexterity (10, a leftover from the deleted baseline-10 model) was also
caught and fixed to 0 -- left at 10, it would have silently given every
test's default-constructed target a real dodge chance.

**Two real, portability-affecting problems were caught and fixed while
building this, neither hypothetical:** `SFML_BUILD_AUDIO` requiring
Vorbis/OGG, which defaults to *system* libraries on Linux and this
project's own bundled ones on Windows/macOS -- forced
`SFML_USE_SYSTEM_DEPS OFF` explicitly and verified by literally
uninstalling the system libraries and confirming a from-scratch build
still succeeds. And a real save/load gap (Prompt 24's own fix) that
generalizes here too -- every new save field this prompt needed
(`playerHybridPickNames`-style reasoning) follows the same "save only
what's a genuine choice, re-derive the rest from level" principle
already established.

**Known, explicitly-flagged gaps, not silently left unfinished:**
monster Strength/Dexterity/Intelligence are still on the old 6-20 scale
and haven't been rebalanced against the new formula -- functionally
correct (compiles, produces real damage) but not tuned. Spellblade's
stats are deliberately left on the old baseline-10 model, since it's
still unreachable in play (reserved for a future unlock, see Prompt 19)
-- fixing it up is a no-op until that unlock mechanism exists. File and
function names (`FighterTalents.cpp`, `fighterTalents()`,
`SorcererTalents.cpp`, `sorcererTalents()`) were *not* renamed to match
Warrior/Mage this time, unlike the thorough Prompt 19 rename -- a
deliberate scope cut given how much else this redesign already
touched, not an oversight.

## ⏳ Prompt 27 — Monster attribute rebalance
The most significant open item flagged from Prompt 26: monster
Strength/Dexterity/Intelligence were still on the old 6-20-range
values, now interacting with a damage/dodge formula they were never
tuned against.

**Result:** every monster's Str/Dex/Int recomputed to reproduce
exactly the same dodge percentages and damage totals already carefully
tuned in Prompt 21 -- not new numbers guessed from scratch. Spider and
Archer's Dexterity (36 and 48) look large next to a player's starting
2-6, deliberately: monster attributes aren't held to the same
"genuinely low, hand-picked" philosophy player stats follow, since
monsters are static and never grow through play the way a player's
starting spread is designed to. Those values exist purely to reproduce
this roster's existing 18%/24% dodge identity exactly under the new
no-baseline formula (36 * 0.5% == 18%, 48 * 0.5% == 24%). A side effect
worth noting, not a separately-designed feature: the same high
Dexterity now also gives Spider/Archer a real crit chance (23%/29%)
under the new global crit system, which happens to reinforce rather
than fight their existing "nimble, precise" identity. Every other
monster's Strength/Intelligence was chosen to keep its own damage total
exactly matching the Prompt 21 value, using the same "base + attribute
bonus = target total" recalibration discipline every attribute-driven
number in this project has used since Prompt 14.

Verified two ways, not just by re-running the existing test suite
(which passed without a single change needed, confirming no test had
been asserting a stale exact value): live combat against a real Ogre --
the case that most exercises this, since most of its damage comes from
the Strength bonus rather than the flat power field -- showed
"Ogre hits Player for 5" consistently across many real hits, and
"Ogre critically hits Player for 7" on the one crit that landed,
both exactly matching the hand-computed target (5 total, crit
== round(5 * 1.5) == 7).

## ⏳ Prompt 28 — Multi-floor dungeon progression (skeleton)
Requested as two connected pieces: gate the boss behind at least a
handful of rooms so there's real time to level up first, and add red
doors to move between separate dungeon "areas." What it became,
through conversation, was a full 10-floor progression structure:
floors 1-4 and 6-9 are pure "clear it, find the door" dungeons with no
boss at all; floor 5 has the existing Goblin Warlord, defeating him
opens the door rather than ending the run; floor 10 is the true final
fight. Explicitly staged: this prompt is the complete structural
skeleton, verified working end to end for all ten floors -- the actual
Lich (see below) is separate, later work, and floor 10 currently reuses
the Goblin Warlord as a placeholder specifically so the full 1-10
structure and victory gating could be built and verified correctly
first, rather than being blocked on content that doesn't exist yet.

**The original "room 5" request turned out to generalize cleanly.** The
existing shortcut-connection system (Prompt 21) already protected the
boss room from being bypassed; the same protection was extended to the
*last* regular room too, since that room now hosts the door on every
non-boss floor -- reaching either a boss or a door still means actually
working through the floor, not skipping most of it via a lucky
shortcut. Verified `dungeon_test` still reports 100% connectivity after
this change, not just assumed compatible.

**Doors need no separate "is it active yet" tracking at all -- a
deliberate simplification, not an oversight.** On a boss floor, the
door tile sits exactly where the boss stands. While the boss is alive
it blocks that tile like any other actor, the same as it always has;
once it's dead, the tile is simply walkable, and the door "activates"
as a pure side effect of an existing game rule rather than a new one.

**A real bug was found and fixed by live testing, not by the design
review that preceded it.** The initial implementation set the GameOver/
victory transition unconditionally the moment the final-floor boss
died -- but `grantXpAndAnnounce()` can itself leave the game paused on
an AttributeAllocation or AbilityChoice screen if that kill's XP
crosses a level-up threshold, and overwriting that unconditionally
would have silently discarded attribute points or a talent choice the
player had genuinely just earned, replacing the choice screen with the
victory screen before it was ever shown. Caught specifically by
live-testing a final-boss kill that *also* crossed a level-up threshold
-- an easy scenario to skip when testing the systems separately, since
neither one alone would have revealed it. Fixed with a
`pendingFinalVictory_` flag that defers the actual transition until
`resumeLevelUpSequence()` -- the same resumable state machine
Prompts 23/24 already built for exactly this class of problem --
confirms nothing is left pending, rather than adding a second, separate
pause mechanism. Verified live twice: once catching the bug happening,
once confirming the fix (attribute screen and talent unlock both shown,
in order, before Victory finally appears).

**Save format bumped to version 5** for `currentFloor` and the new
`Door` tile type (`'D'` in the save file's map rows, alongside the
existing `#`/`.`) -- both covered by dedicated round-trip tests, not
just added and assumed to work.

Live-verified the complete flow across three separate scenarios, not
just one: a plain floor 1 -> 2 transition through a door, a floor 4 -> 5
transition confirming the boss actually appears, and the floor 5 boss
kill -> door -> floor 6 sequence confirming defeat doesn't end the run
and the door becomes reachable immediately after.

## ⏳ Prompt 29 — The Lich
The piece explicitly deferred from Prompt 28: floor kFinalFloor's real
boss, replacing the Goblin Warlord placeholder that had been standing
in for it.

**A genuinely new mechanic, not a reskin.** Nothing in this engine
previously let a monster create another monster mid-fight -- every
existing AIBehavior only ever acted on itself or the player. Needed a
new `AIActionType::Summon`, `MonsterType`/`MonsterTier` fields on
`AIDecision` (so LichBehavior decides *what* to summon and Application
still owns *how* -- the same "AIBehavior decides, Application executes"
split every other action type already follows, not an exception carved
out for this one), and real handling in `executeAIDecision()` that
actually constructs the Monster via the existing `createMonster()` and
adds it to both `monsters_` and the scheduler. Confirmed safe to do
mid-turn-processing specifically because `monsters_` is
`vector<unique_ptr<Monster>>` -- the underlying Monster objects' own
addresses never move even if the vector itself reallocates, so nothing
holding a raw pointer into it (which is exactly what
`processMonsterTurns()`'s own loop does, via `currentActor_`) can be
invalidated by a mid-loop `push_back`.

**Deliberately no phase structure like BossBehavior.** The summon
mechanic itself is what makes the fight escalate (more attackers over
time), so a second, HP-threshold-based escalation axis on top would
have been redundant rather than additive. Instead: Kiter-style ranged
behavior (bolt from range, retreat if approached), with summoning
preferred over bolting whenever it's off cooldown, under its cap (3),
and there's an open tile to summon into.

**A new `LichBehavior` test was written matching `boss_test.cpp`'s own
rigor** -- not just relying on live testing, given how much genuinely
new logic (cooldown gating, the cap held across multiple ready
cooldowns, falling back gracefully when every neighboring tile is
blocked, and confirming a blocked attempt doesn't silently consume the
cap) needed real coverage. Caught a real mistake in the test itself
during first the run, not the implementation -- a target placed beyond
the Lich's sight radius, not just beyond attack range, which produced
a `Wait` decision instead of the expected `Move` and would have been a
confusing false failure to debug blind.

**Live-verified the complete fight, not just the isolated mechanic:**
approached a real Lich and watched it summon a Skeleton, bolt for
exactly the hand-computed 9 damage (6 base + 3 from Intelligence),
land a critical hit for exactly 13 (round(9 * 1.5)), and watched the
summoned Skeleton itself deal exactly 3 damage (2 base + 1 from
Strength) and physically block the player's path to the Lich --
confirming the "dangerous in numbers if ignored" design intent, not
just the numbers. Separately verified the multi-level-jump case
specifically: defeating the Lich from level 5 crossed two level
thresholds at once (4 attribute points, not 2), and confirmed every
point plus the resulting talent unlock still resolved correctly before
Victory appeared, exercising the Prompt 28 fix's more complex case, not
just the simple one.

**Also fixed while here, not left as a loose end:** two places that
hardcoded "Goblin Warlord" in player-facing text (the victory log line
and the GameOver screen) now read generically or from a captured
`defeatedBossName_`, so the correct boss name shows regardless of
which floor's fight the player actually won.

## ⏳ Prompt 30 — FighterTalents/SorcererTalents renamed
The last cosmetic loose end from Prompt 26's class rename, deliberately
deferred at the time: `FighterTalents.hpp/cpp` -> `WarriorTalents.hpp/cpp`,
`SorcererTalents.hpp/cpp` -> `MageTalents.hpp/cpp`, `fighterTalents()` ->
`warriorTalents()`, `fighterTalentUnlockedAtLevel()` ->
`warriorTalentUnlockedAtLevel()`, `sorcererTalents()` -> `mageTalents()`,
`sorcererTalentUnlockedAtLevel()` -> `mageTalentUnlockedAtLevel()`, and
(for full consistency, beyond what the name strictly required)
`tests/fighter_test.cpp` -> `tests/warrior_test.cpp`,
`tests/sorcerer_test.cpp` -> `tests/mage_test.cpp`, matching the
Prompt 19 precedent where the file/test renames happened alongside the
class rename, not separately.

**Two real, compile-breaking bugs were introduced by the rename itself
and caught immediately by actually rebuilding, not just editing and
assuming correct:** `WarriorTalents.cpp` and `MageTalents.cpp` each
still `#include`d their own *old* header name after the file rename --
a mistake the rename's own mechanical nature makes easy to make (moving
a file doesn't automatically fix a self-referential include inside it)
and easy to catch immediately, simply by attempting a real build
straight after.

**A second search pass, broader than the first, was needed and
deliberately run before considering this finished.** The first
grep for `fighterTalents`/`sorcererTalents` (matching the plural
"Talents") missed `fighterTalentUnlockedAtLevel`/
`sorcererTalentUnlockedAtLevel` (singular "Talent" plus a different
suffix) entirely -- a different, non-overlapping substring, not a
variant the first search's pattern could have matched. Caught by a
full clean rebuild surfacing the resulting compile errors directly in
`talent_unlock_test.cpp`, then closed out with a broader follow-up
search across the whole codebase (twice, since the second pass itself
turned up a handful more: a stale cross-reference in `talent_test.cpp`,
a stray local variable name in `hybrid_spec_test.cpp`, and -- found
only because the search was thorough enough to surface it --
`MageTalents.cpp` still describing the pre-Prompt-26 `damageType` field
in a comment, a leftover staleness from the *attribute redesign*, not
this rename, fixed while already there rather than left for a third
pass to find later).

## Phase 3 — Tactical combat and build variety (proposed 2026-09-25)

The next ten prompts develop the requested ToME4/PoE2 direction: deliberate
turn-based combat, recognizable class identities, skills that can be
modified, and equipment that makes character-building decisions matter.
Entries remain **future implementation prompts** until marked done with
recorded results. The starting files document the Lich
and class-file renames through Prompt 30; begin this phase from that state.
Historical completion claims above have not been re-verified for this plan.

Keep the existing three starting classes, ten floors, and level-10 cap for
this phase. Each prompt must produce something usable in the real game.
Explain the design and relevant C++ trade-offs, build the changed targets,
run focused checks, and demonstrate the new behavior before advancing.
Update README.md and ARCHITECTURE_DECISIONS.md with what was actually built;
record results here only after checking them. Keep game rules independent
of SFML and definitions separate from their execution logic. Extract a
small module when a feature needs it; avoid a speculative engine rewrite.

**Persistence is part of each feature.** Add save/load support when new
persistent state first appears. Follow the existing version-bump and clear
old-save rejection policy unless a migration is deliberately implemented.
Preserve state across floor transitions as well as save/load. Use stable
definition IDs and unique instance IDs where multiple copies can exist;
display names and vector positions should not identify new saved content.

The concrete counts and rewards below are initial design proposals, open
to adjustment when implementing their prompt. Prompt 35 explicitly revisits
the locked Prompt 24 hybrid rules; its proposed replacement must be settled
with the user before changing those rules.

## ✅ Prompt 31 — Choose targets and inspect the battlefield

**Prompt:** Give the player deliberate control over talent targeting.
For targeted attacks, allow cycling through valid visible enemies, show
the selected target and affected tiles, and confirm or cancel before
spending a turn. Keep immediate activation for unambiguous self-targeted
abilities. Add a keyboard inspection panel showing a visible enemy's HP,
statuses, speed, and a short description of its behavior. Show a talent's
cost, cooldown, range/targeting rules, and damage estimate with uncertainty
clearly identified. Add a paged talent menu or equivalent keyboard control
so every learned talent remains accessible, including a full hybrid kit.

**Why now:** Selecting the target is necessary for later skill modifiers,
status combinations, and boss mechanics. More abilities also need a usable
interface before the game adds more ways to acquire them.

**Keep bounded:** Mouse and keyboard, using the existing text renderer. No new
art dependency or general UI framework. Inspection must not reveal hidden
actors. Preview calculations must not advance combat RNG or apply effects.

**Done when:** Two enemies in sight can be targeted independently; the
preview and actual affected tiles agree; cancelling or selecting an invalid
target spends no turn, mana, or cooldown. All talents in a twelve-talent
kit can be used. Existing self-casts and movement talents remain usable.

**Result (2026-09-25):** Implemented explicit aiming with mouse/keyboard,
Tab cycling, confirm/cancel, projectile arrows, AoE and movement previews,
and a paged talent sidebar. Inspection shows enemy stats, effects, behavior,
and ability descriptions while keeping remaining cooldowns hidden by default,
as refined in conversation. A permission parameter supports later revealing
abilities; an actual Analyze talent remains future design work.

`TalentTargeting` is independent of SFML and supplies both preview and cast
resolution. Damage estimates share the real damage arithmetic without RNG.
Transient selection stores coordinates/indexes, and clears on load or floor
regeneration. Casting copies the selected talent before applying damage,
because kill XP can append a talent and invalidate references into the kit.
The sidebar reduces map width; 28-pixel tiles keep the full sight radius
within the remaining viewport. See README for controls and the deliberately
changed player projectile/visible-splash rules.

**Verification:** Windows Debug build succeeded. All nineteen pre-existing
console tests passed, along with the new `targeting_test` and hidden-window
`application_targeting_test`. Checks cover chosen targets, interception,
walls/corners, hidden information, movement geometry, damage estimates,
cancel/invalid-cast resource invariants, one-turn confirmation, mouse/camera
mapping, casts beyond the ninth talent, and save/load selection reset.
Rendered snapshots of aiming, blocked shots, area targeting, hybrid page two,
and Lich inspection were generated for visual review. An initial integration
test selected Mind Shatter where it meant Overload; correcting that fixture
made its intended second-page self-buff check pass. This is automated
integration and render verification, not a full balance playthrough.

## ✅ Prompt 32 — Real inventory and equipment

**Prompt:** Turn Item and Inventory from placeholders into playable
systems. Start with three equipment slots: weapon, armour, and one charm.
Create a small set of fixed items with stable definition IDs, ground
pickup, an inventory screen, equip/unequip, and item comparison. Establish
base character stats separately from equipment bonuses, and calculate
effective stats from those sources without repeatedly mutating the base.
Choose and document which inventory actions consume a turn before wiring
them into the scheduler. Include a few fixed pickups in ordinary floors.

**Why now:** This creates the item ownership, stat calculation, and UI
foundation that randomized loot and skill runes will reuse.

**Keep bounded:** Small C++ definition tables are sufficient. Start with
existing attributes and HP/mana bonuses; shops, crafting, durability,
encumbrance, and elemental resistances can wait. Armour is an equipment
slot here; a new physical mitigation formula is outside this prompt.

**Done when:** Pickup transfers ownership exactly once; an occupied slot
can be replaced without losing either item; repeated equip/unequip never
accumulates bonuses. Define maximum-pool changes so swapping equipment
cannot manufacture free HP or mana. Ground items, inventory, equipment,
and effective stats survive save/load and floor changes correctly.

**Result (2026-09-25):** Implemented nine fixed items, weapon/armour/charm
slots, unique ownership and instance IDs, G pickup, B inventory with keyboard
selection/paging, equip/removal and base/current/after comparisons. Three fixed
supplies appear near each ordinary floor's entrance. Browsing is free;
successful pickup/equipment changes cost one turn and close the inventory.
Permanent stats and equipment bonuses are separate; capacity changes clamp
current pools without refilling them. Save format 6 persists ground items,
bag order, equipment and instance counters and rejects old saves. Owned gear
persists between floors; uncollected items leave with the old map.
Debug `roguelike` builds successfully. Automated tests and interactive
playthrough were not run in this prompt; runtime verification remains pending.

## ✅ Prompt 33 — Loot with useful affixes and paced rewards

**Prompt:** Add a seeded loot generator using Prompt 32's item model.
Begin with normal items, magic items carrying one affix, and rare items
carrying two compatible affixes. Use a small reviewed pool of approximately
eight affixes, with explicit eligible slots and value ranges. Add bounded
ordinary-enemy drops, one guaranteed reward chest per floor, and stronger
boss rewards. Show rolled modifiers and comparisons clearly in the UI.
Use a dedicated loot RNG stream so opening an inventory or previewing a
skill cannot change the next drop.

**Why now:** A run should produce several chances to improve or redirect
a build. This gives equipment a progression role while leaving more
complex skill-changing items to the next prompt.

**Keep bounded:** No trading economy, crafting currencies, or sprawling
rarity system. Use the stats already supported by Prompt 32. Derive reward
strength mainly from floor depth, with boss and elite adjustments, and
document the intended number of meaningful drops across ten floors.

**Done when:** A known seed produces reproducible item rolls; incompatible
or duplicate affixes are excluded according to explicit rules; a rare item
can be compared without guessing which stats it changes. Saving preserves
the actual rolled items and loot RNG state. Claimed chests cannot grant
their rewards again after loading, and summoned Skeletons cannot become
an unlimited source of loot or XP.

**Result (2026-09-25):** Added normal/magic/rare equipment, eight slot-filtered
affixes with no repeated stat group, depth/tier scaling, dedicated serialized
loot RNG, two ordinary drops maximum per floor, one claimed-once chest per
floor and two rare items per boss. Comparisons show actual rolls. Only floor
one retains fixed entrance supplies. Summons grant neither XP nor loot;
eligibility and exact monster tier persist. Budget: about 33-37 equipment
candidates per full run, aiming for 6-10 useful changes pending playtesting.
The Debug game target builds; runtime tests/playthrough were not run.

## ✅ Prompt 34 — Skill runes that change ability behavior

**Verification follow-up:** Local active-target rebuild and all twenty console
tests passed, alongside 28 targeting checks and 35 new rewards checks.
Chests, affix constraints, rune casts/ownership, cooldown persistence,
save/load, drop caps and summoned-enemy reward exclusion are covered.
Rendered inventory/rune screens were inspected. Widen's ambiguous already-
attached comparison now explicitly shows base/current/proposed values.
This supersedes the build-only verification status in the original result.

**Prompt:** Add one support-rune slot per active talent. Introduce stable
talent IDs and explicit compatibility tags such as melee, projectile,
area, movement, and damaging; migrate talent-owned saved state from name
or position lookup to IDs as part of this change. Resolve a temporary
effective talent from its base definition and equipped rune. Implement
four initial runes: Chain (one additional valid target at reduced damage),
Widen (larger existing area at increased mana cost), Venom (poison on a
successful damaging hit at reduced direct damage), and Swift Passage
(longer pure movement with a longer cooldown). Pick initial numbers during
implementation and expose the complete resulting behavior in the preview.
Add runes to Prompt 33's rewards with an early guaranteed rune choice.

**Why now:** A player can reshape an existing ability around a preferred
playstyle, making the same starting class support different builds.

**Keep bounded:** One rune per skill; no arbitrary trigger chains or
scripting language. Centralize compatibility and effective-talent logic
instead of adding a separate special case to each class. Define how Venom
interacts with an existing on-hit effect before allowing that combination.
Equipping and removing runes must follow an explicit turn/cooldown rule.

**Done when:** Each rune visibly changes at least one real class talent,
invalid combinations explain why they are invalid, and Chain cannot hit
the same target twice or reach an unseen target. Removing a rune restores
the original definition and cannot reset a running cooldown. Save/load
preserves rune ownership and attachment. Attribute scaling currently uses
cooldown tiers: use the base talent's tier so a rune's cooldown modifier
does not accidentally change its damage scaling too.

**Result (2026-09-25):** Added stable talent IDs, typed compatibility flags,
one rune per talent, shared effective-talent resolution, V rune management,
and Chain/Widen/Venom/Swift Passage. Previews and casts share modified paths,
radius, costs and movement; cooldown-based damage scaling retains the base
tier. Changes cost one turn without resetting running cooldowns. First chest
offers a saved, deferred rune choice; even-floor chests and bosses add random
runes. User approved Mage Blink at level 2 (3 tiles, 4 mana, cooldown 4), making
Swift Passage available in a playable kit. The Mage hybrid pool now has seven
options. Save format 8 persists equipment rolls, loot RNG, chest/drop state,
talent IDs/cooldowns and rune ownership/attachments; older saves are rejected.
The Debug game target builds. Existing fixtures were adapted, but automated
tests and an interactive playthrough were not run; runtime verification and
balance tuning remain pending.

## Prompt 35 - Talent trees (implemented; runtime/balance verification pending)

The user approved replacing the borrowed-talent/native-mastery proposal with
ten trees. Current rules: PROMPT_35_DESIGN.md. Starting attributes unchanged;
1 initial tree point, 3 ability points; 2 attribute points and 1 ability point
per level, plus tree points at 5/7/9. Unlock or specialize trees; talents have
three ranks. Runes removed. Four equipment slots include shields.

Catalog, progression, browser/hotbar, ten trees, statuses/passives, equipment
requirements and version-9 persistence are implemented. Debug game compilation
succeeded. No runtime tests or balance playthroughs were performed this iteration.
Previous Prompt 34 results do not validate it. See NEXT_STEPS.md.

The old Prompt 36+ text below predates this change. Reconcile overlapping ailment
and guard mechanics and rune reward references before implementing it.

## Prompt 36 - Ailment combinations (implemented; runtime verification pending)

Adapted to the current tree system: one-charge Marked (+25% direct damage),
Shield Bash/Volley/Arcane Bolt setup abilities, universal C Cleanse, non-refreshable
stuns, recovery immunity and explicit boss limits. Existing flat Guard is reused;
no second Guarded status or finite absorption pool is added. Damage-over-time
ignores Guard/Marked; dodges do not consume Marked. Logs explain consumed combos.

Version 10 saves persist statuses/cooldowns and accept migrated version-9 tree
saves. Full rules and arithmetic examples: PROMPT_36_DESIGN.md. Game compilation
only; runtime, save-migration and balance verification remain outstanding.

## ⬜ Prompt 37 — Enemy intent and encounters with mixed threats

**Implemented locally; runtime verification pending.** See PROMPT_37_DESIGN.md
for the reaction-window contract, encounter budgets and version-11 persistence.

**Prompt:** Give dangerous enemy actions a readable wind-up. Begin with
the Bomber's area attack and the Ogre's stun attack: show the intended
target area, commit to it, then resolve after a clearly documented reaction
window. Build small encounter groups combining existing roles, such as
an Archer protected by a melee enemy or a Shaman behind an Ogre. Replace
uniform-tier populations with floor-budgeted groups containing ordinary
monsters and occasional elites, with a conservative cap on dangerous
combinations. Review how this replaces Prompt 22's player-level tier rule.

**Why now:** Equipment, talent trees, and hybrid builds need varied tactical
problems. A telegraphed threat makes movement and defensive abilities
valuable even when dealing immediate damage is tempting.

**Keep bounded:** Use existing enemy types before expanding the roster.
Intent belongs to game state and must persist through saving. Define a
reaction window in terms of the scheduler's actual action order: a fast
enemy must not begin and finish its wind-up before the player can respond.
Show actual committed actions; do not render a speculative AI choice that
changes invisibly just before execution.

**Done when:** The player can step out of a shown blast, interrupt an
eligible wind-up, and inspect an elite's relevant differences. Loading a
save mid-wind-up restores the same threat. Sampled dungeon seeds respect
encounter budgets and leave a safe starting area. Floor difficulty does
not spike solely because the player gained a level in the previous fight.

## ⬜ Prompt 38 — Floor identities and optional risk/reward rooms

**Implemented locally; runtime verification pending.** Three themes, regional
chest rewards and optional sealed vaults with persisted reward choices. See
PROMPT_38_DESIGN.md for current rules and pending acceptance checks.

**Prompt:** Give the ten-floor run three simple themes using room layouts,
palette changes, enemy composition, and rewards: an early barracks area,
a middle ruined sanctum, and late crypts leading to the Lich. Add one
optional vault room to eligible ordinary floors, with a visible description
of its increased danger and a choice of three generated item or rune
rewards after completion. Offer a route around it so entering is deliberate.
Use the same encounter budgets and reward generation already built.

**Why now:** Exploration should offer decisions about risk and build
direction. Different floor themes also make progress through the run
recognizable without needing a large art production step.

**Keep bounded:** Reuse the current room-and-corridor generator and
simple graphics. No overworld, backtracking campaign, procedural quest
system, or mandatory vaults. Avoid adding hazards until the room and
reward loop itself is working.

**Done when:** A sampled seed set keeps every mandatory exit and boss
reachable, keeps optional-room placement from bypassing protected final
rooms, and never places a vault in the starting room. Its encounter can
be completed with each starting class. A reward can be selected only once;
the offered choices, chosen reward, and encounter completion survive
saving without rerolling or duplicating the reward.

## ⬜ Prompt 39 — Boss fights that exercise the new builds

**Implemented locally; runtime verification pending.** See PROMPT_39_DESIGN.md
for boss warnings, recovery, lifetime ritual limits and save migration.

**Prompt:** Upgrade both existing boss encounters using the systems above.
Keep the floor-5 Goblin Warlord's three-phase identity, but telegraph its
largest attacks and add a recovery opening worth exploiting. Keep the
floor-10 Lich's ranged summoner identity; visibly announce summon attempts,
provide fair positioning opportunities, and review its Skeleton cap and
replacement policy so killing minions has a clear tactical payoff.
Tune boss control resistance using the visible rules from Prompt 36.

**Why now:** The boss fights should reward targeting, defense, movement,
and ability combinations learned during the run. This develops the Lich
already implemented in Prompt 29 instead of scheduling it a second time.

**Keep bounded:** Two existing bosses, no third boss or new phase framework.
Every starting class must have a viable response without needing a rare
drop or one required hybrid talent. Check the actual current summon-cap
semantics before changing them; distinguish a total summon limit from a
simultaneously-alive limit, and record any deliberate change.

**Done when:** Each starting class can complete both fights with sensible
gear, and representative pure and hybrid builds have different useful
answers. Boss attacks cannot resolve without their promised reaction
window. Summons cannot grant unlimited progression or rewards. Saving
mid-fight preserves phases, intents, summon bookkeeping, and statuses;
the Warlord still opens progression and the Lich still triggers victory
after all earned level-up choices have resolved.

## ⬜ Prompt 40 — Full-run balance and a shareable Windows build

**Prompt:** Play and tune the complete ten-floor run as a coherent game.
Use a small recorded set of dungeon/loot seeds and a build matrix covering
all three starting classes, native specialization, and each supported
hybrid pairing. Record floor-by-floor character level, approximate fight
length, deaths or dangerous damage spikes, rewards found, and when the
build's defining choices became available. Tune XP, reward frequency,
enemy budgets, costs, and cooldowns from those observations. Keep the
level-10 cap unless play evidence supports a separately explained change.

Check that the specialization choices at levels 5, 7, and 9 arrive early
enough to use for a meaningful part of the run. Remove obvious dominant
options and document remaining balance questions instead of claiming
exhaustive balance from a few playthroughs. Add concise in-game controls
and descriptions for the new systems, then produce a Windows Release
package containing the executable, required runtime dependencies, fonts,
sounds, licenses, and a short start guide.

**Why now:** This is the integration pass for the preceding nine prompts,
with an actual build someone else can play and give feedback on.

**Done when:** Relevant automated checks and recorded playthroughs pass;
save/load is exercised at floor transitions, reward choices, specialization
choices, and boss wind-ups. The packaged build starts from a separate
folder without the source tree or build-directory asset paths, and clean
machine/runtime requirements are verified or explicitly documented if that
environment is unavailable. Update the build instructions and known issues
with actual results. Creating the package completes this prompt; public
upload or distribution is a separate action.

## ⬜ Prompt 41+ — revisit after the build-variety phase

Possible later work: additional starting classes, a larger talent-category
tree, elemental resistances and damage conversion, crafting, more rune
slots, unique items with complex triggers, new biomes, and additional
platform packages. Sequence these from play feedback after Prompt 40.
Keep the first ten-floor run understandable and enjoyable before expanding
the number of interacting systems further.

## Decisions locked in before starting Prompt 13

- **Naming: "Spellblade" stays** -- not renamed to "Templar." No
  mechanical difference either way; this was purely a flavor question,
  settled in favor of the name already established since Prompt 9.
- **Dodge is symmetric, and monsters get real attributes because of
  it** -- see Prompt 14 above. This was the one design choice in this
  phase with a real trade-off (more consistent vs. swingier fights
  against Dexterity-flavored content); decided in favor of consistency.
- **Exact attribute formula numbers** (Prompt 14) still aren't drafted
  here on purpose -- worth designing live when we get there, the same
  way Spellblade's actual talent numbers were designed live in Prompt 9
  rather than pre-decided in the original `ROADMAP.md`.
- No other changes to the Prompt 13-17+ shape as drafted.

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
