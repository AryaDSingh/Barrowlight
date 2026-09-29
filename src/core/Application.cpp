#include "core/Application.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "core/GameRules.hpp"
#include "core/SaveGame.hpp"
#include "core/PlayLayout.hpp"
#include "entities/AttributeFormulas.hpp"
#include "entities/MonsterFactory.hpp"
#include "entities/HybridSpec.hpp"
#include "entities/PlayerClassFactory.hpp"
#include "entities/PlayerLeveling.hpp"
#include "entities/StatusEffectLogic.hpp"
#include "entities/TalentEffects.hpp"
#include "world/DungeonGenerator.hpp"
#include "world/FieldOfView.hpp"

namespace engine {

namespace {
constexpr unsigned int kWindowWidth = playLayout::windowWidth;
constexpr unsigned int kWindowHeight = playLayout::windowHeight;
constexpr char kWindowTitle[] = "Roguelike Engine - Dev Window";
constexpr int kSightRadius = 8;
constexpr unsigned int kInitialSeed = 1337;

// Same launch-directory-relative reasoning as kSaveFilePath (GameRules.hpp).
constexpr const char* kFontPath = "assets/fonts/DejaVuSansMono.ttf";

// The full regular roster, in the order rooms get populated. Fewer than
// 6 non-player, non-boss rooms means a partial roster, not a crash --
// see regenerateLevel.
constexpr std::array<MonsterType, 6> kRoster = {
    MonsterType::Goblin, MonsterType::Spider, MonsterType::Ogre,
    MonsterType::Archer, MonsterType::Shaman, MonsterType::Bomber,
};

} // namespace

Application::Application()
    : window_(sf::VideoMode({kWindowWidth, kWindowHeight}), kWindowTitle),
      // Spellblade is just this constructor's placeholder starting
      // point -- selectClass() (called once the person picks a class on
      // the selection screen) reconfigures player_'s stats and talents
      // for real before any dungeon is generated. See
      // PlayerClassFactory for what each class's Stats/TalentSet
      // actually are.
      player_(Position{0, 0}, statsForClass(PlayerClass::Spellblade),
              talentSetForClass(PlayerClass::Spellblade)) {
    // Auto-flush cout after every insertion (see ARCHITECTURE_DECISIONS.md,
    // Prompt 9) -- fixes stdout buffering for every diagnostic/combat-log
    // line at once.
    std::cout << std::unitbuf;

    // A missing/unreadable font doesn't crash the game -- drawText()
    // skips text entirely without a loaded font (SFML 3 asserts on
    // shaping text with one in debug builds), so the rest of the HUD
    // (bars, tiles, console output) still works. Logged once,
    // plainly, rather than treated as fatal. Raw std::cout here, not
    // log() -- logMessages_ is empty and meaningless before the window
    // and constructor have even finished.
    fontLoaded_ = font_.openFromFile(kFontPath);
    if (!fontLoaded_) {
        std::cout << "Warning: failed to load font at " << kFontPath
                   << " -- on-screen text will not render.\n";
    }

    window_.setFramerateLimit(60);
    window_.setKeyRepeatEnabled(false); // A held confirm key must not cast twice.
    // Deliberately no regenerateLevel() call here -- mode_ starts at
    // ClassSelection (see the member's default), and selectClass()
    // calls regenerateLevel() itself once a real choice is made. Prompt
    // 13's text rendering is what finally makes a real selection screen
    // possible; before that, defaulting straight into the Spellblade
    // (as this constructor did through Prompt 14) was the only option.
}

void Application::run() {
    while (window_.isOpen()) {
        processEvents();
        update();
        render();
    }
}

void Application::logImpl(const std::string& message) {
    std::cout << message << std::endl;
    logMessages_.push_back(message);
    while (logMessages_.size() > kMaxLogMessages) {
        logMessages_.pop_front();
    }
}

void Application::processEvents() {
    while (const std::optional event = window_.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window_.close();
        }

        if (mode_ == GameMode::Playing && !inventoryOpen_ && !runesOpen_) handleTargetingMouse(*event);

        if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            if (keyPressed->code == sf::Keyboard::Key::Escape) {
                if (runesOpen_) { runesOpen_ = false; continue; }
                if (inventoryOpen_) {
                    inventoryOpen_ = false;
                    continue;
                }
                if (mode_ == GameMode::Playing && (aimingTalent_ || inspecting_)) {
                    cancelTargeting();
                    continue;
                }
                window_.close();
                continue;
            }

            if (keyPressed->code == sf::Keyboard::Key::F9) {
                // Reachable from either mode -- a person who wants to
                // continue a previous run shouldn't have to pick a class
                // first just to reach a point where loading is possible.
                // loadGame() determines the actual class from the save
                // file itself and switches mode_ to Playing on success.
                loadGame();
                continue;
            }

            if (mode_ == GameMode::ClassSelection) {
                // Deliberately only these keys handled here -- this
                // screen doesn't need arrow keys, talent keys, or save,
                // so nothing else in the Playing-mode switch below even
                // applies yet. As of Prompt 19: only the 3 base classes
                // are offered here -- Spellblade still exists in
                // PlayerClassFactory (see PlayerClass.hpp), just not as
                // a starting option anymore.
                if (keyPressed->code == sf::Keyboard::Key::Num1) {
                    selectClass(PlayerClass::Warrior);
                } else if (keyPressed->code == sf::Keyboard::Key::Num2) {
                    selectClass(PlayerClass::Mage);
                } else if (keyPressed->code == sf::Keyboard::Key::Num3) {
                    selectClass(PlayerClass::Thief);
                }
                continue;
            }

            if (mode_ == GameMode::GameOver) {
                // selectClass() (reached via the ClassSelection screen
                // this leads back to) does the actual reset -- this
                // mode transition alone doesn't need to touch
                // map_/monsters_/player_ itself.
                if (keyPressed->code == sf::Keyboard::Key::Enter) {
                    mode_ = GameMode::ClassSelection;
                }
                continue;
            }

            if (mode_ == GameMode::AbilityChoice) {
                // Keys 1-N pick that option (N == pendingHybridChoices_
                // .size(), at most 6); 0 declines, only meaningful
                // during the one-time spec-in decision.
                std::optional<std::size_t> pickedIndex;
                static constexpr std::array<sf::Keyboard::Key, 7> kChoiceKeys{
                    sf::Keyboard::Key::Num1, sf::Keyboard::Key::Num2, sf::Keyboard::Key::Num3,
                    sf::Keyboard::Key::Num4, sf::Keyboard::Key::Num5, sf::Keyboard::Key::Num6,
                    sf::Keyboard::Key::Num7,
                };
                for (std::size_t i = 0; i < kChoiceKeys.size(); ++i) {
                    if (keyPressed->code == kChoiceKeys[i] && i < pendingHybridChoices_.size()) {
                        pickedIndex = i;
                        break;
                    }
                }

                if (pickedIndex.has_value()) {
                    const Talent& chosen = pendingHybridChoices_[*pickedIndex];
                    player_.talents().learnTalent(chosen);
                    player_.hybridSpecced() = true;
                    log(pendingHybridChoiceIsSpecIn_ ? "You spec into the hybrid path -- learned "
                                                      : "Learned ",
                        chosen.name, "!");
                    soundManager_.play(SoundEffect::Select);
                    mode_ = GameMode::Playing;
                    resumeLevelUpSequence();
                } else if (pendingHybridChoiceIsSpecIn_ &&
                           keyPressed->code == sf::Keyboard::Key::Num0) {
                    log("You decide to stay on your current path.");
                    soundManager_.play(SoundEffect::Select);
                    mode_ = GameMode::Playing;
                    resumeLevelUpSequence();
                }
                continue;
            }

            if (mode_ == GameMode::AttributeAllocation) {
                bool allocated = false;
                if (keyPressed->code == sf::Keyboard::Key::Num1) {
                    player_.baseStats().strength += 1;
                    player_.baseStats().maxHp += 1;
                    log("+1 Strength.");
                    allocated = true;
                } else if (keyPressed->code == sf::Keyboard::Key::Num2) {
                    player_.baseStats().dexterity += 1;
                    log("+1 Dexterity.");
                    allocated = true;
                } else if (keyPressed->code == sf::Keyboard::Key::Num3) {
                    player_.baseStats().intelligence += 1;
                    player_.baseStats().maxMana += 1;
                    log("+1 Intelligence.");
                    allocated = true;
                }

                if (allocated) {
                    player_.refreshEquipmentStats();
                    player_.unspentAttributePoints() -= 1;
                    soundManager_.play(SoundEffect::Select);
                    // Tentatively resume -- resumeLevelUpSequence() puts
                    // mode_ straight back to AttributeAllocation if
                    // there are still points left from this level-up,
                    // so the same screen naturally reappears for the
                    // next point rather than needing a separate loop
                    // here.
                    mode_ = GameMode::Playing;
                    resumeLevelUpSequence();
                }
                continue;
            }

            if (runesOpen_) { handleRuneKey(keyPressed->code); continue; }
            if (inventoryOpen_) {
                handleInventoryKey(keyPressed->code);
                continue;
            }
            if (keyPressed->code == sf::Keyboard::Key::B) {
                openInventory();
                continue;
            }
            if (keyPressed->code == sf::Keyboard::Key::V) {
                cancelTargeting(); mousePixel_.reset(); runeFeedback_.clear();
                runesOpen_ = true;
                continue;
            }
            if (keyPressed->code == sf::Keyboard::Key::G) {
                pickupItem();
                continue;
            }
            if (handleTargetingKey(keyPressed->code, keyPressed->shift)) continue;

            switch (keyPressed->code) {
                case sf::Keyboard::Key::Up:
                case sf::Keyboard::Key::W:
                    tryMovePlayer(0, -1);
                    break;
                case sf::Keyboard::Key::Down:
                case sf::Keyboard::Key::S:
                    tryMovePlayer(0, 1);
                    break;
                case sf::Keyboard::Key::Left:
                case sf::Keyboard::Key::A:
                    tryMovePlayer(-1, 0);
                    break;
                case sf::Keyboard::Key::Right:
                case sf::Keyboard::Key::D:
                    tryMovePlayer(1, 0);
                    break;
                case sf::Keyboard::Key::R:
                    regenerateLevel(std::random_device{}());
                    break;
                case sf::Keyboard::Key::Num1:
                    requestTalent(talentPage_ * 9);
                    break;
                case sf::Keyboard::Key::Num2:
                    requestTalent(talentPage_ * 9 + 1);
                    break;
                case sf::Keyboard::Key::Num3:
                    requestTalent(talentPage_ * 9 + 2);
                    break;
                case sf::Keyboard::Key::Num4:
                    requestTalent(talentPage_ * 9 + 3);
                    break;
                case sf::Keyboard::Key::Num5:
                    requestTalent(talentPage_ * 9 + 4);
                    break;
                case sf::Keyboard::Key::Num6:
                    requestTalent(talentPage_ * 9 + 5);
                    break;
                case sf::Keyboard::Key::Num7:
                    requestTalent(talentPage_ * 9 + 6);
                    break;
                case sf::Keyboard::Key::Num8:
                    requestTalent(talentPage_ * 9 + 7);
                    break;
                case sf::Keyboard::Key::Num9:
                    requestTalent(talentPage_ * 9 + 8);
                    break;
                case sf::Keyboard::Key::F5:
                    saveGame();
                    break;
                default:
                    break;
            }
        }
    }
}

