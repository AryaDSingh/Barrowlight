# Prompt 35 proposal — pending user agreement

This is a reviewable proposal, not implemented gameplay. ROADMAP.md explicitly
requires agreeing the replacement specialization rules before implementing them.

## Recommended rules

- Keep all starting kits and native unlocks, including Mage Blink at level 2.
- Replace the current free hybrid picks at levels 5-10 with exactly three
  permanent specialization choices, at levels **5, 7 and 9**.
- Each choice grants either one native mastery or one borrowed talent.
- Warrior, Mage and Thief may choose either other class as their secondary.
  The first borrowed talent locks the secondary class for that run.
- Maximum two borrowed talents; no duplicates, third class, or respec.
- A pure character can spend all three choices on its three native masteries.
  A hybrid must spend at least one choice on a native mastery.
- A borrowed talent keeps its original scaling attribute, costs and targeting.
  Eligibility respects its source class's unlock level. Masteries only modify
  native talents, not borrowed copies. Each mastery can be chosen once.
- Show the exact resulting talent and compatible runes before committing.
  Choices are free level-up rewards; equipment/rune changes retain their turn costs.

## Initial mastery values for review

These are proposed starting numbers, not balance-tested values. Effects apply
to the named skill only, and preserve its original damage-scaling cooldown tier.

| Class | Mastery | Proposed change | Cost / concern |
|---|---|---|---|
| Warrior | Driving Cleave | Cleave pushes each surviving, successfully hit enemy one tile away | No wall collision damage; blocked/occupied destinations stop movement. Resolving multiple pushes needs deterministic order. |
| Warrior | Cleansing Cry | Rallying Cry removes Poison when cast | Makes a native defensive choice useful; coordinate with Prompt 36's broader cleanse rules. |
| Warrior | Measured Fury | Berserker's Fury costs 4 HP instead of 8; cooldown 6 instead of 4 | Safer attack in exchange for lower frequency. |
| Mage | Lingering Shatter | Mind Shatter's successful stun lasts 2 enemy turns; cooldown 7 instead of 5 | Retain 60% on-hit chance; check control uptime before accepting these numbers. |
| Mage | Deliberate Storm | Arcane Storm costs 5 mana instead of 7; cooldown 5 instead of 4 | Improves resource endurance at the cost of frequency. |
| Mage | Far Blink | Blink moves 4 tiles instead of 3; costs 6 mana instead of 4 | Swift Passage then reaches 6 tiles; terrain, visibility and occupancy rules still apply. |
| Thief | Extended Vault | Vault Kick retreats up to 5 tiles instead of 3; cooldown 5 instead of 4 | Extra escape distance may be blocked by terrain; this remains an attack, not pure movement. |
| Thief | Spreading Volley | Volley radius increases by 1; mana cost increases by 2 | Widen can add another radius; review the combined footprint and cost. |
| Thief | Certain Shot | Piercing Shot always crits on a successful hit, using normal 1.5x crit damage instead of its 2x special multiplier; cooldown 9 instead of 7 | Dodges still work. Trading peak burst and frequency for reliable crits may be too weak with high Dexterity. |

## Why this direction

Three choices create an opportunity cost for borrowing skills and allow Thief
to participate. The two-borrow limit prevents a hybrid from simply collecting
the other class's complete kit. The tradeoff is less frequent skill acquisition
after level 5, and a larger decision screen. Native masteries need to be useful
enough that the third native choice is competitive with a second borrowed skill.

## Implementation order after agreement

1. Add stable mastery definitions/IDs and specialization state: chosen secondary,
   selected mastery IDs, borrowed talent IDs, and pending reward levels.
2. Queue level-5/7/9 rewards exactly once across multi-level XP grants. Resolve
   native talent unlocks and attribute allocation in a documented order, and
   defer final victory until every pending reward is resolved.
3. Replace the current hybrid screen with native mastery / secondary class /
   borrowed skill selection. Support all options with paging and keyboard access.
4. Resolve effective talents in order: base definition -> native mastery -> rune.
   Preserve original cooldown scaling, existing remaining cooldowns, and rune
   instance ownership. Extend targeting/application together for Cleave push.
5. Save specialization state, pending rewards and any pending victory by stable
   IDs. Bump the save version; follow the existing reject-old-saves policy.
6. Update documentation and existing fixtures. Build; record runtime verification
   separately. Important cases: all-native runs, both possible secondaries for
   each class, two-borrow cap, duplicates, level jumps, final-boss level-up,
   rune/mastery interactions and saving during a pending choice.

Prompt 36 is next only after this feature and its verification are complete.
