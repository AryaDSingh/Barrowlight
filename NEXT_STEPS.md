## Latest checkpoint - death modes (2026-09-28)

- Choose Roguelike (default, one life) or Adventure (two extra lives) before
  choosing a class. Click the mode button or press M.
- Adventure death screen offers Revive in town/R. Spends one extra life,
  restores HP/mana/cooldowns, clears player effects and dissolves summons.
  Gear, progression, enemy damage and floor rewards persist. Returning to the
  dungeon arrives at its entrance or the nearest free connected tile.
- Deathless triggers before any Adventure revival. No milestone life grants yet.
- Save version 21 preserves mode/lives; versions 9-20 default to Roguelike.
  Floor travel carries the current life count, not archived character values.
- Not built or tested this pass. Playtest death, revival, saving and floor travel.
- Next structural item: town progression/quests. Prompt 40 remains deferred.

---

## Latest checkpoint - depth-based difficulty correction (2026-09-28)

- Removed first-entry player-level scaling. Enemy HP, STR/INT and XP depend
  only on global depth; loot uses that same depth directly.
- Chooser and HUD show fixed depth levels. Ruins spans 1-10; Crypts 11-20.
- Save version 20 migrates versions 9-19, including cached floors, preserving
  remaining enemy health proportion. Allied summons keep talent scaling.
- Not built or tested for this correction. Earlier check counts below predate it.
- Next: playtest deeper-floor balance; then death-mode selection.

---

## Latest checkpoint - selectable dungeons and locked entry levels (2026-09-28)

- Town Choose dungeon/M: Ruins and Deep Crypts, ten freely selectable depths each.
  Preview the suggested level, lock and visited status; Enter commits, Esc leaves.
- Reuses persistent floor IDs/maps, with no refresh on revisit. Current depth
  resumes exactly; other visited depths use their entrance. Arrival costs no turn.
- First entry fixes the baseline within its band. Enemy health/attributes/XP and
  loot quality use the fixed baseline, including later generated floors and loads.
- Save version 19 accepts 9-18; older visits retain baseline difficulty.
- Build passed; 155 reward/progression + 29 targeting + 21 save checks passed.
  Reviewed chooser/town screenshots. See DUNGEON_SELECTION.md for exact rules.
- Next structural item: death-mode selection (Roguelike/Adventure), then town
  progression/quests. Playtest deep-entry risks and reward balance before tuning.
  Prompt 40 remains deferred.

---

## Latest checkpoint - focused UI acceptance (2026-09-28)

- Debug game builds; 29 targeting + 134 reward/menu checks pass (163 labelled
  checks including screenshots). Real event dispatch is exercised with hidden
  SFML windows, isolated saves and Codex profiles.
- Reviewed rendered screenshots and fixed overlapping attribute descriptions,
  equipment names, binding labels, talent/vault buttons, rank table headings
  and town footer placement. See UI_ACCEPTANCE_PASS.md for scope and evidence.
- tests/run_ui_pass.py repeats the focused pass after building its two targets.
  The earlier full 22-executable suite was not rerun for this UI pass.
- Still needs user feedback: drag/drop feel, display scaling, combat pacing and
  equipment power. This is not a full human campaign playthrough.
- Next: selectable dungeon/depth progression with first-entry difficulty locking,
  following Tier 3 and the current design documents. Prompt 40 remains deferred.

---

## Latest checkpoint - dungeon actions and restart (2026-09-27)

- Added an eleven-button action bar above the map: Bag, Trees, Codex, Use,
  Wait, Rest, Explore, Town, Cleanse, Save and Load. Buttons call the existing
  gameplay actions and keep their normal costs and restrictions.
- Aiming/inspection shows Cancel instead. Rest and auto-explore show Stop;
  existing input interruption consumes the first click before another action.
- Death/victory screens have New character and Codex buttons. Class selection
  has Load and Codex buttons, plus the latest log message for load failures.
