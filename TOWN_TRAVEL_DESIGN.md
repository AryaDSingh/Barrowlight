# Town, persistent floors, Waystone and rest

Implemented locally. Debug build passed after correcting file-stream helper
signatures. No tests or playthroughs were run. Prompt 40 remains deferred.

## Player controls

| Context | Input | Action |
| --- | --- | --- |
| Dungeon | H | Waystone: return to town after 10 quiet turns |
| Dungeon | R | Fast wait for HP, mana, cooldowns and readiness |
| Dungeon | Space | Wait one turn |
| Blue < entrance stairs | G | Previous floor; on floor 1, return to town |
| Downstairs | Walk onto / G | 1 descend, 2 town, Esc stay |
| Town | D | Resume at the exact location left in the dungeon |
| Town | R | Inn: restore HP/mana/cooldowns for free |
| Town | Tab / arrows / Enter | Switch buy/sell, select, trade |
| Town | B | Equip/unequip using the existing inventory |
| Either | F5 / F9 | Save/load the campaign |

Waystone is provisional naming. It is a universal starting action, not equipment:
no bag/Charm slot, point investment or consumable is required. Readiness is
shown beside the talent panel header. New characters still begin in the dungeon.

## Safety and time

A quiet turn is a completed player action without an offensive player cast,
attack against the player (including a dodge), visible living enemy, pending
enemy intent, or harmful player effect (Poison, Burn, Stun, Chill, Marked).
Eligible turns accumulate to ten; combat resets progress. Menus and floor
arrivals do not contribute. Stunned skips never accelerate readiness.

Waystone checks safety again on activation. Stairs require no current danger,
but do not require the ten-turn Waystone cooldown. Town return via stairs still
uses the Waystone rule. Leaving during a boss wind-up is blocked.

R waits at most 100 ordinary player actions, paced at about one per 60ms. It
stops when Waystone is ready, HP/mana are full and learned cooldowns are ready;
also on combat/danger, a game-mode change, or a key/click. Cancelling input stops
the wait rather than also performing another action. Each wait uses normal
status/AI/cooldown processing and the same Opening buff as Space. Starting on the tenth quiet turn, every completed safe player action restores
5% maximum HP rounded up (minimum 1), capped at maximum HP. Movement and manual
waiting heal too; this is not exclusive to R. Combat/danger suppresses recovery
and resets the ten-turn delay. Healing abilities retain their combat role.
Rest does not auto-resume on load. The inn remains an optional convenience.

R no longer regenerates/heals the dungeon. That debug shortcut was incompatible
with persistent progress and a selling economy.

## Persistent travel

A new floor is generated once. Departed floors retain maps/fog, monsters and
boss state, ground loot, chest state, vault guards/rewards, entrances and exits.
Inactive floors pause. Revisiting imports only world state, keeping the current
character, inventory, gold, progression, item-ID counter and loot RNG. Picking
up or selling an item cannot restore its old location through normal travel.
Travel does not refill HP, mana, cooldowns or tick player effects. If a monster
occupies an arrival stair, the nearest free connected tile is used.

Stepping onto downstairs offers a descent/town/stay menu. Ascending uses the
blue entrance tile. Loot at your feet is picked up before G uses the stairs,
so dropped items on a staircase remain recoverable. Boss progression gates and
final Lich victory are unchanged; this is still one ten-floor dungeon.

## First town economy

A menu hub offers the eleven ordinary item bases, unlimited and without affixes,
for 30 gold each. Stronger affixed gear remains dungeon loot. Selling unequipped
bag items grants `5 + 10 * rarity + 2 * rollTier` gold. Training gear has no sale
value. Buying and reselling is a loss. Equipment changes and shopping consume
no dungeon turns in town. The free inn refills HP/mana and resets cooldowns.
Prices and free recovery are provisional; watch for repetitive hit-and-return
play that is safe but tedious. No quests, consumables, paid services or NPC
simulation have been added.

## Saves and migration

Version 14 stores town state, gold, quiet turns, stair coordinates and up to
nine inactive floor snapshots alongside the active floor. A shared stream
serializer reuses the existing save schema; cached character fields are archival
and are never imported on floor travel. Nested snapshots are depth-limited, and
live-world item identities are checked for duplication across floors.

Versions 9-13 load their current floor. They never stored previous floors, so
those floors cannot be recovered. Their current position becomes the entrance
marker; newly visited floors are preserved normally. Old boss-intent migration
rules from Prompt 39 remain. Exact scheduler energy is still not persisted, so
actor ordering can shift after loading or revisiting a floor.

## Pending checks (not run)

- Visit multiple floors, backtrack for loot, save/reload in town and both floors.
- Pick up/sell/claim rewards, revisit, confirm no duplication or respawning.
- Waystone turn count after attacks, misses, damage-over-time, stealth and stun.
- Fast wait cancellation and stopping on new danger; no post-stop extra action.
- Boss/vault progress across travel, blocked arrival stairs, legacy saves.
- Shopping, equipment compatibility, inn recovery and save/load gold ownership.
- Whether ten quiet turns and free town recovery create enjoyable pacing.
