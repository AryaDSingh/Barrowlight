# Resonance implementation and player mastery

Research and repository review, 4 October 2026. This is a proposed implementation plan; gameplay changes have not been implemented or playtested as part of this review. Numbers below are prototype settings, not established balance.

**The design commitment**

An experienced player should survive longer with the same starting character because they can read danger, prepare a fight, use terrain, conserve resources and recognise when to leave. Talents give that knowledge more applications. Character growth remains satisfying, but success must continue to depend on decisions after a build comes together.

Knowledge has three layers: understanding a rule, recognising when it matters, and managing its consequences across a run. Secret combinations help the first layer. Varied encounters and limited resources sustain the other two after the secrets become familiar.

**What the research supports**

These are developer accounts and official system descriptions, rather than experimental proof that a particular design will work here. The recommendations following this table are adaptations to this game.

| Source | Relevant finding | Application here |
|---|---|---|
| [Cogmind: Designing for Mastery](https://www.gridsagegames.com/blog/2025/08/designing-for-mastery-in-roguelikes-w-roguelike-radio/) | The developer connects mastery with adaptation, player agency, feedback and consequences that extend beyond one fight. | Support scouting, recovery, escape and environmental manipulation as well as damage builds. |
| [Dungeon Crawl Stone Soup design philosophy](https://github.com/crawl/crawl/blob/master/crawl-ref/docs/crawl_manual.rst#n-philosophy-pas-de-faq) | DCSS prioritises meaningful decisions, player skill, clarity and avoiding profitable tedium. | Explain immediate mechanics; preserve uncertainty in situations. Avoid unlock tasks that reward repetitive safe actions. |
| [Into the Breach design postmortem, Matthew Davis](https://media.gdcvault.com/gdc2019/presentations/Into%20the%20Breach%20Postmortem%20Final.pdf) | The slides describe how telegraphed attacks and deterministic player turns shaped combat and readable interfaces. They also emphasise manipulating enemies and designing under constraints. | Make pushes, interruption and terrain consequences predictable. Preserve this game's darkness and exploration uncertainty. |
| [Grim Dawn: Devotion](https://www.grimdawn.com/guide/character/devotion/) | Affinity is a requirement rather than a currency spent on unlocks. Refunding points must preserve dependent requirements. | Recompute affinity from the final build and validate refunds as a whole. |
| [Last Epoch: Skill Specialization](https://support.lastepoch.com/hc/en-us/articles/46363203944859-Skill-Specialization) | Specialisation is limited to five skills; changing specialisation has minimum levels and experience catch-up. | Depth benefits from a limited set of simultaneous commitments. Do not copy its experience re-earning cost automatically into a finite roguelike run. |
| [Cogmind: Tutorials and Help](https://www.gridsagegames.com/blog/2016/07/tutorials-help/) | Contextual help and a deliberately ordered introductory space reduce simultaneous demands on a new player. | Teach one interaction in a readable situation, then combine it with another pressure. |
| [Cogmind: Morgue Files](https://www.gridsagegames.com/blog/2015/10/morgue-files/) | Action records and post-run information support reviewing what happened. | Provide a short factual sequence leading to death and an optional longer history. |

**Current code changes the starting point**

- There are 37 tree definitions. Many existing talents already receive a mastery at rank 5. The common damage profile is 100/120/145/170/200 percent, with exceptions. Compressing ranks requires retuning the power curve, not just changing a constant. See [TalentCatalog.hpp](<C:/school/Personal Project/roguelike/src/entities/TalentCatalog.hpp:65>).
- Hybrid requirements are already visible in the talent screen; Blood Magic retains its altar discovery. The proposal's description of universally hidden hybrid prerequisites is out of date. See [ApplicationTalents.cpp](<C:/school/Personal Project/roguelike/src/core/ApplicationTalents.cpp:47>).
- Spellblade already has Battle Rhythm: casting improves the next melee attack, and a landed melee attack reduces one running spell cooldown. Use that as a comparison for any new Spellsword design. See [TalentCatalog.hpp](<C:/school/Personal Project/roguelike/src/entities/TalentCatalog.hpp:373>).
- Preview and execution already share targeting geometry, and damage estimation is shared with damage application. Extend these paths so new effects remain explainable. See [TalentTargeting.cpp](<C:/school/Personal Project/roguelike/src/world/TalentTargeting.cpp:66>).
- Surface reactions and their timing already exist, including burning oil, freezing conductive ground and electrifying pools. Build on [ApplicationSurfaces.cpp](<C:/school/Personal Project/roguelike/src/core/ApplicationSurfaces.cpp:81>).
- The current death screen gives a generic death message without a causal recap. See [Application.cpp](<C:/school/Personal Project/roguelike/src/core/Application.cpp:2868>).
- UI code indexes trees as four consecutive abilities. Save validation assumes the current tier and rank rules. Both must change before branching trees are safe. See [ApplicationTalents.cpp](<C:/school/Personal Project/roguelike/src/core/ApplicationTalents.cpp:322>) and [SaveGame.cpp](<C:/school/Personal Project/roguelike/src/core/SaveGame.cpp:123>).

Current earned budgets, calculated from [RunProgression.hpp](<C:/school/Personal Project/roguelike/src/entities/RunProgression.hpp:14>):

| Level | Ability points | Tree points |
|---|---:|---:|
| 1 | 4 | 1 |
| 3 | 7 | 1 |
| 5 | 10 | 2 |
| 6 | 12 | 2 |
| 10 | 18 | 4 |
| 20 | 33 | 9 |

The second tree point arrives at level 5 and competes with specialising the first tree. A 4+4 resonance costing one point needs nine ability points; a 5+5 one needs eleven. Under the current tree gates, these can theoretically become available at levels 5 and 6. That small timing difference does not justify making the latter categorically strongest. Actual floor timing needs to be measured.

**Rules to prototype**

1. Keep two forks per pilot tree. The first chooses a combat approach; the second chooses a substantial tool or modifier. Let both first branches reach either capstone where the fiction and mechanics make sense. The automatic branch passive supports the first choice; it is not a third independent choice.
2. Use three ranks for pilot actives and one for passives. The useful behaviour exists at rank 1. Rank 2 improves a practical breakpoint, such as cost, duration or reach. Rank 3 refines the role. A damage increase can be worthwhile; every rank does not need a new subsystem. Avoid upgrades that unexpectedly remove targeting control.
3. Treat ten points as the maximum cost of an all-active-capstone path. A one-rank passive capstone produces an eight-point path under the proposal's template. Decide deliberately whether this difference is acceptable. A tree with two branch passives contains seven authored nodes, so the existing four-node format needs three additional slots per tree before reuse or cuts.
4. Each purchased ability rank contributes one affinity pip. A node declares its affinity; branches can give a mixed tree different colours. Free basic skills, derived imbue variants, resonance purchases and copied effects grant none. This permits a mixed tree to develop both colours without doubling the value of every point.
5. Use 4+4 plus one ability point as the common first resonance threshold for the initial comparison. Defer the family-based price hierarchy until there is evidence that it improves decisions. Keep Paradox as an identity for unusual interactions, with meaningful constraints.
6. Prototype a limit of two active resonances, changed at a sanctuary. This creates an explicit choice when broad investment qualifies for many pairs. Affinity is not consumed. Test the limit before treating it as final; two is a starting constraint, not a researched optimum.
7. Keep the sixteen affinities as provisional design vocabulary. Present relevant ones in the prototype. Add or merge an affinity when its mechanics justify the change, rather than filling a symmetrical table. Gear affinity bonuses and new base trees come later.

**What the first content should do**

Build Fire, One-Handed and Arcane first, in a short test route with fixed-budget loadouts. Compare a Fire specialist, a weapon specialist, a Fire/weapon build and an Arcane/weapon build. Include a broad-investment loadout to look for efficient dipping. Preserve total spending and comparable gear during comparisons.

Fire's first fork can be immediate Fireball versus a stationary Flame Wall. Keep both useful on purchase. Fireball suits an immediate cluster; the wall suits a route enemies must cross. The wall should remain placed by the player when upgraded. Automatically making it follow the caster changes the tool's purpose and could destroy a carefully prepared barrier.

Prefer a second fork between concentrating/consuming established fire and extending/controlling its coverage. These build on knowledge already learned. Defer Phoenix's death rescue from this comparison: it introduces a separate survival economy and makes the fork harder to judge.

Prototype only two resonances initially:

- **Searing Edge, Steel + Flame:** a designated melee finisher consumes an existing Burn to ignite the tile directly behind its target, when that tile is valid. The trade is lost remaining Burn damage for control of space. Preview the destination, expiry and any friendly danger. This is a candidate to test, not a final effect. Reject it if ordinary rooms rarely give it a useful application.
- **Spellsword, Steel + Arcane:** test a resonance version of the existing Battle Rhythm against its current hybrid-tree access cost. Use one fixed passive rank and an explicit once-per-action limit. Make the next-strike state visible. Avoid also granting the identical passive from the old tree in the pilot. Only replace it with a more complicated echo if players find the existing alternation uninteresting.

These contrast an environmental effect with a sequencing effect. Radiance/Shadow is the next experiment because it tests the user's other central fantasy and stresses visibility rules. It should follow a working pilot, not wait for all 37 trees to be converted.

**Learning through the dungeon**

Create a small set of encounter templates with several layouts each. The test route should include:

| Situation | What a beginner can observe | What experience adds |
|---|---|---|
| A melee enemy, an oil patch and a movable burning prop | Oil catches fire; fire persists and can hurt the player | Arrange the fight before committing, then preserve an escape route |
| A ranged enemy overlooking a corridor | Cover breaks an attack line | Use a forked approach, draw another enemy into the line, or disengage |
| A warned heavy attack beside hazardous ground | The marked area is dangerous | Decide whether to move, interrupt, push the attacker or spend an escape ability |
| A lit room connected to darkness | Light changes what can be seen | Decide whether concealing yourself also gives up information you need |
| A damaged character facing optional treasure | Resources have value beyond this fight | Judge whether the reward justifies the next risk |

Use existing room modules, props and encounter planning. Hand-author the relationship between danger, tools and possible exits; vary layout and enemy combinations. Offer multiple viable responses. Avoid ensuring that the player's chosen trick always works, or introducing an enemy that silently invalidates it.

Do not make every warning harder by shortening its response window. Combine pressures that ask for a choice: leaving a cleave may expose the player to an archer; extinguishing fire may remove useful illumination. Preserve readable escape possibilities. An early lesson need not kill a player to be memorable.

**Information and feel**

Show immediate ability costs, affected visible tiles, movement endpoints, known status interactions and conditional effects. Distinguish guaranteed effects from hit-dependent ones. Leave unexplored rooms, unrevealed discoveries and strategic solutions for the player to investigate. An explanation of the rule does not identify the best move.

Resolve the presentation in a readable order: committed action, impact, resulting environmental reaction. Use a short distinctive cue and a persistent state icon when a resonance becomes ready. Keep enemy warnings visible through effects. Avoid one message and animation for every damage tick. Browsing, cancelling and inspecting should remain free.

Record a bounded history of roughly eight committed player actions plus intervening enemy actions and hazards. The death view should show the actual damage sequence, relevant statuses, visible warning and remaining resources. Report observations rather than inventing advice or claiming one action would certainly have saved the run. Keep a longer optional combat history for review.

Discovery has separate states: unknown, hinted and understood. Meeting the affinity requirement reveals the precise purchasable effect, even if a world hint was missed. World events can reveal it earlier. An account-level notebook remembers observed rules and recipes, while every new character still earns its own access. Avoid mandatory repetition such as burning fifty enemies to prove mastery. Essential build functionality should have reliable access; rare discoveries can offer alternative approaches.

**Implementation structure**

Use small C++ components and the existing static catalogue before considering an external content format.

| Component | Proposed change | Reason |
|---|---|---|
| Talent definitions | Add per-node rank count, prerequisites, exclusive choice group and affinity contribution. Give each tree an explicit ordered list of node IDs. | Removes the four-nodes/five-ranks assumption and represents real forks. |
| Talent progression | A shared build validator checks prerequisite closure, exclusive choices, equipment-independent ownership rules and earned budgets. | Purchase UI, respec and save loading should agree. |
| Affinity totals | Derive from purchased nodes; expose a pure query for current and proposed builds. | Refunds and loading cannot leave stale totals or recursive affinity bonuses. |
| Resonance definitions/state | Keep definition, knowledge, purchase, activation and temporary combat state distinct. | Discovering a secret and being able to use it are different facts. |
| Combat resolution | Add a small typed event/result layer around the effects used by the pilot. | Supports causal feedback and bounded resonance hooks without extending every existing conditional. |
| Targeting previews | Share pure predicates and effect calculations with actual resolution. | Prevents a reliable-looking preview from teaching the wrong rule. |
| Save format | Version the new node/state representation and migrate through an explicit conversion. | Changing global rank limits would invalidate older data and assumptions. |

During the pilot, keep legacy rank storage capacity if useful and add per-definition rank counts. Changing `kMaxTalentRank` globally first would affect catalog construction, saved ranks, passive calculations and enemy/legacy definitions simultaneously.

For combat events, record the action ID, source/target handles, ability or resonance ID, cause, relevant positions and actual outcome. Distinguish a manual action, a surface reaction, a status tick and a resonance effect. Queue follow-up effects in a stable order; use stable handles rather than pointers that may outlive a defeated actor. Specify before-hit versus after-hit status checks.

Default rule: a resonance cannot trigger another resonance. Ordinary surface reactions still operate, with explicit limits on chains. Specify whether each hook runs once per action or per target, how misses behave, and whether player-caused damage includes self-costs or allied summons. These distinctions prevent farming, accidental recursion and large-area attacks multiplying a supposedly modest reward.

Do not force existing timing into a new global event framework all at once. Start with the pilot's manual cast, hit and surface-change paths, record their actual order, and preserve the turn contract. Expand only when a new effect needs it.

Respec should be a candidate-build transaction at a sanctuary: show the refund and dependent removals, validate the final build, then commit. Preserve appropriate cooldowns and current HP/mana; changing a build must not refill resources or reset a used death rescue. Start free in development. Test one early correction and a limited sanctuary opportunity before assigning a gold price.

For existing characters, migration needs a controlled allocation screen before the world resumes. Preserve items, world discovery and current combat resources. Reconcile earned point budgets, capstone choices, old rank-5 mastery data, derived variants and hotbar IDs. A free respec alone is not a complete migration specification. Prototype saves can be isolated until this conversion is ready.

**Order of work and gates**

1. Capture the baseline using current Fire, One-Handed, Arcane and Spellblade. Record representative fights, action use and acquisition timing. Build small scenario fixtures and causal action records. Confirm that ordinary environmental play already rewards learning.
2. Remove fixed node/rank assumptions and introduce shared validation. Convert the three pilot trees. Stop and play them before adding resonances: the forks should produce recognisably different tactics on their own.
3. Add derived affinity, the two pilot resonances, feedback and discovery states. Compare equal-budget builds. Test the current level-5 second-tree gate before moving it; if mixed play arrives too late, test moving that grant to level 3 while preserving the end-of-run total and updating class access/save checks together.
4. Test the short route with new players and experienced roguelike players. Fix unclear outcomes and dominant strategies. Then add Radiance/Shadow and one world-taught resonance to challenge the architecture and discovery design.
5. Implement migration and convert further trees in small batches. Keep a hybrid tree when its active kit supplies a distinct playstyle; move a passive-only combination into a resonance when that removes an unnecessary investment detour. Add new base trees only after identifying a missing combat role.

**How to judge whether it feels good**

Use a small initial group spanning beginners, players new to RPGs and roguelike veterans. This is qualitative testing, not a statistically representative study. Ask for predictions before unfamiliar interactions and explanations afterward; avoid coaching them during the attempt.

Compare repeated attempts with the same starting power across different layouts. Then compare specialists, hybrids and broad investors at equal total spending. Rotate scenario order to reduce memorisation. Watch recordings and decisions alongside survival results.

- Learning: can players explain a death or setback accurately and apply that lesson in a different room?
- Tactical identity: do forks change positioning, target order, resource use or retreat decisions?
- Agency: can a player identify a useful response before lethal damage, including when a preferred tactic is unavailable?
- Pacing: how long until the first fork and first resonance are used meaningfully? Do players hoard points because they fear an unexplained commitment?
- Balance: does any build dominate across most scenarios without a corresponding cost? Are broad builds accumulating more useful passives than specialists can compete with?
- Repetition: does optimal play involve safe waiting, repeated preparation or menu swapping before every fight?
- Feel: can players see their prepared interaction pay off without losing track of the next danger?

The core acceptance signal is transfer: with the same starting power, the player makes better decisions in a new layout and can explain those decisions. A higher win rate on one repeated map alone is insufficient.

Meaningful automated checks should cover illegal fork combinations, per-node rank limits, exact point conservation, affinity after refunds, resonance recursion limits, multi-target frequency, visibility-safe previews, resource/cooldown preservation and save round trips. Existing application tests provide a starting point, but some older tests exercise legacy kits.

For reproducible combat comparisons, account for [AttributeFormulas.cpp](<C:/school/Personal Project/roguelike/src/entities/AttributeFormulas.cpp:94>): combat chance currently uses a static RNG seeded from `random_device`. A fixed dungeon seed alone will not replay identical combat. Add a controlled test RNG seam where needed; do not equate a bot run or passing unit tests with verified player enjoyment.

The first complete deliverable should be a short playable route, three revised trees, two resonances, understandable reactions and an informative death review. Its purpose is to establish whether growing knowledge changes how players fight and how far they survive.