- Menu mouse clicks are consumed after transitions. Keyboard shortcuts remain.
- Debug game build passed. No automated or live UI tests run for this slice.
- Next: focused UI acceptance across the newly added menus and controls,
  including full bags, two ring slots, point spending and click-through guards.
  This is not a claim that all interfaces are mouse-complete (for example,
  inventory dropping remains D). Prompt 40 is still deferred.

---

## Latest checkpoint - Codex mouse controls (2026-09-27)

- Click the four family cards, use Previous/Next, or scroll the wheel to browse.
  Close, Save knowledge and Consult town book/rumour buttons use the existing
  keyboard handlers. Consulting remains free and only available in town.
- Town has a Codex/books button. Closing the Codex consumes the click so it
  cannot trigger the underlying shop, inventory or map.
- Family names and discovery status now occupy separate lines to fit the cards.
  Unrevealed families still hide their names; knowledge and save rules retain
  the same behavior.
- Debug game build passed. No automated or live UI tests performed; there is
  no standalone Codex test target in the current repository.
- Next: remaining navigation gaps (game-over/restart and in-dungeon menu/action
  buttons), then a focused UI acceptance pass. Prompt 40 remains deferred.

---

## Latest checkpoint - travel and vault mouse controls (2026-09-27)

- Descent menu has clickable Descend, Return to town and Stay buttons. Mouse
  and keyboard share the same handler and existing danger/quiet-turn checks.
- Vault warning has Open and Leave sealed buttons. Reward cards select without
  claiming; the separate Claim button commits for one turn. Decide later closes
  the menu. A full bag is explained on screen without consuming a reward.
- Reward comparisons use the preferred equipment slot, including a free ring
  slot. Menu clicks are consumed even after closing to prevent map click-through.
- Debug build result: passed. No automated or live UI tests run for this slice.
- Next: Codex mouse controls and remaining navigation gaps. Prompt 40 stays
  deferred; combat balance awaits the user's playtest feedback.

---

## Latest checkpoint - town shop mouse controls (2026-09-27)

- Buy/Sell toggle buttons, item row selection and paging; hovering/selecting
  previews the stock or bag item, and a separate trade button commits.
- Buttons also open equipment, recover at the inn and resume the exact floor.
  Existing keyboard controls, prices, bag limit, and sale restrictions remain.
- Full bag, insufficient gold, training items and gold cap still use the same
  checked trade path as keyboard Enter. UI click events stay within the town.
- Debug game build passed. No tests or live UI playthrough performed.
- Next: floor travel/exit and vault choices, then Codex mouse controls. Prompt 40
  remains deferred; awaiting combat playtest feedback.

---

## Latest checkpoint - class and attribute mouse controls (2026-09-27)

- Class selection now uses three hover-highlighted cards. Clicking Warrior,
  Mage or Thief calls the same class setup as keys 1-3; the starting attributes
  and tree pools are unchanged.
- Attribute allocation uses clickable Strength, Dexterity and Intelligence
  options. Each click spends exactly one point and retains the existing effects,
  refreshes equipment stats and continues the level-up sequence. Keys 1-3 remain.
- Mouse movement updates hover state. Clicks on these full-screen menus are
  consumed, so choosing a class or stat cannot trigger a map action beneath.
- Corrected the class screen's load hint to list supported save versions 9-18.
- Debug game build passed. No tests or live UI playthrough were run.
- Next: town buy/sell, floor travel/exit, vault choices and Codex mouse input.
  Review/combat playtest feedback is still pending; Prompt 40 remains deferred.

---

## Latest checkpoint - talent menu mouse controls (2026-09-27)

- Click visible tree and ability rows to inspect/select without spending points.
  Wheel over the tree list or use Previous/Next tree buttons to browse.
- Separate Unlock/Specialize and Learn/Rank up buttons show their point cost;
  unavailable buttons use existing prerequisite validation and feedback.
- Assign hotbar opens a modal picker showing all eighteen current bindings,
  labelled by page/key. Click a slot to replace it, or Cancel/Esc to leave it.
  Imbue retains its element cycling button and keyboard V behaviour.
- Continue honours initial tree/ability requirements. Menu mouse events are
  consumed even when closing, preventing clicks from reaching the map below.
