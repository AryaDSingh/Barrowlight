# Development Priorities

Ranked by importance. Two questions decided the order:

1. **Does it affect every run?**
2. **Do other things depend on it?**

Combat comes first: if fights are not fun, trees and ascendancies do not matter.
UI comes before new content: every new feature needs screens, and building them
in the old UI means building them twice.

Each tier should be finished and playtested before moving on to the next.
Related design detail lives in PLAYTEST_NOTES.md, DIRECTION_REVIEW.md,
GAME_DESIGN.md, HIDDEN_TREES_DESIGN.md and ASCENDANCY_DESIGN.md.

---

## Tier 0: Fix now

Small, and they affect every run.

| Item | Why | Reference |
|---|---|---|
| Hotbar rank-up bug | Buying a rank moved Blink from slot 7 to slot 3 and removed Lightning Bolt; breaks the loadout mid-run | PLAYTEST_NOTES.md #1 |
| Line-of-sight mismatch | The Archer can hit the player around a corner where the player cannot hit back; use one shared line-of-fire rule for everyone | PLAYTEST_NOTES.md #2 |

## Tier 1: Make the core fun

Everything else sits on top of this.

| Item | Why | Reference |
|---|---|---|
| Stronger enemies and bigger encounter budgets | Playtesting says the game is too easy | PLAYTEST_NOTES.md #3 |
| Telegraph rework: contested space, overlapping danger, enemy attacks that also hit other enemies | One step currently dodges every telegraph; letting attacks hit enemies turns pushes into tactics | PLAYTEST_NOTES.md #4, DIRECTION_REVIEW.md 4.5 |
| Warlord and Lich reworks, plus enemy curses | Bosses are the climax of a run; curses add pressure you cannot step away from | PLAYTEST_NOTES.md #5–6, ROADMAP.md Prompt 39 |
| Auto-explore (Z) | Cheap, and it speeds up every playtest after this | PLAYTEST_NOTES.md #7 |
| Verify Prompts 32–38 during playtesting | These systems are built but untested; bugs in them will hide behind the combat changes | NEXT_STEPS.md |

## Tier 2: Make it playable by others

Makes the game approachable to anyone other than the developer.

| Item | Why | Reference |
|---|---|---|
| UI rebuild: mouse-driven, icon hotbar, status icons, tooltips, click-to-move one step, 50-slot list bag | Every future feature depends on it | PLAYTEST_NOTES.md #7 |
| 11 equipment slots (ToME-style), paper-doll equipment screen with drag and drop, majority-of-pieces armour rule | Best done together, since the paper doll's layout depends on the slots | PLAYTEST_NOTES.md #7 |

**Open call:** if feeling the town and deeper dungeons first matters more, Tiers 2
and 3 can swap, at the cost of rebuilding some screens later.

## Tier 3: Build the game's structure

Turns the 10-floor run into the game described in GAME_DESIGN.md.

| Item | Why | Reference |
|---|---|---|
| Raise the level cap and rework the point economy | Hidden trees and ascendancy need the extra points | GAME_DESIGN.md 6.3 |
| Choosable dungeons and depth; dungeon difficulty based on depth | The core risk/reward loop; stops farming easy zones | GAME_DESIGN.md 3, DIRECTION_REVIEW.md 4.2 |
| Death modes: Roguelike (one life) and Adventure (a few lives) | Only matters once characters get long, so it comes with the cap rise | DIRECTION_REVIEW.md 4.1 |
| Finish Prompt 38 vaults | Optional risk rooms fit the depth loop | ROADMAP.md Prompt 38 |
| Menu town: buying, weak selling, quests, undo points spent since the last town visit | Gives runs a home base | GAME_DESIGN.md 4, DIRECTION_REVIEW.md 4.2, 4.4 |
| Codex and lore fragments | Needed before hidden trees can be discovered | GAME_DESIGN.md 7.4 |

## Tier 4: Build variety

The exciting content, which only pays off once the structure exists.

| Item | Why | Reference |
|---|---|---|
| Hidden trees: Spellblade, Blood Magic, Shadow Archer | No new engine work needed | HIDDEN_TREES_DESIGN.md |
| Ascendancy and trials | Late-game identity; needs trials in dungeons | ASCENDANCY_DESIGN.md |
| Animation tree and ally engine | Biggest engine change; also enables Trickster's Decoy | HIDDEN_TREES_DESIGN.md |
| Sigils (reusable emergency abilities, replacing consumables) | Deferred; design sketch exists but open questions remain | See Sigils note below |

## Tier 5: Expand the world

Only once one region is genuinely fun.

