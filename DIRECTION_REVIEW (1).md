# Direction and Balance Review

A step back from individual features. This reviews everything designed and built
so far (GAME_DESIGN.md, PROMPT_35–37, the armour pass, HIDDEN_TREES_DESIGN.md,
ASCENDANCY_DESIGN.md, PLAYTEST_NOTES.md, ROADMAP.md) against what players say
about the games we are drawing from: Tales of Maj'Eyal, Path of Exile 1 and 2,
Last Epoch, Daggerfall, Dungeon Crawl Stone Soup, Brogue, Dark and Darker,
Into the Breach and Diablo 4. Sources are listed at the end.

This is analysis and recommendation, not a spec. Nothing here is decided until
you agree to it.

---

## 1. Summary

The game already has a strong core: turn-based tactics with telegraphed enemy
attacks, a talent-tree system you like, statuses that combo, and a clear vision
of a layered world with real risk. The biggest risks are not missing features.
They are **five tensions** between features we have already agreed on:

1. **Permadeath vs. very long characters.** A level 50–100 character across
   several regions is dozens of hours. Losing that is where roguelikes lose
   players.
2. **Selling loot in town vs. grinding.** Letting players sell everything and
   choose easy dungeons invites safe, boring farming.
3. **Depth vs. complexity.** 13 trees, 4 hidden trees, 7 ascendancies, 11 gear
   slots, statuses and curses is a lot for players to learn.
4. **No respec vs. discovery.** "Builds are discovered" and "no respec with
   permadeath" pull against each other.
5. **Readable combat vs. real danger.** Telegraphs are currently easy to walk
   out of, and enemies are too weak (confirmed in playtesting).

Recommended identity, in one line:

> A tactical roguelike where you read every fight, choose how deep you dare to
> go, and discover what your character becomes.

Top recommendations (details in the sections below):

