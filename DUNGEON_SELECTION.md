# Dungeon selection and depth scaling - 2026-09-28

Town's **Choose dungeon [M]** offers The Ruins and Deep Crypts, each with ten
selectable depths. Click or use arrows, Enter to travel, Esc to cancel.
All depths are available without character-level gates.

## Difficulty

Enemy strength depends on global dungeon depth, never the player's level or
level on first entry. Ruins depths 1-10 are global levels 1-10; Crypts depths
1-10 are global levels 11-20. The chooser shows this fixed depth level.

Existing floor rosters, encounter budgets, tiers, bosses and Deep Crypts bonuses
remain. On top of those base stats, each global depth beyond 1 adds 8% base HP,
1 STR and INT per two depths, and 5% base XP. These bonuses are additive across
depths, not compounded. Enemy summons use the same depth rules; allied minions
still use their talent's rank/INT scaling. Loot quality uses global depth directly.
These initial numbers need campaign playtesting, especially deep boss HP.

## Persistence and travel

Generated floors persist with their enemies, damage, loot, chests and vaults.
Returning to the current floor resumes the exact position; other visited floors
use their entrance (or a nearby free tile). Travel grants no healing or turns.
The connected stairs route remains. Warlord is at Ruins depth 5, and each dungeon
ends in a Lich. No respawns or repeatable dungeon resets were added.

## Saves

Version 20 accepts versions 9-19. Legacy first-entry levels are read only for
migration, then cleared. Old enemy maximum HP is converted to depth scaling,
preserving remaining health proportion (rounded down, minimum 1 HP). This also
applies to cached floors. Existing items, maps and allied minions are retained.
Current saves restore without repeating the conversion.

The earlier selection feature passed 205 checks before this correction.
This correction has not been built or tested.
