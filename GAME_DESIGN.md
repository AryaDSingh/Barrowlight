# Game Design Document (Working Draft)

Living document. Describes the game we are building toward, not only what
exists today. All numbers are illustrative placeholders; balance will be set
through playtesting. Status tags: **[Built]** implemented in the codebase,
**[Partial]** some pieces exist, **[Planned]** agreed direction, not built,
**[Open]** still being designed.

---

## 1. Vision

A one-life, turn-based roguelike RPG set in a world of layered dungeons. You
build a character by discovering how talent trees, gear and ascendancies
combine, then decide how deep you dare to go. Deeper means stronger rewards and
a real chance of losing the character forever.

### Pillars

1. **Risk vs. reward, always your choice.** The player decides which dungeon
   and how deep to go. Going in underleveled should be tempting, not forbidden.
2. **Builds are discovered, not prescribed.** Starting class is a direction,
   not a cage. Trees, gear and hidden combinations let a run become something
   unexpected.
3. **Readable, tactical combat.** Enemies telegraph danger; the player wins by
   reading intent, positioning and combining effects, not by out-statting.
4. **One life matters.** Permadeath is the stakes. Every system should make
   the question "push on or go back?" interesting.
5. **A world worth returning to.** A town, people and a reason to descend give
   every run context beyond the next floor.

### Inspirations (what we take from each)

| Game | What we borrow |
|---|---|
| Path of Exile | Ascendancy as an identity-defining late choice |
| Tales of Maj'Eyal | Tree-based talents; origin-based starting regions |
| Dark and Darker | Dungeons tiered by level band; clear risk/reward framing |
| Last Epoch / Grim Dawn | Abilities that change behaviour, not just numbers |

---

## 2. Core Loop

```
Town ──► choose dungeon & depth ──► fight, loot, level ──► push deeper or return
  ▲                                                               │
  └──────────── sell, buy, quests, prepare ◄──────────────────────┘
                       (death ends the character)
```

- **Short loop:** clear a floor, read enemy intents, collect loot, spend points.
- **Medium loop:** decide at each floor exit whether to continue or return.
- **Long loop:** grow a character across regions and dungeon tiers toward the
  highest bands and the final threats.

---

## 3. World Structure  [Planned]

### 3.1 Regions and origins

The world contains several regions, each with its own starting town and
dungeon set (ToME-style). A character's origin decides which region they start
in. Players can later travel to other regions when strong enough.

- Origin may be tied to race or background.  [Open]
- Each region has a distinct theme, enemy roster and bosses.

### 3.2 Dungeons tiered by level band

Each region offers dungeons suited to a level range. Entry is never hard-locked;
the band is a recommendation and a difficulty signal.

| Band (placeholder) | Example dungeon | Theme | Final threat |
|---|---|---|---|
| 1–10 | The Ruins | Barracks, sanctum, crypts | Goblin Warlord / Lich |
| 10–20 | The Crypts | Undead, necromancy | Lich (expanded) |
| 20–30 | The Pit | Demonic, fire | Devil Lord |
| Higher | TBD | TBD | TBD |

The current game reuses its 20 persistent floors as two selectable ten-depth
dungeons: The Ruins and Deep Crypts. Town offers dungeon/depth selection, with
difficulty determined by dungeon depth, never character level or first entry. The connected stairs route
remains available. Warlord is at Ruins depth 5; each dungeon ends in a Lich.
See DUNGEON_SELECTION.md for this first slice and balance limits.

### 3.3 Multi-layer dungeons

A dungeon is a stack of floors. Deeper floors are harder and richer. The
player can choose to leave between floors (exit route design [Open]), so every
descent is a decision.

- Floor themes shift within a dungeon.  [Partial: Barracks/Sanctum/Crypts]
- Optional vaults: risky side rooms with better rewards.  [Built, Prompt 38]
- Mid-dungeon and final bosses.  [Built: Warlord, Lich]

### 3.4 Risk vs. reward levers  [Planned]

- Deeper floors and higher-band dungeons drop higher-rarity loot.
- Underleveled entry increases reward (and danger).
- Leaving early is always possible but forfeits deeper rewards.
- Death is permanent; what is carried when you die is lost. The one planned
  exception is knowledge: Codex entries and collected lore persist across
  characters (see 7.4). Any other meta carry-over is still [Open].

---

## 4. Town  [Partial: menu, trading, recovery and rumours built]

The hub between runs. Keep it functional first, atmospheric second.

- **Merchants:** sell loot, buy gear and consumables.
- **NPCs:** give context, rumours and quests.
- **Quest board / quest givers:** dungeon-specific objectives (kill a boss,
  recover an item, reach a depth, clear a vault).
- **Services (later):** identify/upgrade gear, respec options (if ever), stash.  [Open]

First version can be a menu-driven town; a walkable town can come later.

---

## 5. Story  [Open]

Needs a reason to descend. Requirements for whatever premise we choose:

- Explains why dungeons exist and why they get worse the deeper you go.
- Supports multiple regions with their own threats.
- Allows the final bosses of each band (Warlord, Lich, Devil Lord...) to feel
  connected.
