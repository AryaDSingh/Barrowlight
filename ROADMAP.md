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

## ⬜ Prompt 16 — Shadow (Dexterity + Intelligence) + end-game screens
Third class, completing the initial triangle. **Shadow**: agile
assassin/rogue-caster hybrid, reusing existing mechanisms in new
combinations rather than inventing more -- mobility talents building on
Blink's precedent, a "Backstab"-style conditional-bonus talent (reusing
Execution's conditional-multiplier mechanic from Prompt 9), and
player-castable Poison (currently only Spider can inflict it -- the
player has never had access to it). Also: proper on-screen "You died" /
"You have won" screens (using Prompt 13's text rendering), replacing the
"window just closes" behavior from Prompts 10-11 -- closes that
long-flagged gap too.

## ⬜ Prompt 17+ — open-ended, lower priority
Camera/viewport for dungeons bigger than one screen (flagged since
Prompt 5). Sound (`SFML_BUILD_AUDIO` has been off since Prompt 2,
deliberately, until there was a reason to want it -- there's a reason
now). Additional classes to round out the full PoE-style set (Duelist,
Ranger, Witch, Scion) once the 3-class pattern is proven. Packaging/
distribution for actually publishing. Not sequenced precisely yet --
revisit once Prompts 13-16 are done and it's clearer what matters most.

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