- Keyboard controls and save format 18 retained. No tests or runtime UI
  playthrough performed for this slice. Debug game build passed.
- Next: mouse controls for class selection, attribute spending, town trading,
  vault/travel choices and Codex. Combat playtest feedback remains pending;
  Prompt 40 remains deferred.

---

## Latest checkpoint - inventory/equipment (2026-09-27)

- Eleven equipment slots in a paper-doll layout; original slot IDs preserved.
  Armour is now labelled Body and Charm is labelled Amulet. Two interchangeable
  ring slots; default equip fills an empty ring slot before replacing Ring 1.
- 50-item bag, twelve rows per page. Hover compares bonuses; right-click equips
  or removes; drag bag items onto compatible slots, equipped items into the bag.
  Mouse actions cannot click through to the map. Keyboard controls retained.
- D drops a selected bag item in the dungeon for one turn. Town uses selling.
  Normal pickups, chest/vault claims and purchases refuse a full bag before
  spending resources or consuming rewards. Old saves, starter training grants
  and guaranteed boss rewards may retain overflow; no owned items are discarded.
- Thirteen additional base items enter loot and town stock. Additional slots
  have modest base stats, but combined affix power still needs playtesting.
- Armour type uses head/body/hands/feet majority, empty pieces count as cloth,
  ties favour body. Old body-only heavy/light outfits may now count as cloth.
- Save version 18 accepts versions 9-17. Existing item IDs remain valid.
- Debug game build passed. No automated tests or visual playthrough for this
  slice; the earlier 22-test pass predates these changes.
- Next: mouse controls for talent selection/spending and remaining menus.
  Prompt 40 stays deferred. Await the user's combat playtest feedback.

---

# Development checkpoint - 2026-09-26

## Current direction

GAME_DESIGN.md is the flexible game vision. Weigh choices by player clarity,
interesting decisions and enjoyment, and revise after playtesting. Its new lore,
Codex and knowledge-persistence design is accepted as future direction; those
systems are not yet implemented. Repeated hints should become actionable before
scarce build-point commitments (proposed refinement, not a locked rule).

## Implemented locally

- Prompt 35: ten trees / forty talents / three ranks, points, starting access,
  specialization, hotbar and rune removal. Starting stats unchanged.
- DEX/rank/distance-based stealth checks.
- Prompt 36: Marked, C Cleanse, stun recovery and boss limits.
- Prompt 37: Bomber/Ogre committed warnings, reaction windows, interruptions,
  floor-budgeted pairs, elite inspection and saved intents.
- Prompt 38: floor themes, regional chest rewards and optional vaults. Read
  PROMPT_38_DESIGN.md for the current rules and pending verification cases.
- Ground targeting: empty casts permitted, normal costs still apply, visible
  affected enemies outlined. Terrain/range/equipment/prerequisites remain.

Current save format: **14**. Versions 9-13 migrate; <=8 require a new run.
Old maps/populations remain as saved; new floors get current generation.

## Vault controls

Purple V: stand adjacent and G to read the warning. Enter opens for one turn;
Esc leaves it sealed. Elite Goblin + Archer; retreat is allowed. Their combined
seven-point cost is reserved from the existing floor budget. After both die,
G at/adjacent to the cache opens three rare reward choices; arrows select,
Enter claims one for one turn, Esc defers. F5/F9 work in both vault menus.

## Verification and publication

- Town/travel Debug build passed after fixing save-helper stream signatures.
- No tests added/run or runtime/visual playthrough performed for current work.
- Existing targeting tests still have enemy-required assumptions to update when
  verification is requested. Historical pass counts do not cover these changes.
- Current branch: codex/cleanup-runes-verification. Last push: 970e81c.
  Current tree/status/theme/targeting/vault work remains uncommitted/unpushed.

## Next