- Works with permadeath (new characters can pick up where others fell).

Placeholder premise to react to: a frontier town sits above sealed ancient
places; the seals are failing, and what lies below grows stronger with depth.

---

## 6. Character

### 6.1 Classes  [Built]

| Class | Primary | Identity | Starting trees available |
|---|---|---|---|
| Warrior | STR | Melee, durable | One-Handed, Two-Handed, Shield |
| Thief | DEX | Ranged, evasive, stealth | Bow, Stealth, Acrobatics |
| Mage | INT | Elemental casting | Fire, Ice, Lightning, Arcane |

Classes set starting attributes, HP/Mana and the eligible first tree. After the
first tree, any tree can be unlocked (currently from level 5).

### 6.2 Attributes  [Built]

- **STR:** max HP, scales STR abilities.
- **DEX:** dodge (capped), crit, stealth, scales DEX abilities.
- **INT:** max mana, scales INT abilities.
- Global crit chance for all creatures; abilities scale from one attribute.

### 6.3 Levels and points  [Built at cap 20 / Planned at higher cap]

The game is designed for a much higher level cap (soft target 50–100) even
though current content supports a provisional 20-floor run. Point sources will scale with it.

- Attribute points each level.
- Ability points each level.
- Tree points at intervals.
- Possible extra starting point(s) to ease early builds.  [Open]

Exact rates are a balancing task for when content supports the higher cap.

---

## 7. Talent Trees

### 7.1 Core trees  [Built, Prompt 35]

Ten trees, four talents each (Foundation, Complement, Synergy passive,
Advanced active), three ranks per talent. Specialization unlocks a tree's
Advanced talent.

- Martial: One-Handed, Two-Handed, Shield
- Dexterity: Bow, Stealth, Acrobatics
- Magic: Fire, Ice, Lightning, Arcane

Status combos already exist (Burn/Meteor, Chill/Shatter, Shock/Discharge,
Marked, Cleanse, stun recovery).

### 7.2 Armour trees  [Built: first pass]

Anyone can wear any armour. The armour type you are wearing enables a matching
tree's talents.

| Tree | Armour type | Flavour |
|---|---|---|
| Unarmoured / Cloth | Cloth | Mana, casting efficiency, evasion |
| Light Armour | Leather | Mobility, crit, dodge |
| Heavy Armour | Plate | Guard, HP, stun resistance |

- Talents can include attribute bonuses, passives and actives that only work
  while wearing the matching armour.
- Opens builds like a plate-wearing Mage or a cloth-wearing Warrior as
  deliberate choices rather than penalties.
- Armour itself is not stat-gated.

### 7.3 Hidden combination trees  [Built: four-tree first pass]

Special trees that are invisible until the player meets their condition, then
revealed with a log message and appear in the tree browser.

- Unlocked by specialized-tree combinations and run relics; see HIDDEN_TREES_DESIGN.md.
- A hand-designed set of four: Spellblade, Animation, Blood Magic, Shadow Archer.
- Example: any martial tree specialized + any elemental tree specialized
  reveals a **Spellblade** tree (weapon enchants, on-hit procs).
- Other candidates to brainstorm: Bow + Ice, Stealth + Arcane, Shield + Fire.

Unlocking costs a tree point like any other tree.

### 7.4 Discovering hidden trees  [Built: lore, Codex and conditions]

Hidden trees should feel like discoveries, not guide lookups. Three layers
work together, from subtle to certain:

1. **Lore fragments.** Readable items found in the world that hint at a hidden
   tree without spelling out its exact condition.
   - **Thematic enemy drops.** Enemies tied to a tree's theme can drop related
     lore. Archers and rogue-type enemies drop fragments about Bow/Stealth
     hidden trees; the Lich drops lore about a necromancy/undeath tree.
   - **Bosses give stronger hints** than regular enemies, and may drop the
     rare items that unlock item-based hidden trees directly.
   - **Town rumours and books.** NPCs and readable books in town offer vaguer,
     more atmospheric hints.
   - Lore doubles as world-building for the story (section 5).
2. **The Codex.** An in-game guide book listing hidden trees.
   - Unknown trees appear as "???".
   - Finding related lore adds a vague hint to the entry.
   - Unlocking the tree reveals its full condition and description.
3. **Knowledge survives death.** Codex entries and collected lore persist
   across characters. Once any character unlocks a hidden tree, future
   characters can see how to reach it. This rewards experience without making
   new characters stronger, which suits permadeath.

Example flow: a player kills the Lich and finds a fragment describing "those
who bound death to their will." The Codex now shows a "???" entry with that
hint. A later character meets the condition, unlocks the necromancy tree, and
the entry becomes fully revealed for every character after.

Candidate hidden trees tied to this system (full designs in HIDDEN_TREES_DESIGN.md):

