# Development checkpoint — 2026-09-25

## Current state

### Latest follow-up: cleanup and local verification completed

- Removed the 12 obsolete Marauder/Fighter/Archer/Sorcerer talent and test
  files after confirming their references were confined to one another.
- Rebuilt the game and all active tests locally. Twenty console targets pass,
  targeting integration passes 28 checks, and the new rewards integration
  passes 35 checks. See `tests/application_rewards_test.cpp` for exact scope.
- Widen worked mechanically; its current-to-proposed comparison was unclear
  when already attached. Rune UI now shows base/current/proposed values and
  explicitly labels an already-attached rune. Inspected rendered snapshots.
- Test snapshots/logs are under `build/rewards-checks` and
  `build/targeting-checks`; these generated artifacts are not committed.
- Checkpoint branch: `codex/cleanup-runes-verification`.
- **Next:** settle `PROMPT_35_DESIGN.md` with the user before changing gameplay.
  No new agreement on Prompt 35 has been received during this follow-up.

The paragraphs below preserve the original checkpoint context; the verification
update above supersedes its earlier untested status.

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
- Follow-up tests cover chest/rune/reward behavior and their save/load paths;
  see the latest update above. A full run and broader balance testing remain.
- Current save format is **8**; older saves are deliberately rejected.
- Loot pacing and rune balance still need a complete gameplay evaluation.
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

- `src/core/Application.cpp`: turn commitment, level-up sequence, saving/loading.
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