Prompt 38 implementation is complete, with runtime acceptance checks pending.
Prompt 39 is implemented; see PROMPT_39_DESIGN.md. **Prompt 40 is deferred at the user's request.** Town/travel work follows the new design direction.
Respect the design doc's preference to stabilize through playtesting when asked.
Known risks: vault frequency and skip/reward balance, changed theme layouts,
control/stealth uptime, flat passives and late weapon availability.
Scheduler energy still resets on load; Warlord phase/enrage bookkeeping and Lich ritual count now persist. No elemental
resistances or stealth pursuit memory. R is now safe fast waiting.

## Build

From repository root (for asset paths):

```powershell
& 'C:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe' --build build --config Debug --target roguelike --parallel 4
```

Executable: build/bin/Debug/roguelike.exe.

## Prompt 39 checkpoint

Warlord blasts: 2-action warning; enraged melee: 1-action warning. Heavy attacks
and interrupts yield one player action of recovery. Lich bolts: 1-action warning;
rituals: 2-action purple warning, blockable by occupying the marked tile. Three
lifetime ritual attempts, including interrupted/blocked ones; no replacements,
XP or loot from summons. Fresh skeletons wait for a player action before acting.
Legacy saves <=12 load the Lich with rituals exhausted because dead-summon counts
were never stored; fresh fights and version-13 saves track the exact count.
No tests/playthroughs were run; avoid presenting these rules as verified balance.

## Town/travel checkpoint

Read TOWN_TRAVEL_DESIGN.md. Persistent floor backtracking, downstairs choice,
H Waystone after 10 quiet turns, R bounded/interrupted fast waiting, menu town,
buy/sell basic gear, free inn recovery and version-14 campaign saves are built.
No tests/playthroughs run. Previous floors cannot be reconstructed from <=13
saves, but newly explored floors persist. Current progression and inventory are
never restored from cached character snapshots when travelling.

Town: D resumes the exact dungeon location, R heals/refills cooldowns, B manages
equipment, Tab switches buy/sell, arrows select, Enter trades. Shop prices and
inn recovery are provisional. The Lich still ends the existing ten-floor run.
Next feature selection should follow GAME_DESIGN.md; Prompt 40 remains deferred.
Potential next bounded step: armour types/trees or Codex foundations, depending
on user preference. Keep the level cap until additional content needs it.

## Out-of-combat HP recovery

User rejected mandatory inn trips for routine recovery. Starting at ten quiet
turns, each completed safe player action heals 5% maximum HP rounded up, capped
at full HP. R now waits for HP as well as mana/cooldowns/readiness. Combat,
visible enemies, intents and harmful effects suppress healing and reset the
delay. No additional combat healing or save fields were introduced.

### Mana balance follow-up

Combat mana regeneration is 1 per player turn; safe regeneration stays 2.
Combat uses the shared danger check plus attacks during the current turn.
Fire, Ice, Lightning and Arcane active spell costs are doubled at every rank,
including Blink. Arcane Efficiency applies afterward. Martial, Bow, Stealth
and Acrobatics costs are unchanged; free universal actions stay free. New
costs appear in the tree browser and live previews and apply to loaded builds.
No save-format change. Balance remains provisional pending playtesting.

## Armour types foundation

Explicit ArmourKind metadata now identifies Cloth (Woven Robes), Light (Scout
Leathers), Heavy (the existing Chain Coat), or Unarmoured (empty slot). The
inventory, shop and vault cards show the type. All classes retain access to all
armour; bonuses and equip rules are unchanged. Stable definition IDs make this
work for existing saved gear without migration. Armour talent trees are next;
they are not implemented by this metadata/UI change.


## Armour talent checkpoint

- Implemented Cloth/Unarmoured, Light and Heavy trees: 12 new talents, 3 ranks each.
- Existing level-5/7/9 tree points unlock them; class starting pools stay intact.
- Matching equipment required, with live passive checks and visible menu requirements.
- Stable IDs use the existing save layout; see ARMOUR_TALENTS_DESIGN.md.
- Next: gather armour play feedback, especially Light + Bow/Acrobatics stacking,
  then select the next living-design feature. Prompt 40 remains deferred.


## Codex / lore checkpoint

- J opens a turn-free Codex overlay from all screens, preserving the underlying
  menu; J/Esc returns. Opening stops fast rest.