bool Application::isOccupied(Position pos, const Actor* exclude) {
    return actorAt(pos, exclude) != nullptr;
}

Actor* Application::actorAt(Position pos, const Actor* exclude) {
    // The integration-pass bug fixed in Prompt 11: movement previously
    // only checked map_.isWalkable() (terrain), never whether another
    // actor already stood on the destination tile. Never surfaced in
    // earlier testing because every scripted test walked to a tile
    // *adjacent* to a target, never onto it -- but nothing stopped a
    // real player (or two monsters converging from different angles)
    // from sharing a tile.
    if (&player_ != exclude && player_.position().x == pos.x && player_.position().y == pos.y) {
        return &player_;
    }
    for (auto& m : monsters_) {
        if (m.get() == exclude || m->stats().hp <= 0) {
            continue;
        }
        if (m->position().x == pos.x && m->position().y == pos.y) {
            return m.get();
        }
    }
    return nullptr;
}

bool Application::tryMovePlayer(int dx, int dy) {
    if (!window_.isOpen()) {
        return false;
    }

    const Position current = player_.position();
    const Position target{current.x + dx, current.y + dy};

    if (!map_.isWalkable(target.x, target.y)) {
        return false; // wall or map edge -- a pure input mistake, no turn consumed
    }

    Actor* blocker = actorAt(target, &player_);
    if (blocker != nullptr) {
        // Bumping into another actor doesn't move the player and isn't an
        // attack (no bump-to-attack in this game -- combat only happens
        // through talents), but it's still a genuine action, and treating
        // it as a complete no-op (as it was before this fix) starves
        // monsters of turns: a monster mid-approach needs its own next
        // turn to notice it's now adjacent and attack (its *current*
        // decision was already computed using its pre-move position --
        // see ARCHITECTURE_DECISIONS.md, "Combat pacing"), and if the
        // player's only remaining input is repeatedly bumping the same
        // direction, that turn would otherwise never come. So this still
        // consumes a turn -- everyone else, including the actor just
        // bumped, gets to act -- even though the player doesn't move.
        log("You bump into ", blocker->name(), ".");
        lastMoveDirection_ = Position{dx, dy};
        currentActor_ = &scheduler_.nextTurn();
        processMonsterTurns();
        advanceTurnsUntilPlayerCanAct();
        return true;
    }

    player_.setPosition(target);
    for (const auto& item : groundItems_) {
        if (item->position().x == target.x && item->position().y == target.y)
            log("You see ", item->name(), ". G: pick up (1 turn).");
    }
    lastMoveDirection_ = Position{dx, dy};
    player_.talents().tickCooldowns();
    updateFieldOfView();

    currentActor_ = &scheduler_.nextTurn();
    processMonsterTurns();
    advanceTurnsUntilPlayerCanAct();

    // Stepping onto a door tile advances to the next floor -- checked
    // after the normal move/turn processing above completes, not
    // before, so the move itself still counts as a real turn (monsters
    // still get to act on it) even though regenerateLevel() below
    // immediately replaces them anyway. On a boss floor this tile only
    // becomes reachable once the boss is actually dead -- while alive
    // it occupies (and blocks) this exact position like any other
    // actor, so no separate "is the boss defeated" check is needed
    // here at all.
    if (mode_ == GameMode::Playing && map_.tileAt(target.x, target.y).type == TileType::Door) {
        currentFloor_ += 1;
        log("You step through the door into floor ", currentFloor_, ".");
        regenerateLevel(std::random_device{}()); // fresh layout, same convention as the R key --
                                                   // not tied deterministically to floor number,
                                                   // so replaying the same run still varies
    }

    return true;
}

