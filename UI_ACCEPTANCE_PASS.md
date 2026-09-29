# UI acceptance pass — 2026-09-28

The focused pass passed **163 labelled checks across two executables**:
29 targeting checks and 134 reward/menu checks. Counts include screenshot-write
checks. The Debug game builds successfully. This updates UI coverage; the older
22-executable combat pass was not rerun in full.

## Coverage

- Updated targeting coordinates for the bottom hotbar; ground/ray targeting,
  free previews, resource consumption, inspection and save restoration.
- Real Application event dispatch: class cards, mandatory starting choices,
  one-point purchases, page-two hotbar assignment, attribute spending and restart.
- Ring drag/drop, filling either ring slot, cancelling incompatible drops,
  full-bag restrictions, replacing equipment, page navigation and menu closing.
- Both rings and expanded armour slots survive save/load. Two heavy pieces tie
  two empty cloth pieces with body deciding; three empty pieces outvote heavy body.
- Town buying/selling, full-bag gold/item-ID preservation, Codex selection and
  town lore consultation, closing overlays, and returning to the saved position.
- Vault selection versus claiming, sealed-vault cancellation, opening, full-bag
  reward preservation and successful claims. Travel respects quiet-turn rules.
- Dungeon Bag/Wait/Rest/Cancel controls; one click during rest only stops it.
- Retained reward suite also samples 500 floor layouts and parses 60 generated
  class/floor saves. Those checks are not complete campaign playthroughs.

## Visual corrections

Inspected actual SFML screenshots of the new menus and HUD. Fixed attribute
labels overlapping their descriptions; equipment names crossing slot borders;
raw Basic Attack/Cleanse IDs and clipped text in the binding picker; the picker
covering only part of an underlying line; narrow talent and vault button labels;
rank-table column headings; and the town footer overlapping its trade button.

## Repeat

Build Debug targets `roguelike`, `application_targeting_test`, and
`application_rewards_test`, then run:

```powershell
C:/Python313/python.exe tests/run_ui_pass.py
```

Results and logs: `build/ui-acceptance/results.json` and each test's `output.log`.
Screenshots: `build/ui-acceptance/application_rewards_test/build/rewards-checks/`
and `build/ui-acceptance/application_targeting_test/build/targeting-checks/`.
The runner isolates working directories, saves and Codex profiles from player data.
Tests use hidden SFML windows and feed events through the game's dispatcher;
they do not drive a physical mouse through the operating system.

## Remaining hands-on checks

- Drag/drop feel, target selection speed, small HUD labels and readability at
  the user's monitor scale. Resized/high-DPI layouts were not exercised.
- Real combat pacing, boss difficulty and the total power of eleven equipment
  slots still need player feedback.
- Not every state/input combination was exercised, including every Imbue
  variant, every long rare-item name, and all save/load failure cases.

Next roadmap work is selectable dungeon/depth progression with difficulty locked
on first entry. Prompt 40 remains deferred.