- Three unknown/hinted pages, seven fragments: three town rumours/books and
  four guaranteed first-defeat discoveries (Archer, natural Skeleton, Warlord, Lich).
- Town J then L collects the selected page's rumour. Summons grant no lore.
- Separate version-1 `codex.txt` profile persists across death, new characters
  and campaign loads; temporary/backup writes preserve prior knowledge.
- Save layout 14 unchanged. No hidden trees, new points or combat bonuses.
- Next decision: reconcile hidden-tree costs with the level-10 point budget
  before implementing Spellblade. Two specialized trees already cost all four
  available tree points. More content/progression or a revised prerequisite is
  needed; do not silently grant bonus points or raise the cap.
- Prompt 40 remains deferred; no tests or interactive playthrough run.


## Hidden trees / post-Lich checkpoint

User approved extending progression and the proposed defaults. Implemented four
hidden trees (16 ranked talents plus linked Imbue variants), level/floor cap 20,
odd-level tree points through 19, and a provisional Deep Crypts band. Floor 10
Lich now opens a descent and grants the Animation relic; floor 20 is final.

Allied summons support enemy targeting/intents, no player friendly fire, ally
kill rewards, cap dissolution, swapping, Grave Pact and persisted lifetimes.
Hidden conditions/relics are per run; Codex revelations persist across runs.
Format 15 reads 9–14 with floor-10 migration. See HIDDEN_TREES_DESIGN.md for
precise rank values, accepted defaults and limitations.

Next: play feedback on progression pace and hidden-tree combat. Ascendancy,
dungeon/depth selection and original Deep Crypts bosses remain future features.
Prompt 40 remains deferred; no tests or interactive playthrough run.

Final integration review corrected minion Chill handling: it slows movement on
alternate turns, preserving adjacent attacks, consistent with ordinary enemies.


## Priority Tier 0: hotbar and line-of-fire fixes

Read PRIORITIES.md and PLAYTEST_NOTES (1).md. These now guide the next work;
DIRECTION_REVIEW.md and ASCENDANCY_DESIGN.md are referenced but not present.

- Rank purchase explicitly preserves the complete hotbar, including empty slots.
  Existing rank code did not auto-fill; the reported Blink incident was not
  reproduced. Bare number keys in the tree screen did directly replace slots,
  a plausible UI cause. Assignment now requires B then 1-9 (Shift: page 2); Esc
  cancels assignment. A remains learn/rank up.
- Shared canonical, symmetric line-of-fire geometry now handles player rays,
  direct spells, chaining, area propagation and ranged AI. Both diagonal corner
  sides must be open. Open-cell FOV uses the same geometry; wall-face visibility
  retains shadowcasting. Enemy ranged attacks require a clear lane; unseen targets are not attacked.
- Warlord cornered melee now requires orthogonal adjacency, as player melee and
  Chaser attacks already do. Telegraph blast effects cannot propagate through
  walls from the marked center.
- No save layout change. Current loaded hotbars stay as saved; already lost
  bindings cannot be reconstructed, so rebind them once using B + number.
- Pending player acceptance: Blink at slot 7, rank 2 and 3 without moving it;
  Lightning Bolt still bound; save/load preserves both; corner shots blocked in
  both directions; adjacent melee only. No automated tests or interactive
  playthrough performed in this pass.
- Next after Tier 0 play feedback: Tier 1 enemy strength and telegraph pressure.
  UI, equipment-slot expansion and ascendancies wait behind those priorities.

Tier 0 Debug game build passed; git diff --check reported no whitespace errors.


## Tier 1 encounter variety checkpoint (2026-09-27)

- New goblin raiders mark targets for pack follow-up hits. Barracks/Sanctum
  packs now contain three enemies, with ranged, bomber or support back lines.
- Crypt packs use skeletons, chilling Skeleton Archers, Skeleton Guards with
  committed cleaves, and Bonecallers that empower nearby allies.
- Floor encounter budget is now 20 + 3 per depth (was 14 + 2); vault cost and
  entrance safety distance remain. Groups only spawn when the whole pack fits.
