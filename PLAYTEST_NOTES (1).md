# Playtest notes

Findings from live playtesting. Each entry has what happened, the likely cause
where known, and what we want instead. Suspected causes are hypotheses from the
design docs and earlier code, not confirmed by reading the current code.

## Session: 2026-09-26

### Bugs

#### 1. Ranking up an ability moved it on the hotbar and removed another ability  [High]

**What happened:** Blink was bound to hotbar slot 7. After buying a rank (at
rank 3), Blink moved to slot 3 and replaced Lightning Bolt, which was no longer
on the hotbar.

**Likely cause:** Prompt 35 says "Learning auto-fills" the hotbar. Buying a
*rank* may be running the same auto-fill logic as *learning* a new ability,
re-binding the ability to a default slot and overwriting whatever was there.

**Expected:**
- Buying a rank never changes hotbar bindings.
- Auto-fill only happens when an ability is learned for the first time, and
  only into an empty slot. It should never overwrite an existing binding.

**To check:** does it happen at rank 2 as well as rank 3? Does it happen for
other abilities, or only Blink? Does it survive save/load?

#### 2. Line-of-sight is not the same for the player and enemies  [High]

**What happened:** Player spells cannot hit enemies around a corner, but the
Archer can hit the player diagonally around the same corner. The player cannot
do the same back.

**Likely cause:** Players and monsters probably use different rules to decide
whether an attack can reach. The Archer's AI (Kiter) checks whether the player
is visible in its field of view, which is generous near corners. Player
targeting appears to trace an actual projectile path, which gets blocked by the
corner. These can disagree.

**Expected:** one shared line-of-fire rule for everyone. If the Archer can hit
you, you can hit the Archer from the same tiles, and vice versa.

**Also check:** melee is "adjacent, no diagonals" for the player. Confirm that
enemies follow the same melee rule.

### Balance

#### 3. Enemies are too weak overall  [High]

Enemies need to be much stronger. Prompt 37 describes the current floor budget
curve as intentionally conservative, so this confirms it should be raised.
Candidate levers: more HP and damage, larger floor budgets, more elites, and
earlier access to dangerous groups (Bomber, Ogre + Shaman).

#### 4. Telegraphed attacks are too easy to avoid  [High]

Bomber and Ogre warnings can be dodged by stepping one tile away, so they rarely
threaten the player. Enemies also need pressure that cannot simply be walked away
from.

Ideas:
- Larger or oddly shaped danger areas, so one step is not always enough.
- Several telegraphs at once that overlap, forcing a choice.
- Telegraphs that push the player toward another danger.
- Pair telegraphs with unavoidable pressure such as curses (see 6).

#### 5. The Lich and its summons need to be stronger  [High]

The Lich fight is too easy. Current values: 110 HP, ranged bolt, up to 3
Skeleton summons (15 HP, 3 damage each).

Ideas:
- More HP and damage for the Lich.
- Stronger Skeletons, or several summoned at once.
- Add curses to the Lich's kit (see 6), so the fight has pressure you cannot
  simply step away from.

### Design ideas from this session

#### 6. Enemy curses (unavoidable pressure)  [New mechanic]

Curses are debuffs enemies apply that do not depend on the player standing in
the wrong place. The Lich is the natural first user.

| Curse | Effect |
|---|---|
| Grave Curse | Stops health regeneration for several turns |
| Frailty | Take more damage for a few turns |
| Weakness | Deal less damage for a few turns |
| Mana Drain | Lose mana each turn |
| Doom | Take a large burst of damage after a few turns unless removed |

Questions to settle:
- Can Cleanse remove curses? It currently removes Poison, Burn, Chill and
  Marked. If curses are cleansable, Cleanse becomes much more important; if not,
  they need short durations.
- Confirm whether the player currently has natural HP regeneration. Grave Curse
  only matters if there is regen to stop.
- Curses would give Juggernaut's Unstoppable (immune to ailments) a clear use.

### UI

#### 7. The UI needs a major rework  [High, own phase]

The UI needs a lot of work. Direction: a fully mouse-clickable interface in the
style of Tales of Maj'Eyal, with an equipment screen like WoW, ToME or Path of
Exile. Keyboard controls should keep working alongside the mouse.

**Inventory (bag):**
- A **list**, not a grid. No inventory-space management.
- **50 spaces** for now, so bag size is not a constraint.
- Hover tooltips with a comparison against equipped gear (the current
  inventory's comparison panel can be reused).

**Equipment screen:**
- A **section for each slot** arranged like a paper doll (as in WoW, ToME, PoE).
- **Drag and drop** from the bag onto a slot to equip, and off a slot to remove.
- Right-click an item in the bag as an equip shortcut.
- **Slots follow ToME:** main hand, off hand, head, body, cloak, hands, belt,
  feet, amulet, and two rings (eleven slots).
- The current Charm slot becomes the Amulet slot.
- ToME's light source, tool and quiver slots are skipped for now: light would be
  a new sight-radius mechanic, tools (digging) and ammo (quivers) do not exist.

**Interface (ToME style):**
- Everything mouse-clickable: talents, inventory, equipment, menus, log,
  character sheet.
- Icon hotbar along the bottom with cooldown overlays, instead of a text list.
- Status icons for buffs, debuffs and curses, with hover tooltips.
- **Click to move one step at a time** (no automatic pathing on click).
- Click an enemy to attack.
- Tooltips everywhere, so the game can be learned without leaving it.

**Auto-explore (Z), as in ToME:**
- Walks toward the nearest unexplored area of the floor.
- Stops when: an enemy comes into view, the player takes damage, an enemy
  telegraph appears, an item, chest or door is seen or stepped on, or nothing is
  left to explore.
- Does not pick up items or open chests automatically.

Scope: most screens are currently keyboard-driven text lists, so this is closer
to a UI rebuild than a polish pass.

**Gameplay impact of new equipment slots (not only UI):**
- New slots mean new item types, loot tables and stats to balance.
- **Armour type is decided by the majority of armour pieces** (for the armour
  trees):
  - Four pieces count: head, body, hands and feet. Cloak, belt, amulet and
    rings do not count.
  - Empty armour slots count as Cloth/Unarmoured (matching how the Cloth tree
    already accepts an empty armour slot).
  - Ties (for example 2 cloth and 2 plate) are broken by the body armour.
  - Examples: 3 plate pieces = Heavy; 2 leather + 2 cloth = whatever the body
    armour is; nothing worn = Cloth.
  - Armour-tree actives and passives check this at cast/use time, so swapping
    one piece can change your armour type immediately.

## Next steps

- Fix bugs 1 and 2 first; both affect every run.
- Then raise enemy strength (3), rework telegraphs (4) and strengthen the Lich (5).
- Curses (6) can be designed alongside the Lich rework.
- Plan the UI rework (7) as its own phase rather than squeezing it between
  gameplay fixes.