bool Application::tryUseTalent(std::size_t talentIndex, Position cursor) {
    if (!window_.isOpen()) {
        return false;
    }

    const std::vector<Talent>& talents = player_.talents().knownTalents();
    if (talentIndex >= talents.size()) {
        return false;
    }
    // Own a copy: killing a target can grant a talent and reallocate the kit.
    const Talent talent = player_.talents().effectiveTalent(talentIndex);

    const auto unavailable = talentUnavailableReason(player_, talentIndex);
    if (!unavailable.empty()) {
        log(unavailable);
        return false;
    }

    const TalentTarget target = targetPreview(talentIndex, cursor);
    if (!target.valid) {
        log(target.message);
        return false;
    }
    const auto& affected = target.affected;
    const Actor* chainedTarget = target.chainedTarget;
    const Position blinkDestination = target.destination;

    // Commit point: validation and preview above are side-effect free.
    cancelTargeting();

    player_.stats().mana -= talent.manaCost;
    player_.stats().hp -= talent.hpCost;

    if (talent.shape == EffectShape::Movement) {
        player_.setPosition(blinkDestination);
        log(player_.name(), " uses ", talent.name, ", blinks to (", blinkDestination.x, ',',
            blinkDestination.y, ")");
    } else if (talent.effectKind == TalentEffectKind::Heal) {
        for (Actor* target : affected) {
            applyTalentHeal(talent, player_, *target);
            log(player_.name(), " uses ", talent.name, " (", target->stats().hp, "/",
                target->stats().maxHp, " hp)");
        }
    } else if (talent.effectKind == TalentEffectKind::SelfBuff) {
        for (Actor* target : affected) {
            applyTalentSelfBuff(talent, *target);
        }
        log(player_.name(), " uses ", talent.name, "!");
    } else {
        for (Actor* target : affected) {
            Talent hitTalent = talent;
            if (target == chainedTarget) hitTalent.damagePercent /= 2;
            if (applyTalentDamage(hitTalent, player_, *target)) {
                log(player_.name(), " uses ", talent.name, " on ", target->name(), " (",
                    target->stats().hp, "/", target->stats().maxHp, " hp left)");
                soundManager_.play(SoundEffect::Hit);
                checkAndHandleDeath(*target);
            } else {
                log(target->name(), " dodges ", player_.name(), "'s ", talent.name, "!");
                soundManager_.play(SoundEffect::Dodge);
            }
        }

        // Vault Kick (Archer, Prompt 16): moves the caster away from
        // the target after the damage step above, regardless of
        // whether that damage landed -- the retreat is the caster's own
        // follow-through motion, not an on-hit effect a dodge would
        // block. affected[0] rather than a loop: retreatDistance-bearing
        // talents are always AdjacentEnemy + SingleTarget (exactly one
        // target), never an AoE shape, so there's only ever one
        // position to retreat away from.
        if (talent.retreatDistance > 0 && !affected.empty()) {
            const Position targetPos = affected[0]->position();
            const Position awayDirection{player_.position().x - targetPos.x,
                                          player_.position().y - targetPos.y};
            const Position retreatDestination = target.destination;
            player_.setPosition(retreatDestination);
            lastMoveDirection_ = awayDirection;
            log(player_.name(), " vaults back to (", retreatDestination.x, ',',
                retreatDestination.y, ")");
        }
    }

    player_.talents().startCooldown(talentIndex);
    player_.talents().tickCooldowns();
    updateFieldOfView(); // in case Blink moved the player

    currentActor_ = &scheduler_.nextTurn();
    processMonsterTurns();
    advanceTurnsUntilPlayerCanAct();
    return true;
}