- Existing Elite tier guarantees magic-or-better gear. The serialized Nightmare
  tier is displayed as Rare, keeps 2.2x HP / 1.8x damage / 4x XP, and guarantees
  rare gear. At most one layout-selected Rare per floor from floor 4, budget
  permitting; none in the opening pack or bomber/Ogre-Shaman packs.
- Named encounters: Grik the Packleader (marking melee), Veyra the Ashkeeper
  (wide blasts). At most one on floors 3/8/13/18, when a safe budgeted room fits;
  they do not lock stairs. Gold borders, named inspection, guaranteed rare gear.
  These are fixed encounter variants, not random affix combinations or new
  unique equipment. Special drops bypass the two ordinary-drop floor cap.
- Bomber, Warlord blast and Ashkeeper: radius-2 diamond, three player actions.
  Skeleton Guard: radius-1 diamond, two actions, then one action recovery.
  All committed damaging attacks can strike other monsters except their caster;
  walls clip damage and the visible warning identically. Friendly-fire kills
  grant normal rewards so baiting enemy attacks remains useful.
- Save format 16 reads 9-15 as well; legacy in-progress radius-1 blasts retain
  their old area and timer, including after resaving. Existing visited floors
  retain their population; new packs appear on newly generated floors/new runs.
- Balance is provisional. Playtest raider + archer bursts, Chill near guard
  cleaves, rare guard damage, and whether three-action blasts create pressure.
- Remaining Tier 1: broader boss/curses work, varied telegraph shapes, Z auto-
  explore, and player acceptance of Tier 0 plus Prompts 32-38. No automated
  tests or live playtest were run for this pass.

Build status: Debug game target compiled successfully. Automated tests and live
playtesting remain pending.


## Tier 1 auto-explore checkpoint (2026-09-27)

- Z starts exploration toward the nearest unseen frontier. Breadth-first search
  traverses discovered walkable tiles only; unseen terrain is not used to route.
- One ordinary movement action per 100 ms, recalculated after every step. Enemy
  turns, cooldowns, regeneration, FOV and allied skeleton swaps use normal movement.
- Stops for visible enemies, enemy intents, harmful effects/combat, damage, menus,
  new visible loot/chests/stairs/vaults, stepping on points of interest, blocked
  routes, exhausted reachable frontiers, input or focus loss. A bounded step count
  is a fallback against unexpected loops. There are no automatic bump attacks.
- Starting Z acknowledges currently visible discoveries. This lets players leave
  unwanted loot behind without being interrupted every frame. New discoveries
  still stop immediately after the movement action that reveals them.
- No automatic collection, chest opening, vault opening or stair travel. The
  descent menu remains manual while exploring. Auto-explore state is transient:
  loading a save or generating a floor cancels it, without a save format change.
- Remaining Tier 1: Warlord/Lich and curse work, more telegraph shapes, and live
  acceptance of encounters, auto-explore, Tier 0 fixes and Prompts 32-38.
- No automated tests or live playtest were run for this implementation.

Auto-explore build status: Debug game target compiled successfully.


## Tier 1 boss and curse pass (2026-09-27)

- Warlord: new-fight HP 115 (was 90), basic total melee 7 (was 6).
  Phase 2 advances/melees during Fury cooldown rather than waiting or retreating.
  Phase 3 enrage adds 4 damage (was 6), paired with a radius-1 cleave and two
  player actions to escape. Radius-2 Fury retains three actions. Committed
  attacks still hit other enemies and have one action of recovery.
- Lich: new-fight HP 150 (was 110), base bolt total 10 (was 9). Its three
  ritual attempts now produce Guard, Archer, Guard. Interrupted/occupied rituals
  still spend an attempt; no summon XP or loot. Deep-floor scaling applies to
  these enemy summons. Player Animation summons are unchanged.
- Grave Hex: range 6 and clear sight; 10 enemy-turn cooldown. Above 60% Lich HP,
  Mana Drain removes 2 mana on each of four status ticks. At/below 60%, Doom
  applies instead (also used against a target with no maximum mana).