| Hidden tree | Likely lore source | Unlock idea |
|---|---|---|
| Spellblade | Warlord, battlemage books | Martial + elemental specialization |
| Animation (summoning) | Lich, crypt enemies | Rare Lich item + Arcane specialized |
| Blood Magic | Warlord or Shamans | Blood Testament + magic tree specialized |
| Shadow Archer | Archers, rogue enemies | Bow + Stealth specialization |

Rogue-type enemies do not exist yet; they would join the roster for this.

---

## 8. Ascendancy  [Planned]

A late, identity-defining choice layered on top of trees.

- Available ascendancies depend on which trees the character has specialized.
- Each ascendancy offers a small set of options: mostly permanent passives,
  plus at least one signature active ability.
  - Example active: **Elemental Fury** — greatly increased elemental damage for
    a few turns.
- A character gets one ascendancy, possibly two.  [Open]
- Discovered per character/run based on its build, fitting permadeath.

---

## 9. Equipment and Loot

### 9.1 Slots  [Built]

Weapon, Armour, Charm, Off-hand. Martial trees require matching weapons;
Shield requires a shield. Two-handed weapons and bows conflict with shields.

### 9.2 Armour types  [Built: first pass]

Cloth, Light (leathers), Heavy (chain; plate later), each linked to its armour tree (7.2).

### 9.3 Loot  [Built]

Rarity tiers with affixes; chests; boss rewards; persisted loot RNG. Runes
were removed in Prompt 35.

### 9.4 Economy  [Built: provisional gold and trading]

Gold (or equivalent) from dungeons, spent in town. Selling loot is the main
income source.

---

## 10. Enemies and Combat

### 10.1 Combat  [Built]

Turn-based, energy scheduler, FOV, manual targeting with previews, ground
targeting, dodge/crit, Guard, status effects, Cleanse.

### 10.2 Enemy intent  [Built, Prompt 37]

Dangerous attacks are telegraphed with tile overlays and a reaction window
(Bomber Blast, Ogre Stun Slam). Stun or push interrupts. This should be the
standard for all future heavy attacks.

### 10.3 Encounters  [Built, Prompt 37]

Floor budgets, paired enemy groups, elites. Will extend per dungeon/band.

### 10.4 Bosses

| Boss | Where | Status |
|---|---|---|
| Goblin Warlord | Ruins, floor 5 | Built |
| Lich (summons Skeletons) | Ruins, floor 10 | Built |
| Devil Lord | Higher band | Planned |

---

## 11. Deferred / Out of Scope for Now

- Elemental resistances and damage types.
- Meta-progression between characters.  [Open]
- Walkable town (menu town first).
- Higher-cap balance (until content exists to support it).

---

## 12. Suggested Phasing

Numbers of prompts are not fixed; this is order, not schedule.

1. **Stabilise current systems.** Playtest trees, statuses, enemy intent and
   themes. Fix what playtesting finds.
2. **Finish the first dungeon.** Vaults and themed rewards (Prompt 38).
3. **Build the progression spine.** Raise the level cap, scale point sources,
   add armour types and armour trees.
4. **Choosable depth and a second dungeon.** Let the player pick dungeon and
   depth; add The Crypts band. Test whether the risk/reward choice feels good.
5. **Hidden trees and ascendancy.** Start with Spellblade and one or two
   ascendancies.
6. **Town.** Merchants and selling first, then NPCs and quests.
7. **Story, regions and origins.** Premise, a second region, origin choice.
8. **Higher bands.** The Pit and the Devil Lord, then beyond.

---

## 13. Open Questions

1. Exact unlock conditions for each hidden tree (tree combos, items, or both?).
2. One ascendancy or two? Is it chosen once or can it change?
3. How does the player leave a dungeon mid-run?
4. Beyond Codex knowledge, is anything else carried to the next character?
5. How specific should lore hints be before a tree is unlocked?
6. How are origins/regions chosen, and are they tied to race?
7. Story premise.
8. Extra starting points: how many, and what kind?
9. Whether the level cap target is ~50 or ~100.


## Current implementation checkpoint: hidden trees and Deep Crypts

The playable run now extends to floor/level 20. Floor 10's Lich grants a rare
Ossuary Seal and opens the Deep Crypts; a stronger Lich ends floor 20. The new
band reuses existing enemies and encounter mechanics. Dungeon selection,
origins, ascendancies and additional regions remain planned.

Tree points: level 1, then every odd level 5–19. Starting attributes and starting
1 tree / 3 ability points remain unchanged; each level grants 2 attribute and
1 ability point. Four hidden trees are implemented as in HIDDEN_TREES_DESIGN.md,
including its accepted defaults and implementation notes.

Codex: J, arrows, J/Esc to return; J then L consults town lore. Each character
meets unlock conditions independently. Relics belong to the run; revealed
conditions and lore survive death. Armour and the Codex described earlier in
this document are already implemented, even where examples remain aspirational.

### Death-mode implementation (2026-09-28)

Roguelike remains the default one-life mode. Adventure is selectable before class
creation and grants two extra lives. Revival spends one life and returns the
character fully recovered to town, retaining gear and world progress. Deathless
acts first. A fixed life budget is the initial balance choice; milestone grants
remain a possible refinement after playtesting.
