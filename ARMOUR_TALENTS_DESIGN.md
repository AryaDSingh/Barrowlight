# Armour talents: initial playable pass

13 total trees, 52 talents, 3 ranks each. Starting attributes and class pools are
unchanged. Armour trees become available at level 5, using the existing tree
points earned at 5/7/9. Unlocking does not grant talents automatically: ability
points and normal tier prerequisites apply. The fourth talent requires tree
specialization. All classes can still wear any armour without stat gates.

## Talents

Values separated by slashes are ranks 1/2/3. Damage actives use the existing
attribute formula, scaled to 100/120/145%. Costs and cooldowns stay fixed.

| Tree | Talent | Effect | Mana / cooldown |
|---|---|---|---|
| Cloth / Unarmoured | Gather Mana | Spend one action to restore 6/7/9 mana | 0 / 8 |
| Cloth / Unarmoured | Loose Weave | +8/10/12 percentage points dodge while at least half mana | Passive |
| Cloth / Unarmoured | Spellweave | Magic-tree hits gain +4/5/6 damage against Burn, Chill or Shock, once even with multiple ailments | Passive |
| Cloth / Unarmoured | Repelling Pulse | INT-scaled adjacent area hit; push survivors up to 2 tiles | 6 / 7 |
| Light | Sidestep | Move up to 2/2/3 visible tiles, granting Opening; triggers Footwork and Kindle if learned | 0 / 5 |
| Light | Agile Fit | +8/10/12 percentage points dodge while Opening is active | Passive |
| Light | Moving Aim | +10/12/15 percentage points crit for direct attacks while Opening is active | Passive |
| Light | Parting Strike | DEX-scaled melee hit, Mark, retreat up to 2 tiles even on a miss | 3 / 6 |
| Heavy | Shoulder Check | STR-scaled melee hit, push survivor up to 1 tile | 2 / 4 |
| Heavy | Brace | Opening reduces direct incoming hits by 2/2/3, stacking with Guard | Passive |
| Heavy | Unyielding | 20/25/30% chance to resist an incoming stun | Passive |
| Heavy | Second Wind | Spend one action to recover 10/12/15% max HP, rounded up | 4 / 10 |

Cloth accepts Woven Robes or an empty armour slot. Light accepts Scout Leathers;
Heavy accepts Chain Coat. Each active requires matching armour at cast time.
Passives read current equipment on every use, so changing gear immediately
switches them off. No new persistent buff or stat cache is introduced.

Opening is the existing short window from waiting or successful movement
abilities; ordinary walking does not grant it. Total dodge remains capped at
60%. Brace does not stop damage over time, and Shield Training enhances actual
Guard, not Brace itself. Unyielding does not cleanse existing stuns.

Gather Mana competes with spending a turn attacking or escaping. Heavy recovery
costs a turn and mana. Armour abilities are not magic-tree abilities: doubled
spell costs and Arcane Efficiency still apply only to Fire/Ice/Lightning/Arcane.
Spellweave also uses exactly these four trees. Bonus damage is included in the
shared preview calculation. All damaging actives retain empty-ground targeting.

## Persistence and scope

Uses existing stable talent/tree IDs, ranks and cooldowns in save format 14;
no format layout change. Existing saves remain readable. Older executables do
not know these new talent IDs and cannot load a save that purchases them.
Cooldowns continue normally even while wearing incompatible armour.

No automatic point refunds, new point grants, starting-stat changes, or armour
stat gates. At the current level-10 cap, investing in armour competes with
cross-class access and specialization; this is deliberately a meaningful choice.

## Playtest priorities

- Light's crit stacks with Bow, and its dodge stacks with Acrobatics. Watch for
  it becoming the default pick despite the existing dodge cap.
- Cloth's mana threshold rewards conserving resources but should not discourage
  casting so strongly that waiting becomes the best action every time.
- Heavy's small flat block may be stronger against groups of weak enemies than
  bosses. Check that Second Wind is useful without trivializing combat.
- Try gear changes, all three ranks, save/load, and blocked push/retreat paths.
  This pass received a game build, not automated tests or an interactive playtest.

Prompt 40 remains deferred. Further armour tuning should follow play feedback.
