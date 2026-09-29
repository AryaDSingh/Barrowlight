# Development checkpoint — 2026-09-29

## Cleanup pass (2026-09-29), before Prompt 35

- Prompts 32-34 now have tests: `loot_test` (inventory, effective stats,
  LootGenerator determinism/tiers/affix rules/rarity weights), `rune_test`
  (compatibility against the real kits, effective values, attachment
  bookkeeping) and an extended `savegame_test` (items, loot RNG, chest,
  runes, and rejection of invalid item/rune states).
- Deleted the stale pre-rename `Archer/Fighter/Marauder/SorcererTalents`
  sources and tests; nothing referenced them.
- `Application.cpp` split by concern into `ApplicationTurns.cpp`,
  `ApplicationProgression.cpp`, `ApplicationRender.cpp` and
  `ApplicationSave.cpp` (verbatim moves). Shared constants: `core/GameRules.hpp`.
- GCC/Clang builds use `-Wall -Wextra -Wpedantic` and are warning-free.
- Missing font no longer aborts debug builds (text is skipped); the
  class-select screen shows Warrior/Mage instead of Fighter/Sorcerer.
- Verified on Linux: full build, all 23 tests pass, and a headless
  playthrough (class select, movement, F5 save, F9 load) under Xvfb.
- Not changed: Prompt 35 still awaits the user's decision; scheduler
  energy is still deliberately not saved (see SaveGame.hpp).


## Current state

Prompts 31-34 are implemented. Prompt 35 has a concrete proposal in
`PROMPT_35_DESIGN.md`; its replacement rules await user agreement. No Prompt 35
gameplay code has been implemented. The user's latest request was to proceed
and upload a checkpoint/handoff if usage became tight. At the start of this
checkpoint turn, 9% of the five-hour allowance remained.

The checkpoint includes all related work already present in the working tree,
including earlier Lich and class-renaming changes. It is not limited to the
last two prompts. Build output, runtime saves and local IDE state are ignored.

## Implemented features

- 31: manual mouse/keyboard targeting, trajectory/area previews, enemy inspection.
- 32: three equipment slots, pickups, inventory/comparisons, separated base stats.
- 33: normal/magic/rare gear, eight affixes, independent persisted loot RNG,
  bounded drops, one chest per floor, boss rewards, no summon loot or XP.
- 34: Chain/Widen/Venom/Swift Passage, one rune per talent, rune management,
  stable talent IDs and ID-based saves. User explicitly approved Mage Blink at
  level 2. Mage's hybrid pool now contains seven talents.

Controls: G pickup/open chest; B equipment; V runes; I enemy inspection;
1-9 talents, PageUp/PageDown talent pages; F5/F9 save/load.
First-floor chest offers a rune choice through V then 1-4.

## Verification and compatibility

- Final Debug game build after Prompts 33-34 succeeded, using VS2022 CMake.
- `git diff --check` passed at that checkpoint.
- At the time, no tests were added/run for Prompts 32-34; see the cleanup
  pass above for the tests since added.
- Current save format is **8**; older saves are deliberately rejected.
- Loot pacing, rune balance and new UI layouts need gameplay verification.
- Saves preserve monster talent cooldowns, tiers and reward flags, but not
  scheduler energies or all AI-internal counters. Summons remain ineligible
  for rewards even if a summon counter restarts after loading.
- R remains a development regeneration/healing shortcut; reward budgets assume
  normal travel through floor doors.

## First action for the next session

Read `PROMPT_35_DESIGN.md` and check whether the user has agreed to it or
requested changes. ROADMAP Prompt 35 says to settle the replacement rules
first. Do not infer agreement from the existence of this proposal. Then follow
its implementation order. Keep the stable-ID/rune/save architecture intact.

Key files:

- `src/core/Application.cpp`: input and level setup; `ApplicationTurns.cpp`,
  `ApplicationProgression.cpp` (level-up), `ApplicationSave.cpp` (saving/loading).
- `src/core/ApplicationTargeting.cpp`, `src/world/TalentTargeting.cpp`: previews.
- `src/core/ApplicationInventory.cpp`, `ApplicationLoot.cpp`, `ApplicationRunes.cpp`.
- `src/entities/HybridSpec.cpp`, `PlayerLeveling.cpp`, `PlayerClassFactory.cpp`.
- `src/entities/TalentSet.cpp`, `Talent.hpp`, `Rune.hpp`: effective talent rules.
- `src/entities/Item.hpp`, `LootGenerator.hpp`: equipment definitions and rolls.
- `src/core/SaveGame.hpp/.cpp`: versioned persistence.

Build from repository root with PowerShell:

```powershell
& 'C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' --build build --config Debug --target roguelike --parallel 4
```

Executable: `build/bin/Debug/roguelike.exe`. Launch from the repository root
so assets resolve. SDK access may require an approved build outside the sandbox.