| # | Recommendation | Why |
|---|---|---|
| 1 | Make combat dangerous before adding more systems | Playtesting says it is too easy; everything else depends on this |
| 2 | Let telegraphed attacks hit enemies too | Turns dodging into a tactical puzzle (Into the Breach) |
| 3 | Add Sigils: reusable emergency abilities (decided instead of consumables) | Permadeath games need panic buttons |
| 4 | Offer two death modes: Roguelike (one life) and Adventure (a few lives) | ToME's answer to losing long characters |
| 5 | Allow undoing recently spent points, in town only | ToME's answer to "no respec" traps |
| 6 | Make selling weak and dungeon depth the main source of wealth | DCSS's anti-grind lesson |
| 7 | Fix a dungeon's difficulty when first entered, within its band | Stops over-leveled farming of easy zones |
| 8 | Fewer, bigger loot drops | Avoids Diablo 4-style loot fatigue across 11 slots |
| 9 | A "complexity budget" and progressive disclosure | "Same depth, less complexity" (PoE2's own goal) |
| 10 | Build one region really well before adding more | Daggerfall's lesson: big is not the same as good |

---

## 2. Where the game stands

**Strengths**
- Deliberate turn-based combat with targeting previews and enemy intent.
- Talent trees with ranks, specialization and cross-class access.
- Status combos (Burn/Meteor, Chill/Shatter, Shock/Discharge, Marked).
- A clear long-term vision: town, layered dungeons, regions, hidden trees,
  ascendancy.
- A disciplined documentation habit, which makes a large project manageable.

**Weaknesses (from playtesting and docs)**
- Enemies too weak; telegraphs dodged by one step; Lich too easy.
- Line-of-sight rules differ between player and enemies.
- Hotbar bug on rank-up.
- UI is keyboard text lists; needs a mouse-driven rebuild.
- Large amounts of recent work (Prompts 32–38) are built but not verified.
- No emergency tools yet. Decided direction: Sigils (see 4.5), deferred.

---

## 3. What we learned from each game

### Tales of Maj'Eyal
- Reviews praise its long-term build strategy but call its moment-to-moment play
  repetitive and bloated. **Our opening:** be as deep in builds *and* better in
  moment-to-moment tactics.
- Common new-player friction: repetitive early dungeons, not knowing where to
  go, and permadeath punishing mistakes. Many players only stick with it after
  switching to **Adventure mode** (multiple lives).
- ToME lets you **unlearn recently learned talents, but only in a quiet place
  like a town.** A small, controlled respec.
- Its **prodigies** (its version of ascendancy) include some gated behind story
  events and stat thresholds, which is close to our lore-gated hidden trees.
- Its developers had to rework prodigies whose uncontrollable downsides players
  hated (for example, a proc that damaged allies and destroyed walls). Lesson:
  powerful effects should be controllable.

### Dungeon Crawl Stone Soup
- Core principles: meaningful decisions (no no-brainers), avoid grinding, clarity
  without spoilers, and a painless interface with auto-explore.
- **Shops do not buy items**, specifically so players do not vacuum up every item
  to sell. Directly relevant to our town.
- Deliberately places dangerous out-of-depth monsters on levels, so players must
  plan escapes rather than fight everything.

### Path of Exile 1 and 2
- Enormous build depth, but frequently described as overwhelming, poorly
  explained and punishing for new players.
- A common complaint: **"if you skill wrong, you are done"**, because respecs are
  expensive.
- PoE2's stated goal was **"same depth, less complexity."**
- Loot arguments go both ways: some players say PoE2 drops too little, while
  Diablo 4 was criticised for drowning players in meaningless loot.

### Last Epoch
- Praised for skill specialization that makes the same skill play differently,
  and for encouraging experimentation. Its respec system is relatively forgiving,
  though some players still felt re-levelling made experimentation costly.
- Mastery choice is permanent, which is close to our one-ascendancy rule.

### Daggerfall
- Loved for freedom, character creation depth and not making you "the chosen
  one." Criticised for enormous maze-like dungeons and a world where procedural
  content makes places feel interchangeable.
- Lesson: **a small, carefully made world beats a huge generated one.** Mix
  hand-authored anchors (bosses, set-pieces, quest dungeons) with generated
  filler.

### Dark and Darker
- The tension comes from **gear fear**: hesitating to risk good gear because death
  loses it.
- Its loop works because some things persist even when you die (a stash, a base
  kit you cannot lose). Without that, there is no reason to take gear out.
- Lesson for us: if death deletes everything, "should I carry this back to town?"
  has little meaning. We should decide what, if anything, survives death.

### Into the Breach and Hoplite
- Telegraphs work because the player **cannot simply step away**: space is
  contested, there are objectives to protect, and **enemies can be pushed into
  each other's attacks.**
- "Seeing what the enemy will do next turn is everything" — but only because the
  board forces hard choices.

### Brogue
- Praised for its interface and a smooth learning curve that reveals depth
  gradually. Allies are a core feature, which supports our Animation tree idea.

### Diablo 4
- Loot fatigue came from constant small upgrades, not from quantity alone.
  Players prefer rare drops that feel like a leap in power and last for hours.

---

## 4. The five tensions, with recommendations

### 4.1 Permadeath vs. very long characters

**Problem:** GAME_DESIGN.md targets a level cap of 50–100 across several regions.
That could easily be 20–40 hours per character. ToME shows that losing a
character that long is where many players quit, and that many players only stay
because Adventure mode exists.

**Options:**
1. **Death modes (recommended).** *Roguelike*: one life, the purist mode.
   *Adventure*: a few extra lives earned at milestones (for example at certain
   levels or after bosses). Both use the same game.
2. **Shorter runs.** Keep the character journey to a few hours. Conflicts with
   the level 50–100 vision.
3. **Persistent meta-progression.** Codex knowledge (already planned) plus
   possibly a shared stash (see 4.2).

**Recommendation:** option 1 plus Codex knowledge. Keep Roguelike mode as the
"real" way to play if you like, but do not force it on everyone.

### 4.2 Selling loot vs. grinding

**Problem:** A town where you can sell every item, combined with choosable
dungeons and a 50-slot bag, invites the grind DCSS deliberately designed out:
clear easy dungeons, vacuum up items, sell, repeat.

**Recommendations:**
- **Selling pays little.** Most gold comes from dungeon rewards (chests, bosses,
  quests, depth bonuses), not from selling random drops.
- **Depth pays more.** Deeper floors and underleveled entry multiply gold, XP and
  loot quality. The reward for risk should dwarf the reward for farming.
- **Dungeon difficulty is fixed when first entered, within its band.** A 1–10
  dungeon entered at level 8 stays level 8 for that character, so returning at
  level 20 gives almost nothing. Nothing is gained by farming it.
- **Decide what survives death.** If nothing does, gold and gear only matter to
  the current character. If a small shared stash survives, you get Dark and
  Darker's gear-fear loop. Recommendation: start with nothing surviving except
  Codex knowledge, and revisit once the town exists.

### 4.3 Depth vs. complexity

**Problem:** planned systems now include 10 core trees, 3 armour trees, 4 hidden
trees, 7 ascendancies with 6 nodes each, 11 gear slots, 8+ statuses, curses,
enemy intents, a Codex, trials, a town and quests. Every reference game that went
this far is described as overwhelming.

**Recommendations:**
- **Complexity budget.** Every new system must either replace something, or pass
  a test: does it create a new *decision* the player did not have before? If it
  only adds a number, cut it.
- **Progressive disclosure.** Introduce systems when they matter. This is already
  happening naturally: armour trees at level 5, hidden trees when discovered,
  ascendancy at the first trial. Keep that pattern for everything new.
- **Explain in the game, not the wiki.** Tooltips everywhere, readable combat
  logs, clear previews. DCSS's "playable without spoilers" is the right bar.
- **Merge overlaps** found in the ascendancy balance pass (Attunement/Overload,
  Footwork Master/Agile Fit, Jack of All Trades/Convergence, Stalwart/Unstoppable).
  See ASCENDANCY_DESIGN.md.

### 4.4 No respec vs. discovery

**Problem:** the design pillar is "builds are discovered." But with no respec,
permadeath and scarce points, a wrong choice can ruin a long character. That is
exactly PoE2's "skill wrong and you're done" complaint.

**Recommendation:** ToME's approach. **Points spent since your last town visit
can be undone, in town only.** Older points are permanent. This:
- lets players experiment in the dungeon and correct mistakes at the next town,
- keeps commitment meaningful (you cannot rebuild your whole character),
- fits the town as a safe place,
- supports hidden trees and ascendancy, which reward late build changes.

Ascendancy itself stays permanent, as agreed.

### 4.5 Readable combat vs. real danger

**Problem:** confirmed in playtesting: enemies are too weak, one step dodges every
telegraph, and the Lich is easy. ToME's reviews suggest its weakness is
moment-to-moment tactics; this is where we can stand out, but only if fights are
genuinely threatening.

**Recommendations:**
1. **Let telegraphed attacks hit enemies.** Right now enemy attacks cannot hurt
   other enemies. Changing that turns Bomber blasts and Ogre slams into tools:
   push an enemy into the blast, stand behind an Ogre's target. This is the core
   of what makes Into the Breach work, and we already have push effects (Shield
   Bash, Whirlwind, Repelling Pulse, Shoulder Check).
2. **Contest the space.** Telegraphs should overlap, block corridors, or force a
   choice between two dangers. One telegraph alone is always escapable.
3. **Unavoidable pressure.** Curses (from PLAYTEST_NOTES.md) and damage over time
   that cannot be stepped out of, balanced by Cleanse and Sigils.
4. **Out-of-depth threats.** Occasionally place a monster the player should not
   fight, as DCSS does. It creates escape decisions, not just fights.
5. **Emergency tools: Sigils (decided).** The game currently has none, and
   permadeath games need ways out of a bad situation. **Decision:** no
   consumables. Instead, **Sigils**: ToME-style reusable abilities on long
   cooldowns, in limited slots. They avoid hoarding and farming and act as build
   choices. Named "Sigils" rather than "runes" to avoid confusion with the skill
   rune system removed in Prompt 35. Implementation is deferred; the sketch and
   open questions are in PRIORITIES.md.
6. **Stronger enemies across the board**, then tune down from there. It is easier
   to find the right difficulty by starting too hard.

---

## 5. Balance review of existing systems

| System | Concern | Suggestion |
|---|---|---|
| Dodge cap 60% | Very high for a turn-based game; Slippery + Agile Fit create a dodge→Opening→dodge loop | Consider a lower cap (~40%) or diminishing returns above 30% |
| Stealth | Several layers (Conceal, Shadow Archer, Shadowcaster) could make the player rarely seen | Test stealth builds against groups, not single enemies |
| Control uptime | Chill, Stun, Earthshaker, Lingering Elements stack toward permanently disabled enemies | Stun recovery helps; watch Chill duration extensions |
| Guard | Flat reduction is strong against many weak hits and weak against bosses; several ascendancy nodes assume Guard exists | Resolve the Guard dependency (see ASCENDANCY_DESIGN.md balance pass) |
| Global 5% crit | Fine, but Flurry-style multi-hit abilities multiply its effect | Consider one crit roll per ability rather than per hit |
| Integer mana costs | Small percentage reductions round away (noted in Prompt 35) | Keep costs larger so reductions matter |
| Encounter budgets | Deliberately conservative; playtesting confirms too easy | Raise budgets and elite frequency first |
| Point economy | 12 ability points at level 10 are tight for 13+ trees | Revisit when the level cap rises |

---

## 6. Loot direction

- **Fewer, bigger drops.** With 11 slots, constant small upgrades will cause
  Diablo 4-style fatigue. Aim for drops that change what you do.
- **Uniques that change behaviour.** Runes were removed, but their idea
  (changing how an ability works) could return as unique item effects. That is
  more exciting than +3% damage.
- **Class-agnostic drops.** Diablo 4 players felt class-tailored drops reduced
  surprise and experimentation. Allow finding a staff as a Warrior; it may send
  your build somewhere new, which is exactly the "discovered builds" pillar.
- **Depth-scaled quality.** Tie rarity to depth and risk, reinforcing 4.2.

---

## 7. World and story direction

- **One region, done well, first.** A town, two or three dungeons with distinct
  identities, a handful of quests and bosses. Expand only after this feels good.
- **Anchors plus generation.** Hand-design bosses, set-piece rooms, vaults and
  quest dungeons; generate the connective floors. Avoids Daggerfall's
  "everything feels the same."
- **Quests change dungeons.** A quest should alter what you do in a dungeon (find
  a specific room, protect someone, retrieve an item under pressure), not only
  "kill X."
- **Not the chosen one.** Daggerfall's players liked being an ordinary person in
  a larger world. That suits permadeath too: many adventurers try, few succeed.
- **Lore as mechanics.** The Codex and lore fragments already make story
  functional. Lean into that instead of long text dumps.

---

## 8. What will make it fun

The moments we should design for:

1. **"I read the fight and outplayed it."** Pushing an Ogre into a Bomber's blast;
   stepping out of two overlapping telegraphs; cleansing a curse at the last turn.
2. **"I discovered something."** A lore fragment that points to a hidden tree; a
   unique item that sends the build somewhere new.
3. **"Do I go deeper?"** Standing at the stairs with a good haul, deciding whether
   to push on.
4. **"That was close."** Surviving with a Sigil, an escape or a lucky dodge,
   and remembering it.
5. **"This character is mine."** Ascendancy and hidden trees making each character
   feel different from the last.

Every new feature should strengthen at least one of these.

---

## 9. Recommended priorities

Superseded by **PRIORITIES.md**, which has the full tiered plan. Summary:

1. **Combat pass.** Stronger enemies, telegraphs that hit enemies, contested
   space, curses, the Lich rework, and the line-of-sight fix.
2. **Verify what exists.** Prompts 32–38 are built but untested. Fix the hotbar
   bug and anything else that surfaces.
3. **UI rebuild.** Mouse-driven, icon hotbar, equipment paper doll, auto-explore.
   Brogue and ToME show how much interface quality matters.
4. **Death modes and town respec.** Roguelike/Adventure modes and "undo points
   since last town visit."
5. **First region slice.** A menu town (buy, weak selling, quests), two dungeons,
   Codex.
6. **Hidden trees, then ascendancy.** Start with the trees that need no new
   engine work (Spellblade, Blood Magic, Shadow Archer), then Animation.
7. **Raise the level cap and expand** once the first region is fun.

---

## 10. Decisions for you

1. Death modes: Roguelike only, or Roguelike plus Adventure (lives)?
2. Town respec: undo points spent since the last town visit?
3. What survives death: only Codex knowledge, or also a small stash?
4. Selling: weak selling, no selling (DCSS), or full selling?
5. Should enemy telegraphed attacks damage other enemies?
6. Should dungeon difficulty lock when first entered?
7. ~~Consumables~~ **Decided:** Sigils (ToME-style reusable abilities) instead of consumables. Implementation deferred.
8. Dodge cap: keep 60% or lower it?

---

## Sources

- Tales of Maj'Eyal user reviews (Metacritic): https://www.metacritic.com/game/tales-of-majeyal/user-reviews/
- Tales of Maj'Eyal Steam review analysis (VaporLens): https://vaporlens.app/app/259680/tales_of_maj_eyal
- ToME forum, permadeath modes discussion: https://forums.te4.org/viewtopic.php?f=39&t=38444
- ToME commit, respec only in town: https://git.net-core.org/tome/t-engine4/-/commit/c2eeba45599d4aea7494fd68f36d65e3ec80a4a5
- ToME 1.6 prodigy rework: https://git.net-core.org/tome/t-engine4/merge_requests/467
- ToME prodigy requirements commit: https://git.net-core.org/nsrr/t-engine4/-/commit/9e969491de9f746c32875a5566b388c6ffcf1730
- DCSS manual (design philosophy): https://github.com/crawl/crawl/blob/master/crawl-ref/docs/crawl_manual.rst
- DCSS overview (Wikipedia): https://en.wikipedia.org/wiki/Dungeon_Crawl_Stone_Soup
- Path of Exile 2 discussions (Steam): https://steamcommunity.com/app/2694490/discussions/0/594008890765554181/?ctp=2 and https://steamcommunity.com/app/2694490/discussions/0/594008672803344953
- PoE2 "same depth, less complexity": https://steamcommunity.com/app/238960/discussions/0/4132683013933474867
- Last Epoch systems overview (Inven Global): https://www.invenglobal.com/articles/26283/what-makes-last-epoch-different-unique-systems-overview
- Last Epoch respec feedback (Steam): https://steamcommunity.com/app/899770/discussions/0/4338725867374359150
- Daggerfall and procedural generation (Pretzel): https://www.pretzel.cc/daggerfall-and-procedural-generation-finding-beauty-in-the-mundane/
- Daggerfall Unity forum on dungeon size: https://forums.dfworkshop.net/viewtopic.php?t=1502
- Dark and Darker terminology (gear fear): https://metabot.gg/en/darkanddarker/guides/terminology-and-slang-glossary
- Into the Breach (PC Gamer via PressReader): https://www.pressreader.com/usa/pc-gamer-us/20190101/282389810553787
- Brogue (Wikipedia): https://en.wikipedia.org/wiki/Brogue_(video_game)
- Diablo 4 loot fatigue (Icy Veins): https://www.icy-veins.com/d4/news/players-are-not-sure-diablo-4s-horadric-cube-will-fix-loot-fatigue/
