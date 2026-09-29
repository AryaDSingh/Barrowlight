# Automated playtest pass — 2026-09-27

## Result

Debug game and test targets built successfully. All **22 test executables passed**,
with 430 labelled checks (100 through application handlers). These are controlled
scenarios and sampled generation checks, not a complete human campaign playthrough
or a balance certification. Several older tests exercise retained legacy kits.

Results: `build/playtest-pass/results.json`. Each executable has an isolated
working directory and `output.log` beneath `build/playtest-pass/`. Application
screenshots are under its nested `build/rewards-checks/` or `build/targeting-checks/`.
Normal player saves and Codex profiles were not used or overwritten.

Repeat after building Debug targets:

```powershell
C:/Python313/python.exe tests/run_suite.py
```

## Coverage added or strengthened

- Actual save parsing/restoration is now required by application round-trip tests.
  The previous reward assertions could pass after a failed load, because the
  original state had not changed. Missing stair coordinates were fixed in fixtures.
- Real Lich factory/AI: healthy Mana Drain, wounded Doom, clear-line requirement,
  Hex cooldown, exact Doom expiry damage, and a real Cleanse cast on its last turn.
- Curses interrupt rest, exploration and Waystone use; quiet healing is blocked
  while cursed and resumes after ten quiet turns.
- Scheduled ritual release produces Guard/Archer/Guard; attempt counts do not
  double-increment, the cap holds, and summons are reward-ineligible.
- Committed enemy blast kills another enemy while excluding its caster.
- Elite/Rare/named loot bypasses the ordinary cap with the intended rarity floors;
  ordinary cap and summon reward exclusion persist through genuine save/load.
- Doom, Hex cooldown and committed ritual round-trip; a representative synthetic
  version-16 Lich save migrates with the ten-turn Hex grace period.
- Descending, saving/loading cached floors, then ascending restores loot identity
  and arrival position without respawning that floor.
- 60 actual generated states (three classes times 20 floors) serialize and parse.
- 500 encounter layouts (25 seeds per floor): walkable/non-overlapping spawns,
  entrance safety distance, ordinary opening pack and Rare/unique caps, plus
  evidence that new undead, Rare and named variants occur.
- Rank 2/3 purchases preserve a manually arranged hotbar, including empty slots.
- Auto-explore moves, stops without bump-attacking visible enemies, stops for new
  loot and offscreen warnings, acknowledges visible loot on restart, and finishes
  a fully explored floor. One full open-floor simulation completed in 166 moves
  with two discovery stops. The initial 140-step bound was too restrictive;
  extending it demonstrated completion, not an infinite loop. Routing efficiency
  remains a player-experience question.
- Shared line-of-fire rejects blocked corners both ways and is symmetric over
  2,401 endpoint pairs in a representative obstacle arrangement.

## Fixes from this pass

- Shortened the controls row after screenshots exposed overlap with the talent
  panel; inspected the regenerated screenshot to confirm separation.
- Cleanse's action log now explicitly includes curses.
- Updated stale test expectations for level 20, Rare display names, Warlord
  behaviour/HP and the application-owned Lich commitment counter. Added isolated
  test runner and durable result logs.

## Gaps for manual playtesting

1. **Boss difficulty:** fight Warlord and Lich at ordinary levels/gear. Do telegraphs
   allow a useful counterattack window? Can melee builds reach a retreating Lich?
   Controlled tests often use boosted HP or selected positions and do not answer
   these balance questions.
2. **Doom/Cleanse choices:** is the countdown clear, and is taking the hit ever a
   reasonable alternative to cleansing? Try Cleanse already on cooldown.
3. **Crypt combinations:** chilling archers plus guard cleaves; Rare guards and
   Bonecaller buffs; summon crowding; wall/corridor fights with friendly fire.
4. **Exploration feel:** Z in narrow generated corridors, item-heavy rooms, near
   vaults and with friendly skeletons. Assess stop frequency and backtracking.
   Real keyboard/mouse/focus-loss cancellation was not driven by the automated
   harness; verify a key or click stops before performing an unwanted action.
5. **Loot pacing:** whether guaranteed special drops feel worth the risk, and
   whether denser packs plus better drops accelerate progression too much.
6. **UI known gap:** long Lich inspection text exceeds the current panel height,
   clipping later ability detail. Earlier summary/counterplay remains visible.
   Multi-status HUD readability and overlapping warnings need human review. This
   is still a limitation for the planned UI pass, not a fixed issue.
7. **Broader coverage:** all hidden-tree combinations, repeated mixed-status fights,
   every historical save version, full campaign victory/death, and inventory/
   equipment edge cases were not exhaustively exercised. Legacy migration coverage
   here specifically targets version 16 plus current-format cached floors.

Tier 1 still needs human acceptance. This pass does not complete or publish the
previously deferred packaging/release work.