void Application::updateFieldOfView() {
    std::vector<Position> visible = computeFieldOfView(map_, player_.position(), kSightRadius);
    exploredMap_.update(visible);
}

void Application::selectClass(PlayerClass cls) {
    soundManager_.play(SoundEffect::Select);
    playerClass_ = cls;
    player_.inventory() = Inventory{};
    player_.baseStats() = statsForClass(cls);
    player_.stats() = player_.baseStats();
    player_.unspentAttributePoints() = 0;
    player_.statusEffects().active().clear();
    nextItemId_ = 1;
    runeChoiceAvailable_ = false;
    pendingFinalVictory_ = false;
    pendingHybridChoices_.clear();
    pendingLevelUpFromLevel_ = 2;
    loot_.restore(std::random_device{}());
    player_.talents() = talentSetForClass(cls);
    // A fresh class selection is a genuinely new character -- starts at
    // level 1 with 0 XP, same as anyone picking up the game for the
    // first time, regardless of what level a previous run (via this
    // same long-lived player_ object) happened to reach. Loading a save
    // instead (see loadGame()) is the only path that should ever set
    // these to anything else.
    player_.level() = 1;
    player_.xp() = 0;
    player_.hybridSpecced() = false;
    currentFloor_ = 1;
    mode_ = GameMode::Playing;
    regenerateLevel(kInitialSeed);
}

