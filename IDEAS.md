# Ideas backlog: unusual mechanics

Collected 2026-10-02 from research into roguelikes and turn-based games
with unusual mechanics. None of this is built yet; these are candidates to
revisit. Each idea notes where it comes from and how it would fit the
systems already in the game (darkness, surfaces, landmarks, trials,
ascendancies, persistent floors).

Recommended first: **1 (vampire curse)**, **4 (pushing into hazards)**,
**2 (Dread meter, possibly as the shrine rework)**, then **3/5 (bones and
nemesis)** for longer-term stories.

---

## Top picks

### 1. Vampire curse: become the monster
- **Source:** [Golden Krone Hotel](https://www.roguebasin.com/index.php/Golden_Krone_Hotel).
  Each run is played as both human and vampire. Vampires see in the dark
  and drink blood, but sunlight and water wreck them.
- **For us:** a rare event, such as a vampire lord's crypt, curses the
  player with vampirism for a while.
  - **Gains:** darkvision, life drain, and Gloomstalker-style strength in
    the dark.
  - **Costs:** light burns. Your own torch, braziers and wisps turn against
    you. Blood pools heal you; water hurts.
- **Fit:** it flips everything built for light and surfaces. It's mostly one
  status effect plus hooks that already exist (Gloomstalker light checks,
  surface ticks, tileLit).

### 2. Dread meter: madness instead of a hunger clock
- **Source:** [Infra Arcana](https://blog.patientrock.com/descending-into-madness-infra-arcana/).
  A madness meter rises from what you encounter, pushing efficient
  exploration.
- **For us:**
  - **Rises:** standing in pitch dark, seeing horrors (the Lich,
    Gloomstalkers), being Smothered.
  - **At thresholds:** fake monsters in the dark, phantom footsteps, and a
    torch that flickers.
  - **Falls:** light, shrines, the town and the lantern.
- **Fit:** gives darkness a long-term cost, and gives shrines a real job. It
  could be the shrine rework (see the playtest note in NEXT_STEPS.md).

### 3. Bones: the ghost of your last character
- **Source:** NetHack's bones files and the
  [Spelunky ghost](https://steamcommunity.com/app/239350/discussions/0/864975632732132776/).
- **For us:**
  - **On death:** the floor where a character died (in Roguelike mode, or
    on a lost Adventure life) is remembered.
  - **Next run:** that floor can turn up with the old character as a
    hostile revenant, wielding its gear and some of its abilities. Killing
    it recovers the gear.
  - **Spelunky twist:** linger too long on a deep floor and a Lich-wraith
    starts hunting you.

### 4. Telegraph puzzles and pushing into hazards *(built 2026-10-02/03: Shove, collisions, chasms, enemy knockbacks; movement attacks not yet)*
- **Source:** [Into the Breach](https://www.gamedeveloper.com/game-platforms/road-to-the-igf-subset-games-i-into-the-breach-i-)
  (telegraphs turn fights into puzzles) and
  [Hoplite](https://en.wikipedia.org/wiki/Hoplite_(video_game)) (movement is
  the attack).
- **For us:** telegraphs, pushes and enemy friendly fire already exist.
  - **Push into surfaces:** shove a goblin into burning oil or electrified
    water, or knock one into a brazier so it tips onto its allies.
  - **Pits and chasms** in some modules: push a monster in and it falls to
    the floor below, dropping no loot.
  - **Movement attacks:** some talents trigger on the steps you take.
- **Fit:** makes surfaces tactical in every fight, not only set pieces.

### 5. A dungeon that remembers: nemeses and legends
- **Source:** [Dwarf Fortress](https://www.pcgamer.com/dwarf-fortresss-roguelike-adventure-mode-arrives-on-steam-in-april-bringing-with-it-my-favorite-feature-your-adventurer-can-literally-slay-the-dragon-that-slew-your-dwarves-and-your-next-fortress-can-make-a-statue-of-it/)
  legends, and the nemesis idea.
- **For us:**
  - **Promotion:** a monster that kills the player, or that the player
    flees from, is promoted. It gains a name and title ("Grub the
    Torchbreaker"), levels up and reappears deeper.
  - **Monuments:** statues or plaques appear in the town square for slain
    trial guardians and nemeses.
- **Fit:** cheap on top of persistent floors; makes runs feel like stories.

---

## Wilder ones

### 6. Polymorph traps and potions
- **Source:** [NetHack polymorph](https://nethackwiki.com/wiki/Polymorph).
- **For us:** become a random monster for N turns, with its sprite, stats
  and attacks. Be an Ogre that stuns, a Drowned One that heals in water, or
  a Gloomstalker in the dark. Polymorphing enemies is a gamble.
- **Feasibility:** surprisingly doable, since monsters are already data plus
  behaviour.

### 7. Cyclic floors
- **Source:** [Unexplored's cyclic dungeon generation](https://www.gamedeveloper.com/design/unexplored-s-secret-cyclic-dungeon-generation-).
  Floors are built from loops ("a lock and key on the same cycle", "a
  shortcut back to the start").
- **For us:** the 3x3 module grid could add locked gates with levers on the
  far side, and one-way drops that make you circle back.

### 8. Gods with conducts *(built 2026-10-03 as the shrine rework: Seraph, Sleeper Below, Ash Saint, Whisperer)*
- **Source:** Dungeon Crawl Stone Soup's gods, and Caves of Qud's factions.
- **For us:** pick a patron at a shrine (another shrine-rework angle). Each
  god wants something:
  - the Flame God likes burning things and hates water;
  - the Shadow God wants you unlit and Concealed.
  Obey and favour grows into boons; disobey and you're punished.
- **Fit:** ties directly into the light, fire and water systems.

### 9. Tempo bonus
- **Source:** [Crypt of the NecroDancer](https://happymag.tv/best-roguelike-games/)
  (rhythm-based roguelike).
- **For us:** not full rhythm. A momentum meter: acting decisively builds a
  combo that boosts damage; waiting, resting and stalling in menus break it.
  The least gothic of these ideas.

### 10. Grafts: rebuild yourself from parts
- **Source:** [Cogmind](https://www.resetera.com/threads/if-you-enjoy-complex-classic-roguelikes-cogmind-is-the-sci-fi-game-for-you.1308/)
  (build yourself from salvaged parts) and
  [Caves of Qud mutations](https://wiki.cavesofqud.com/wiki/Mutations).
- **For us:** grafts harvested from bosses and champions:
  - the Warlord's horn: a charge attack;
  - the Lich's eye: darkvision;
  - a Drowned One's lungs: breathe in water.
  Limited body slots, and each graft has a drawback.

---

## Other references from the research
- [Golden Krone Hotel](https://store.steampowered.com/app/497800/Golden_Krone_Hotel/):
  dynamic lighting used as a weapon; snuffing torches to hide.
- [Dungeons of Dredmor](https://en.wikipedia.org/wiki/Dungeons_of_Dredmor):
  an approachable interface over deep systems; one skill choice per level.
- [DoomRL / DRL](https://www.roguebasin.com/index.php/DoomRL): ranged combat
  that feels like a shooter while staying turn-based; clean auto-targeting.
- Brogue: no experience points; power comes from items and allies.

---

# New talent trees (brainstorm, 2026-10-02)

Already built: Acrobatics (a Thief starting tree). Spellblade, Animation,
Blood Magic and Shadow Archer exist but are locked until an unlock is designed.

Proposed unlocks: weapon trees open when you wield the weapon; magic schools
come from tomes (rare drops, landmark rewards); hybrid trees open at ~5 ranks
in both parent trees (which could also reopen the four locked trees).

First batch chosen: Brawling, Whip, Shadow + Radiance, Alchemy (all built
2026-10-02), then the hybrid unlock rule (built 2026-10-03). Second batch:
Spear, Daggers, Mace and Crossbow (built 2026-10-03; Pole Vault, Bleed,
Sundered and Pinned among them). Still open: Earth, Tide, Hexes, Venom,
Traps, the Acrobatics expansion and the new hybrids.

## Weapons (each a new WeaponKind + tree)
- **Spear / Polearm** (two-handed, reach 2): Reach Strike past an ally; Brace
  (strike whatever steps adjacent, counters charges); Pole Vault over an enemy
  or chasm; Sweep pushes an arc back. Mastery: Impale pins, and a pinned
  target pushed into a wall stays stuck.
- **Daggers** (dual wield, off-hand dagger): Bleed status that leaves blood
  trails; Backstab from Conceal or behind; Flurry hits twice (affixes proc
  twice); Throw Dagger, picked up again from the floor.
- **Mace / Flail**: Stagger delays an enemy telegraph a turn; Sunder lowers
  armour; Shatter bonus vs Chilled and breaks ice; Toll stuns undead.
- **Whip / Chain** (the inverse of a shove): Lash pulls an enemy 2 tiles,
  through fire or into a chasm; Trip; Disarm; Crack snuffs a torch or brazier
  at range.
- **Crossbow**: reload action; bolts pierce a line; Heavy Bolt pushes 1;
  Pin to a wall.

## Physical
- **Brawling** (gauntlets or bare fists): Tackle (charge then shove), Grapple
  (the held enemy moves with you), Throw (toss it 3 tiles into others),
  Headbutt. Mastery: Domino, pushed enemies push what they hit.
- **Acrobatics expansion** (Hoplite movement attacks): Lunge (stepping toward
  an enemy strikes it), Vault, Wall-kick.
- **Alchemy** (DEX + INT, not magic): thrown flasks of oil, water, frost and
  fire; Acid surface eats armour; Smoke cloud blocks light and sight and
  Conceals.
- **Traps / Sabotage**: snares, bear traps, shoving tripwires, rigged barrels.

## Magic
- **Shadow**: Snuff lights in a radius; Gloom Step between unlit tiles;
  Smother blinds an enemy; strong in the dark, weak in light.
- **Radiance**: home of Conjure Light. Flare blinds and reveals Concealed;
  Sear hurts undead and darkvision monsters; Dawn relights torches in view.
- **Earth**: Raise Pillar (temporary wall to shove into), Quake pushes
  outward, Fissure opens a temporary chasm.
- **Tide**: makes water; Wave line push; Undertow pull. Sets up Lightning/Ice.
- **Hexes**: Linked Pain copies damage; Misfortune; Puppet turns an enemy on
  its allies. Mastery: hexes spread on death.
- **Venom**: Poison status; drifting poison gas that explodes in fire.

## Hybrids (~5 ranks in both parents)
- **Lamplighter** (Radiance + Fire): fight with the torch itself, throw it,
  a lantern as focus.
- **Stormlance** (Spear + Lightning): a thrown spear becomes a lightning rod.
- **Hexblade** (One-Handed + Hexes): strikes lay curses.
- **Saboteur** (Stealth + Alchemy): hidden traps and bombs.
- **Stonefist** (Brawling + Earth): shoves knock up rubble; slam into pillars.