- Story premise.
- Second and third dungeon bands (Crypts, the Pit and the Devil Lord).
- More regions and origins.

---

## Sigils note (deferred)

Sigils replace consumables: ToME-style reusable abilities on long cooldowns,
slotted in limited sigil slots. They were chosen over consumables because they
avoid hoarding and farming and act as build choices. Named "Sigils" rather than
"runes" to avoid confusion with the skill rune system removed in Prompt 35.

Initial sketch: Regeneration, Phase Step, Haste, Warding, Purge, Recall; 2 slots
at start with more at milestones; swapped in town; same-type lockout so several
healing sigils cannot be chained.

Open questions before implementation:
1. Is Purge the only way to remove curses, or does Cleanse also handle them?
2. Warding overlaps Elemental Ward and Mana Shield; acceptable if the ascendancy
   versions are stronger?
3. Swapping: town only, or also at safe points such as a floor entrance?

---

## Next job for Astra

Tier 0 (both bugs), then start Tier 1 with stronger enemies and the telegraph
rework. Playtest after each step.


## Implementation status — 2026-09-27

Tier 0 changes are implemented; the game Debug build passed. Player acceptance
remains pending before moving to Tier 1. The reported hotbar incident was not
reproduced: rank upgrades already used setRank, but bare numeric input in the
tree menu could overwrite bindings. Binding now requires B followed by the
slot key, and rank purchases explicitly preserve the complete hotbar.

Player and enemy terrain line-of-fire and open-cell visibility now share a
symmetric rule. Warlord's cornered melee also requires orthogonal adjacency.
See NEXT_STEPS.md for focused acceptance checks. No automated tests or live
playthrough were run. DIRECTION_REVIEW.md and ASCENDANCY_DESIGN.md, referenced
above, were not present in the workspace during this pass.


### Tier 1 first encounter pass (2026-09-27)

Roster variety, larger budgeted packs, Rare/named variants with guaranteed
rewards, wider telegraphs and enemy friendly fire are implemented. See the
latest NEXT_STEPS.md checkpoint for exact rules and remaining work. This is
not completion of Tier 1: combat balance and earlier fixes need playtesting.


### Tier 1 auto-explore pass (2026-09-27)

Z auto-explore is implemented with safety and discovery interruptions. It still
needs player acceptance; boss/curses work and the broader playtest pass remain.


### Tier 1 boss and curse pass (2026-09-27)

Warlord pressure/cleaves, tougher Lich reinforcements, and cleansable Mana Drain
and Doom are implemented. Exact numbers and migration notes are in NEXT_STEPS.md.
The next priority is player feedback on these combat changes before Tier 2 UI.
Additional curse types are deferred; this checkpoint does not claim Tier 1 has
passed playtesting.


### Automated acceptance checkpoint (2026-09-27)

22 test executables pass; see PLAYTEST_PASS.md for scope and remaining human
checks. Tier 1 still needs combat/pacing feedback. Long enemy inspection text
can clip in the current panel and remains a concrete UI-pass issue.


### Tier 2 first UI slice (2026-09-27)

Proceeding at user request while combat playtest feedback is pending. One-step
mouse movement, adjacent enemy click attacks, talent page buttons and scrolling
inspection are implemented. Remaining UI/equipment work is listed in NEXT_STEPS.md.

Tier 2 update: bottom hotbar, status hover details and scrollable combat log implemented. Inventory/equipment is next. Build passed; live acceptance pending.


### Tier 2 focused UI acceptance (2026-09-28)

Inventory/equipment, talent menus, class/attribute choices, town, vault/travel,
Codex, dungeon action bar and restart controls now have their first mouse pass.
The focused automated UI pass passes 163 labelled checks across two executables;
rendered screenshots were reviewed and layout defects corrected. See
UI_ACCEPTANCE_PASS.md. Physical mouse feel, display scaling and combat balance
still need user feedback. Next structural work: selectable dungeon/depth and
depth-based difficulty. Prompt 40 remains deferred.


### Tier 3 dungeon selection slice (2026-09-28)

Two selectable ten-depth destinations now reuse the persistent world. Difficulty
now follows depth, not player level. Save version 20 migrates older enemy HP.
The previous selection implementation passed 205 checks; the depth correction
has not been built or tested. DUNGEON_SELECTION.md documents limits and provisional scaling. Next:
death modes, then town progression. Human balance acceptance remains pending.

### Death-mode slice (2026-09-28)

Roguelike/Adventure selection and town revival are implemented, with mode/lives
saved in version 21. Adventure starts with two extra lives; no milestone grants.
Not built or tested this pass. Next: town progression/quests, with death-mode
balance and revival behavior awaiting playtesting.