void Application::regenerateLevel(unsigned int seed) {
    runesOpen_ = false;
    inventoryOpen_ = false;
    inventorySelection_ = 0;
    groundItems_.clear();
    cancelTargeting();
    mousePixel_.reset();
    talentPage_ = 0;
    DungeonGenerationParams params; // defaults, then floor-gated below
    // Only specific floors generate with a boss room at all -- every
    // other floor is a pure "clear it, find the door" dungeon. kFinalFloor
    // (10) is a placeholder using the same GoblinWarlord as
    // kFirstBossFloor (5) for now -- the actual Lich (see ROADMAP.md)
    // is a separate, later piece of work; this gets the full 10-floor
    // structure and victory gating correct end to end first.
    params.includeBossRoom = (currentFloor_ == kFirstBossFloor || currentFloor_ == kFinalFloor);
    const GeneratedDungeon dungeon = generateDungeon(params, seed);

    map_ = dungeon.map;

    player_.setPosition(dungeon.playerStart);
    player_.stats().hp = player_.stats().maxHp;
    player_.stats().mana = player_.stats().maxMana;
    player_.talents().resetCooldowns();

    boss_ = nullptr; // clear before repopulating -- see checkAndHandleDeath for why this matters
    monsters_.clear();
    // Prompt 22: the tier every regular monster in this dungeon spawns
    // at, derived from the player's current level. Computed once per
    // regeneration, not per monster -- every monster in a given dungeon
    // is the same tier as every other (see MonsterTier.hpp's own
    // comment for why a simpler uniform-tier-per-level-band was chosen
    // over mixed-tier spawning within one dungeon).
    const MonsterTier tier = tierForLevel(player_.level());
    for (std::size_t i = 0; i < dungeon.otherRoomCenters.size() && i < kRoster.size(); ++i) {
        monsters_.push_back(createMonster(kRoster[i], dungeon.otherRoomCenters[i], tier));
    }
    if (dungeon.hasBossRoom) {
        // The boss always spawns at Base tier regardless of `tier`
        // above -- see createMonster()'s GoblinWarlord/Lich cases for
        // why. Which boss depends on which floor this is: the Lich is
        // the true final fight, not a placeholder like it was before
        // this was actually built.
        const MonsterType bossType =
            (currentFloor_ == kFinalFloor) ? MonsterType::Lich : MonsterType::GoblinWarlord;
        monsters_.push_back(createMonster(bossType, dungeon.bossRoomCenter));
        boss_ = monsters_.back().get();
    }

    // The floor-transition door: on a boss floor it sits exactly where
    // the boss stands, so the player can only reach it (the boss blocks
    // that tile like any other actor, no special "is the boss dead yet"
    // check needed) once the fight is actually won. On a non-boss floor
    // it sits at the last regular room's center -- the same room the
    // shortcut protection above keeps reachable only via the full
    // chain, so reaching the door still means genuinely working through
    // the floor. kFinalFloor has no door at all: reaching it is the
    // end of the run, handled entirely by checkAndHandleDeath's victory
    // branch instead.
    if (currentFloor_ != kFinalFloor) {
        const Position doorPosition =
            dungeon.hasBossRoom ? dungeon.bossRoomCenter : dungeon.otherRoomCenters.back();
        map_.setTile(doorPosition.x, doorPosition.y, Tile{TileType::Door, true, true});
    }

    spawnFixedItems();
    spawnFloorChest();
    exploredMap_ = ExploredMap(map_);

    scheduler_ = TurnScheduler{};
    scheduler_.add(player_);
    for (auto& m : monsters_) {
        scheduler_.add(*m);
    }

    currentActor_ = &scheduler_.nextTurn();
    processMonsterTurns();
    advanceTurnsUntilPlayerCanAct();
    updateFieldOfView();

    std::cout << "Generated dungeon (seed " << seed << ", floor " << currentFloor_
              << "): " << map_.width() << 'x' << map_.height() << ", " << dungeon.roomCount
              << " rooms, " << monsters_.size() << " monsters"
              << (dungeon.hasBossRoom ? " (boss present)" : " (no boss this run)")
              << ", player start (" << dungeon.playerStart.x << ',' << dungeon.playerStart.y
              << ")" << std::endl;
    // Deliberately raw std::cout above, not log() -- logMessages_ is a
    // display buffer for *combat/play* events; this generation summary
    // is a one-time diagnostic that fires before the player has done
    // anything, and would just be a stale, disconnected first line
    // sitting in the on-screen log for the rest of the run.
}

void Application::update() {
    // Nothing here yet -- movement and talents are applied directly in
    // processEvents() since this is a turn-based game with no
    // time-based simulation to advance between player inputs.
}

} // namespace engine