- Doom: 10 + caster INT/4 HP damage on expiry, unaffected by Guard/dodge.
  Five initial status ticks leave four player actions after the immediate
  post-cast tick. It does not damage early; Cleanse removes it without detonation.
  Existing curses are not reapplied while active. Breaking sight prevents new
  casts but does not remove an existing curse. Healing can prepare for the hit.
- Universal C/Cleanse now removes Mana Drain and Doom as well as its old ailments.
  Both curses interrupt resting, auto-explore and quiet regeneration/Waystone
  progress. HUD shows duration and magnitude; inspection and logs explain C.
- Grave Curse, Frailty and Weakness remain unimplemented: Grave Curse would
  duplicate current combat regeneration restrictions; the other effects can
  follow if playtesting shows a useful role rather than extra status clutter.
- Save format 17 reads 9-16. Older Liches receive Hex on a 10-turn grace cooldown;
  existing HP/maxHP and committed attack timers/areas remain preserved. New
  Warlord cleaves, curses and Hex cooldowns persist in active and cached floors.
- Balance and save migration have not been runtime-tested. Player acceptance,
  additional telegraph shapes and the broader Prompts 32-38 check remain before
  treating Tier 1 as complete.

Boss/curse build status: Debug game target compiled successfully. No automated tests or live playtest were run.


## Automated playtest checkpoint (2026-09-27)

All 22 test executables passed after strengthening save assertions and updating
stale expectations. Expanded coverage includes bosses/curses, summons, rewards,
auto-explore, cached-floor travel, 500 encounter layouts and 60 generated saves.
Controls overlap and the outdated Cleanse log were fixed. See PLAYTEST_PASS.md
for exact scope and manual follow-up; combat balance is not yet player-approved.


## Tier 2 first mouse/inspection pass (2026-09-27)

User requested proceeding while their combat playtest is pending.
- Visible-ground left-click takes one dominant-axis cardinal step, not a route.
  Wall/self/hidden clicks spend nothing. Ground clicks blocked by an enemy do
  not silently bump-attack; adjacent enemy clicks explicitly use Basic Attack.
- Selected talent aiming and explicit I/Tab inspection take precedence over
  movement clicks. Keyboard movement and existing talent bindings remain.
- Talent rows have hover/selection button backgrounds and mouse page arrows.
- Enemy inspection retains the last hovered target while entering its panel,
  wraps and scrolls with the mouse wheel, and displays a line-range indicator.
  All details still pass live visibility/death checks, including allied minions.
- This is a first UI slice. Bottom icon hotbar, status tooltips, mouse support
  for the other menus, 50-slot bag, 11-slot equipment and paper doll remain.
- New interactions need live acceptance; the earlier 22-test result predates
  this UI slice and must not be treated as verification of these new controls.

First mouse/inspection UI slice: Debug game build passed. No new automated or live tests were run for this slice.


## Tier 2 bottom hotbar/status pass (2026-09-27)

- Replaced the sidebar talent list with nine bottom hotbar buttons per page:
  keyboard labels, native shape symbols, short names, ranks, availability colour,
  cooldown/cost labels and clickable page arrows. Hover shows full details in
  the sidebar. Existing saved hotbar order is reused without migration.
- Active statuses now have a paged row below the map, with duration counters
  and hover explanations including curse magnitude and Cleanse guidance.
  Deathless readiness remains visible beside the hotbar. Internal UnseenReady
  is hidden and Battle Rhythm is labelled Ready rather than a long timer.
- The log uses two rows above the hotbar; wheel scrolling exposes older retained
  messages and hovering exposes the complete message in the details panel.
- Existing map size and camera remain. Sidebar now gives talent/status detail
  more height while retaining the scrolling enemy inspection panel.
- Remaining: inventory/equipment UI, 50-slot bag, 11 equipment slots and armour
  majority rule, broader mouse-menu support, more distinctive artwork if useful.
- Automated tests were not run for this UI pass. Earlier sidebar-coordinate
  tests need updating for bottom slots when the next verification pass is requested.

Bottom hotbar/status pass: Debug game build passed; live UI acceptance remains pending.
