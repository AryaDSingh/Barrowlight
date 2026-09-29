# Prompt 38: themes, rewards and optional vaults

Implemented locally; Debug compilation passed. No tests or playthroughs run.
This records current provisional rules, not a claim of balanced difficulty.

## Regions and ordinary chests

Floors 1-3 are Barracks (warm stone, 3-5 tile rooms), 4-6 Ruined Sanctum
(violet stone, 4-6 tile rooms), and 7-10 Crypts (cold teal, 3-4 tile rooms).
Boss-room sizes remain unchanged. Names appear in the HUD and entry log.
Sanctum composition prioritizes the Ogre/Shaman pair. The Crypt opening pair
contains a natural Skeleton, which has ordinary reward eligibility.

Ordinary chests favor regional bases with double selection weight: martial
weapons, shields and chain/leather armour in Barracks; staff/robes/Focus Charm
in Sanctum; charms in Crypts. Other bases stay possible, keeping all builds
eligible for useful rewards. Normal enemy and boss loot rules are unchanged.

## Vault placement

At most one on ordinary floors 3, 4, 6, 7, 8, 9, only if space permits.
After all normal rooms/corridors/shortcuts, attach a 5x5 chamber through one
sealed wall tile to an ordinary room other than the start or final room.
The new chamber plus its complete perimeter must occupy untouched walls.
This preserves existing mandatory routes and prevents a vault connection from
bridging two rooms. The gate remains solid and opaque until explicitly opened.
No placement means no vault, no reserved enemies or reward generation.

## Player interaction

- A visible purple V marks the sealed entrance. Stand orthogonally adjacent
  and press G to read the warning for free.
- Warning names the Elite Goblin and Archer, elite stat multipliers, reward,
  one-turn opening cost and ability to retreat. Enter opens; Esc leaves sealed.
- Opening the gate spends a turn. Enemies may respond immediately.
- Defeat both named vault guards, wherever they have moved. A message announces
  completion. An X marks the cache while guarded; ! marks it when cleared.
- At or adjacent to the cache, G opens three reward cards. Up/Down selects;
  Enter claims one for one turn. Esc lets the player decide later.
- Cards show item totals and differences from the currently equipped slot.
  The selected item goes into the bag; other offers are discarded permanently.
- F5/F9 work inside the warning and reward menus. Menu browsing consumes no time.
- Leaving the floor abandons any unclaimed vault reward, like other floor loot.

## Encounter budget and rewards

The Elite Goblin (4 budget) and Archer (3) reserve seven points from the regular
floor budget. One elite slot and one Archer slot are reserved as well. Ordinary
encounters then spend the remainder using existing caps. This concentrates
risk into a voluntary fight rather than adding unbounded population strength.
The guards also use ordinary XP/drop rules; vault rewards do not require drops.

Three Rare items with quality +1 and regional base weighting are generated
when the floor is created. A bounded retry prefers distinct bases; absolute
uniqueness is not promised. Only one is claimable. Existing loot RNG is used;
previewing or reopening the reward menu never generates new offers.

## Saves

Format 12 adds vault center/gate, opened/claimed flags, monster membership and
three offered items. Completion derives from surviving tagged guards. The
selected reward persists as an ordinary inventory item; discarded offers vanish.
Item identities and affixes are stored, not rerolled on load. Item save locations
-3/-4/-5 represent the three offered slots; validation checks uniqueness and
completeness. Formats 9-11 migrate without a vault on the current map. Future
eligible floors use current generation. Previous intent persistence is retained.

## Pending play/verification work

- Sample seeds: placement frequency, mandatory reachability, sealed-vault
  isolation, no start/final-room connections and population budget/caps.
- All classes: opening danger, retreat, ranged corner fighting and the value
  of skipping. Reserving seven points may make the ordinary route too gentle.
- Save/load sealed, partly defeated, cleared, reward-menu-open and claimed.
- Claim exactly once, no duplicated offers, old-save migration and floor exit.
- Visual readability of markers and reward comparison cards.

## Vision review

The updated GAME_DESIGN.md adds lore hints, a Codex and knowledge surviving
death. This direction is accepted for future work; none is implemented here.
Suggested refinement for playtesting: accumulated clues should eventually
be actionable before committing scarce build points. Start with thematic
sources already in the roster rather than requiring new rogue enemies first.

Next roadmap feature: Prompt 39, existing boss telegraphs and recovery windows.
