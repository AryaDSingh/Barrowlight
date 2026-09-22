#include "core/Application.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "core/SaveGame.hpp"
#include "entities/AttributeFormulas.hpp"
#include "entities/MonsterFactory.hpp"
#include "entities/PlayerClassFactory.hpp"
#include "entities/StatusEffectLogic.hpp"
#include "entities/TalentEffects.hpp"
#include "world/DungeonGenerator.hpp"
#include "world/FieldOfView.hpp"

namespace engine {

namespace {
constexpr unsigned int kWindowWidth = 1280;
constexpr unsigned int kWindowHeight = 720;
constexpr char kWindowTitle[] = "Roguelike Engine - Dev Window";
constexpr float kTileSize = 32.f;
constexpr int kSightRadius = 8;
constexpr unsigned int kInitialSeed = 1337;

// Passive regen, applied once per player turn (see
// advanceTurnsUntilPlayerCanAct) regardless of what action was taken --
// without it, a fight that outlasts the player's starting mana pool
// leaves them with talents they can see are "off cooldown" but can never
// actually afford again. Modest on purpose: enough to matter over a
// multi-turn fight, not enough to make mana cost meaningless.
constexpr int kManaRegenPerTurn = 2;

// Relative to wherever the executable is launched from -- same
// reasoning as avoiding data/ file loading elsewhere in this project
// (see ARCHITECTURE_DECISIONS.md): resolving the executable's own
// directory needs platform-specific APIs this project has deliberately
// avoided needing so far.
constexpr const char* kSaveFilePath = "savegame.txt";

// Same relative-path reasoning as kSaveFilePath.
constexpr const char* kFontPath = "assets/fonts/DejaVuSansMono.ttf";

// The full regular roster, in the order rooms get populated. Fewer than
// 6 non-player, non-boss rooms means a partial roster, not a crash --
// see regenerateLevel.
constexpr std::array<MonsterType, 6> kRoster = {
    MonsterType::Goblin, MonsterType::Spider, MonsterType::Ogre,
    MonsterType::Archer, MonsterType::Shaman, MonsterType::Bomber,
};

sf::Color dim(sf::Color c) {
    constexpr float kDimFactor = 0.35f;
    return sf::Color(static_cast<std::uint8_t>(c.r * kDimFactor),
                      static_cast<std::uint8_t>(c.g * kDimFactor),
                      static_cast<std::uint8_t>(c.b * kDimFactor));
}

bool isAdjacent(Position a, Position b) {
    const int dx = std::abs(a.x - b.x);
    const int dy = std::abs(a.y - b.y);
    return (dx + dy) == 1;
}

int distanceSquared(Position a, Position b) {
    const int dx = a.x - b.x;
    const int dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// No sprite/tile art exists yet (see ARCHITECTURE_DECISIONS.md) -- each
// enemy type gets a distinct flat color so the roster is at least
// visually distinguishable at a glance.
sf::Color monsterColor(const std::string& name) {
    if (name == "Goblin") return sf::Color(200, 60, 60);
    if (name == "Spider") return sf::Color(120, 200, 60);
    if (name == "Ogre") return sf::Color(140, 90, 50);
    if (name == "Archer") return sf::Color(210, 170, 60);
    if (name == "Shaman") return sf::Color(170, 70, 210);
    if (name == "Bomber") return sf::Color(230, 110, 30);
    if (name == "Goblin Warlord") return sf::Color(255, 215, 0);
    return sf::Color(190, 190, 190);
}
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

    // A missing/unreadable font doesn't crash the game -- sf::Text just
    // silently draws nothing with an unloaded sf::Font, so the rest of
    // the HUD (bars, tiles, console output) still works. Logged once,
    // plainly, rather than treated as fatal. Raw std::cout here, not
    // log() -- logMessages_ is empty and meaningless before the window
    // and constructor have even finished.
    if (!font_.openFromFile(kFontPath)) {
        std::cout << "Warning: failed to load font at " << kFontPath
                   << " -- on-screen text will not render.\n";
    }

    window_.setFramerateLimit(60);
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

void Application::drawText(const std::string& text, float x, float y, unsigned int size,
                            sf::Color color) {
    sf::Text sfText(font_);
    sfText.setString(text);
    sfText.setCharacterSize(size);
    sfText.setFillColor(color);
    sfText.setPosition({x, y});
    window_.draw(sfText);
}

void Application::processEvents() {
    while (const std::optional event = window_.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window_.close();
        }

        if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            if (keyPressed->code == sf::Keyboard::Key::Escape) {
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
                // applies yet.
                if (keyPressed->code == sf::Keyboard::Key::Num1) {
                    selectClass(PlayerClass::Spellblade);
                } else if (keyPressed->code == sf::Keyboard::Key::Num2) {
                    selectClass(PlayerClass::Marauder);
                } else if (keyPressed->code == sf::Keyboard::Key::Num3) {
                    selectClass(PlayerClass::Archer);
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
                    tryUseTalent(0);
                    break;
                case sf::Keyboard::Key::Num2:
                    tryUseTalent(1);
                    break;
                case sf::Keyboard::Key::Num3:
                    tryUseTalent(2);
                    break;
                case sf::Keyboard::Key::Num4:
                    tryUseTalent(3);
                    break;
                case sf::Keyboard::Key::Num5:
                    tryUseTalent(4);
                    break;
                case sf::Keyboard::Key::Num6:
                    tryUseTalent(5);
                    break;
                case sf::Keyboard::Key::Num7:
                    tryUseTalent(6);
                    break;
                case sf::Keyboard::Key::Num8:
                    tryUseTalent(7);
                    break;
                case sf::Keyboard::Key::Num9:
                    tryUseTalent(8);
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

Position Application::resolveBlinkDestination(Position direction, int maxDistance) {
    Position current = player_.position();
    for (int i = 0; i < maxDistance; ++i) {
        const Position next{current.x + direction.x, current.y + direction.y};
        if (!map_.isWalkable(next.x, next.y) || isOccupied(next, &player_)) {
            break;
        }
        current = next;
    }
    return current;
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
    lastMoveDirection_ = Position{dx, dy};
    player_.talents().tickCooldowns();
    updateFieldOfView();

    currentActor_ = &scheduler_.nextTurn();
    processMonsterTurns();
    advanceTurnsUntilPlayerCanAct();
    return true;
}

bool Application::tryUseTalent(std::size_t talentIndex) {
    if (!window_.isOpen()) {
        return false;
    }

    const std::vector<Talent>& talents = player_.talents().knownTalents();
    if (talentIndex >= talents.size()) {
        return false;
    }
    const Talent& talent = talents[talentIndex];

    if (!player_.talents().isReady(talentIndex)) {
        log(talent.name, " is on cooldown (", player_.talents().cooldownRemaining(talentIndex),
            " turns left)");
        return false;
    }
    if (player_.stats().mana < talent.manaCost) {
        log("Not enough mana for ", talent.name, " (", player_.stats().mana, "/",
            talent.manaCost, " needed)");
        return false;
    }
    if (talent.hpCost > 0 && player_.stats().hp <= talent.hpCost) {
        log("Not enough hp to safely cast ", talent.name);
        return false;
    }

    std::vector<Actor*> affected;
    Position blinkDestination = player_.position();

    if (talent.shape == EffectShape::Movement) {
        blinkDestination = resolveBlinkDestination(lastMoveDirection_, talent.moveDistance);
        if (blinkDestination.x == player_.position().x &&
            blinkDestination.y == player_.position().y) {
            log(talent.name, " has nowhere to go that direction");
            return false;
        }
    } else if (talent.shape == EffectShape::AreaAroundSelf) {
        affected = actorsWithinRadius(player_.position(), talent.areaRadius);
    } else {
        Actor* anchor = nullptr;
        if (talent.targeting == TargetingMode::Self) {
            // Renewal (Prompt 14 follow-up): a Self-targeted,
            // SingleTarget-shaped talent affects the caster directly --
            // AreaAroundSelf (Immolate) searches for nearby *enemies*,
            // which is the wrong tool for "heal yourself," so this is a
            // separate, simpler resolution rather than reusing that path.
            anchor = &player_;
        } else if (talent.targeting == TargetingMode::AdjacentEnemy) {
            anchor = findAdjacentEnemy();
        } else if (talent.targeting == TargetingMode::RangedEnemyInSight) {
            anchor = findNearestVisibleEnemy();
        }

        if (anchor != nullptr) {
            if (talent.shape == EffectShape::AreaAroundTarget) {
                affected = actorsWithinRadius(anchor->position(), talent.areaRadius);
            } else {
                affected.push_back(anchor);
            }
        }
    }

    if (talent.shape != EffectShape::Movement && affected.empty()) {
        log("No valid target for ", talent.name);
        return false;
    }

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
            if (applyTalentDamage(talent, player_, *target)) {
                log(player_.name(), " uses ", talent.name, " on ", target->name(), " (",
                    target->stats().hp, "/", target->stats().maxHp, " hp left)");
                checkAndHandleDeath(*target);
            } else {
                log(target->name(), " dodges ", player_.name(), "'s ", talent.name, "!");
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
            const Position retreatDestination =
                resolveBlinkDestination(awayDirection, talent.retreatDistance);
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

void Application::processMonsterTurns() {
    while (window_.isOpen() && currentActor_ != &player_) {
        Actor* actor = currentActor_;

        const bool hadPoison = actor->statusEffects().has(StatusEffectType::Poison);
        const bool stunned = tickStatusEffects(*actor);
        if (hadPoison && actor->stats().hp > 0) {
            log(actor->name(), " takes poison damage (", actor->stats().hp, "/",
                actor->stats().maxHp, " hp left)");
        }
        checkAndHandleDeath(*actor);

        if (actor->stats().hp > 0) {
            if (stunned) {
                log(actor->name(), " is stunned and loses a turn!");
            } else if (actor->ai() != nullptr) {
                const AIDecision decision =
                    actor->ai()->decideAction(*actor, map_, player_, aliveAllies(actor));
                executeAIDecision(*actor, decision);
            }
            actor->talents().tickCooldowns();
        }

        currentActor_ = &scheduler_.nextTurn();
    }

    removeDeadMonsters();
}

void Application::advanceTurnsUntilPlayerCanAct() {
    // Safety bound, not expected to be hit given finite stun durations --
    // defensive against a genuine bug rather than an anticipated case.
    constexpr int kMaxStunSkips = 50;

    for (int i = 0; i < kMaxStunSkips && window_.isOpen(); ++i) {
        player_.stats().mana =
            std::min(player_.stats().mana + kManaRegenPerTurn, player_.stats().maxMana);

        const bool hadPoison = player_.statusEffects().has(StatusEffectType::Poison);
        const bool stunned = tickStatusEffects(player_);
        if (hadPoison && player_.stats().hp > 0) {
            log("Poison deals damage (", player_.stats().hp, "/", player_.stats().maxHp,
                " hp left)");
        }
        checkAndHandleDeath(player_);
        if (!window_.isOpen()) {
            return;
        }
        if (!stunned) {
            return; // genuinely the player's turn now
        }
        log("You are stunned and lose a turn!");
        currentActor_ = &scheduler_.nextTurn();
        processMonsterTurns();
    }
}

void Application::executeAIDecision(Actor& actor, const AIDecision& decision) {
    if (!decision.announcement.empty()) {
        log(decision.announcement);
    }

    switch (decision.type) {
        case AIActionType::Move:
            if (map_.isWalkable(decision.movePosition.x, decision.movePosition.y) &&
                !isOccupied(decision.movePosition, &actor)) {
                actor.setPosition(decision.movePosition);
            }
            break;

        case AIActionType::Attack:
        case AIActionType::UseAbility: {
            if (decision.target == nullptr) {
                break;
            }

            bool dodged = false;
            if (decision.attackPower > 0) {
                dodged = rollDodge(decision.target->stats().dexterity);
                if (dodged) {
                    log(decision.target->name(), " dodges ", actor.name(), "'s attack!");
                } else {
                    int damage = decision.attackPower;
                    damage += (decision.damageType == DamageType::Physical)
                                  ? physicalDamageBonus(actor.stats().strength)
                                  : magicDamageBonus(actor.stats().intelligence);
                    if (actor.statusEffects().has(StatusEffectType::Empowered)) {
                        damage += actor.statusEffects().magnitudeOf(StatusEffectType::Empowered);
                    }
                    decision.target->stats().hp -= damage;
                    log(actor.name(), " hits ", decision.target->name(), " for ", damage, " (",
                        decision.target->stats().hp, "/", decision.target->stats().maxHp,
                        " hp left)");
                    checkAndHandleDeath(*decision.target);
                }
            }

            // A dodged hit lands no on-hit effect either -- avoiding the
            // blow avoids the poison that would have ridden in on it.
            if (!dodged && decision.effectToApply.has_value() && decision.target->stats().hp > 0) {
                decision.target->statusEffects().apply(*decision.effectToApply);
                log(decision.target->name(), " is affected by ",
                    (decision.effectToApply->type == StatusEffectType::Poison    ? "poison"
                     : decision.effectToApply->type == StatusEffectType::Stun ? "a stun"
                                                                               : "a buff"),
                    "!");
            }

            if (decision.type == AIActionType::UseAbility &&
                decision.abilityIndex < actor.talents().knownTalents().size()) {
                actor.talents().startCooldown(decision.abilityIndex);
            }
            break;
        }

        case AIActionType::SelfBuff:
            if (decision.effectToApply.has_value()) {
                actor.statusEffects().apply(*decision.effectToApply);
            }
            break;

        case AIActionType::Wait:
            break;
    }
}

void Application::checkAndHandleDeath(Actor& actor) {
    if (mode_ == GameMode::GameOver) {
        // Already handled -- without this, a dead player's hp stays <=
        // 0 indefinitely, and this function gets called again on every
        // subsequent status-effect tick (advanceTurnsUntilPlayerCanAct
        // calls it unconditionally each iteration), re-running the
        // entire death branch below repeatedly. Invisible before this
        // prompt, since window_.close() used to make window_.isOpen()
        // false immediately, short-circuiting those later calls before
        // they ever reached here -- now that death leads to a GameOver
        // screen instead of closing the window, that accidental
        // short-circuit is gone, so this needs to be explicit.
        return;
    }

    if (actor.stats().hp > 0) {
        return;
    }

    if (&actor == &player_) {
        log("You have died!");
        // As of Prompt 17: a real GameOver screen instead of closing
        // the window outright. selectClass() (reachable from the
        // ClassSelection screen this leads to) already does a complete
        // reset of player_/map_/monsters_/scheduler_, so nothing extra
        // needs cleaning up here -- transitioning mode_ is the whole fix.
        mode_ = GameMode::GameOver;
        wonGame_ = false;
        return;
    }

    if (&actor == boss_) {
        log(actor.name(), " falls! You have slain the Goblin Warlord!");
        boss_ = nullptr; // must clear before removeDeadMonsters() erases the underlying object
        scheduler_.remove(actor);
        // Defeating the boss ends the run in victory outright, even if
        // other regular monsters are still alive elsewhere in the
        // dungeon -- it's the set-piece finale (Prompt 11), not one
        // more kill among many. This is also the very first place
        // "victory" has existed as a real game state at all; before
        // this, killing the boss just logged a message and let play
        // continue with nothing actually won.
        mode_ = GameMode::GameOver;
        wonGame_ = true;
        return;
    }

    log(actor.name(), " dies!");
    scheduler_.remove(actor);
    // Actual erase from monsters_ happens in removeDeadMonsters(), after
    // the current processMonsterTurns() loop finishes -- never mid-loop,
    // to avoid invalidating pointers still in use this turn.
}

std::vector<Actor*> Application::aliveAllies(const Actor* exclude) {
    std::vector<Actor*> allies;
    for (auto& m : monsters_) {
        if (m.get() != exclude && m->stats().hp > 0) {
            allies.push_back(m.get());
        }
    }
    return allies;
}

Actor* Application::findAdjacentEnemy() {
    for (auto& m : monsters_) {
        if (m->stats().hp > 0 && isAdjacent(player_.position(), m->position())) {
            return m.get();
        }
    }
    return nullptr;
}

Actor* Application::findNearestVisibleEnemy() {
    Actor* nearest = nullptr;
    int nearestDistSq = std::numeric_limits<int>::max();
    for (auto& m : monsters_) {
        if (m->stats().hp <= 0) {
            continue;
        }
        if (exploredMap_.at(m->position().x, m->position().y) != Visibility::Visible) {
            continue;
        }
        const int distSq = distanceSquared(player_.position(), m->position());
        if (distSq < nearestDistSq) {
            nearestDistSq = distSq;
            nearest = m.get();
        }
    }
    return nearest;
}

std::vector<Actor*> Application::actorsWithinRadius(Position center, int radius) {
    std::vector<Actor*> result;
    for (auto& m : monsters_) {
        if (m->stats().hp > 0 && distanceSquared(center, m->position()) <= radius * radius) {
            result.push_back(m.get());
        }
    }
    return result;
}

void Application::removeDeadMonsters() {
    monsters_.erase(std::remove_if(monsters_.begin(), monsters_.end(),
                                    [](const std::unique_ptr<Monster>& m) {
                                        return m->stats().hp <= 0;
                                    }),
                     monsters_.end());
}

void Application::updateFieldOfView() {
    std::vector<Position> visible = computeFieldOfView(map_, player_.position(), kSightRadius);
    exploredMap_.update(visible);
}

void Application::selectClass(PlayerClass cls) {
    playerClass_ = cls;
    player_.stats() = statsForClass(cls);
    player_.talents() = talentSetForClass(cls);
    mode_ = GameMode::Playing;
    regenerateLevel(kInitialSeed);
}

void Application::regenerateLevel(unsigned int seed) {
    const DungeonGenerationParams params; // defaults
    const GeneratedDungeon dungeon = generateDungeon(params, seed);

    map_ = dungeon.map;

    player_.setPosition(dungeon.playerStart);
    player_.stats().hp = player_.stats().maxHp;
    player_.stats().mana = player_.stats().maxMana;
    player_.talents().resetCooldowns();

    boss_ = nullptr; // clear before repopulating -- see checkAndHandleDeath for why this matters
    monsters_.clear();
    for (std::size_t i = 0; i < dungeon.otherRoomCenters.size() && i < kRoster.size(); ++i) {
        monsters_.push_back(createMonster(kRoster[i], dungeon.otherRoomCenters[i]));
    }
    if (dungeon.hasBossRoom) {
        monsters_.push_back(createMonster(MonsterType::GoblinWarlord, dungeon.bossRoomCenter));
        boss_ = monsters_.back().get();
    }

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

    std::cout << "Generated dungeon (seed " << seed << "): " << map_.width() << 'x'
              << map_.height() << ", " << dungeon.roomCount << " rooms, " << monsters_.size()
              << " monsters" << (dungeon.hasBossRoom ? " (boss present)" : " (no boss this run)")
              << ", player start (" << dungeon.playerStart.x << ',' << dungeon.playerStart.y
              << ")" << std::endl;
    // Deliberately raw std::cout above, not log() -- logMessages_ is a
    // display buffer for *combat/play* events; this generation summary
    // is a one-time diagnostic that fires before the player has done
    // anything, and would just be a stale, disconnected first line
    // sitting in the on-screen log for the rest of the run.
}

void Application::saveGame() {
    SaveGameState state;
    state.map = map_;
    state.exploredMap = exploredMap_;
    state.playerPosition = player_.position();
    state.playerClass = playerClass_;
    state.playerStats = player_.stats();
    state.lastMoveDirection = lastMoveDirection_;

    const std::vector<Talent>& talents = player_.talents().knownTalents();
    state.playerCooldowns.resize(talents.size());
    for (std::size_t i = 0; i < talents.size(); ++i) {
        state.playerCooldowns[i] = player_.talents().cooldownRemaining(i);
    }
    state.playerStatusEffects = player_.statusEffects().active();

    for (auto& m : monsters_) {
        SaveGameState::MonsterSaveData data;
        data.type = m->type();
        data.position = m->position();
        data.hp = m->stats().hp;
        data.maxHp = m->stats().maxHp;
        data.isBoss = (m.get() == boss_);
        data.statusEffects = m->statusEffects().active();
        state.monsters.push_back(std::move(data));
    }

    if (engine::saveGame(state, kSaveFilePath)) {
        log("Game saved.");
    } else {
        log("Failed to save game (could not write ", kSaveFilePath, ").");
    }
}

void Application::loadGame() {
    const std::optional<SaveGameState> loaded = engine::loadGame(kSaveFilePath);
    if (!loaded.has_value()) {
        log("No valid save file found (", kSaveFilePath, ").");
        return;
    }
    const SaveGameState& state = *loaded;

    // Reachable from ClassSelection now (see processEvents()) -- a
    // load that started there needs to actually switch to Playing, the
    // same way selectClass() does after a real choice. Harmless if
    // we're already in Playing (regenerateLevel-style "replace
    // everything" below overwrites map_/monsters_/etc regardless of
    // which mode we were in before this).
    mode_ = GameMode::Playing;

    map_ = state.map;
    exploredMap_ = state.exploredMap;

    player_.setPosition(state.playerPosition);
    playerClass_ = state.playerClass;
    // Reconstruct the correct class's talent list *before* applying
    // saved cooldowns below -- playerCooldowns is a list of remaining-
    // cooldown values applied positionally by index (see
    // TalentSet::setCooldownRemaining's loop below), so it only means
    // the right thing once player_'s talents actually match the class
    // the save was made as. Loading a Marauder save while player_ was
    // still configured as a Spellblade (or vice versa) would otherwise
    // apply a Slam's cooldown value to whatever the Spellblade's talent
    // 0 happens to be, silently wrong rather than caught by anything.
    player_.talents() = talentSetForClass(playerClass_);
    player_.stats() = state.playerStats;
    lastMoveDirection_ = state.lastMoveDirection;

    for (std::size_t i = 0;
         i < state.playerCooldowns.size() && i < player_.talents().knownTalents().size(); ++i) {
        player_.talents().setCooldownRemaining(i, state.playerCooldowns[i]);
    }
    player_.statusEffects().active() = state.playerStatusEffects;

    boss_ = nullptr;
    monsters_.clear();
    for (const SaveGameState::MonsterSaveData& m : state.monsters) {
        std::unique_ptr<Monster> monster = createMonster(m.type, m.position);
        monster->stats().hp = m.hp;
        monster->stats().maxHp = m.maxHp;
        monster->statusEffects().active() = m.statusEffects;
        if (m.isBoss) {
            boss_ = monster.get();
        }
        monsters_.push_back(std::move(monster));
    }

    scheduler_ = TurnScheduler{};
    scheduler_.add(player_);
    for (auto& m : monsters_) {
        scheduler_.add(*m);
    }

    // Deliberately just this, unlike regenerateLevel() -- a freshly
    // generated player never has status effects yet, so running
    // processMonsterTurns()/advanceTurnsUntilPlayerCanAct() there is
    // harmless. A *loaded* player might already be poisoned or stunned;
    // doing the same here would immediately re-tick their status effects
    // before they've taken any action since resuming, applying an extra
    // tick beyond what was actually saved. Any monster whose turn is
    // technically still pending gets caught up naturally the moment the
    // player next moves or casts (tryMovePlayer/tryUseTalent already
    // call processMonsterTurns() themselves) -- the same mechanism that
    // handles it in ordinary play, not a special case for loading.
    currentActor_ = &scheduler_.nextTurn();

    log("Game loaded: ", monsters_.size(), " monsters",
        (boss_ != nullptr ? " (boss present)" : ""), ", player at (", player_.position().x, ',',
        player_.position().y, "), ", player_.stats().hp, '/', player_.stats().maxHp, " hp.");
}

void Application::update() {
    // Nothing here yet -- movement and talents are applied directly in
    // processEvents() since this is a turn-based game with no
    // time-based simulation to advance between player inputs.
}

void Application::renderClassSelection() {
    drawText("Choose your class", 60.f, 60.f, 28, sf::Color(230, 230, 230));

    drawText("1. Spellblade", 60.f, 130.f, 20, sf::Color(120, 170, 255));
    drawText("Strength + Intelligence hybrid. Melee and magic both,", 80.f, 158.f, 14,
             sf::Color(190, 190, 190));
    drawText("a real mana pool, a full 9-talent kit across two trees.", 80.f, 176.f, 14,
             sf::Color(190, 190, 190));

    drawText("2. Marauder", 60.f, 230.f, 20, sf::Color(230, 120, 90));
    drawText("Pure Strength. A free spammable basic attack, hp as the", 80.f, 258.f, 14,
             sf::Color(190, 190, 190));
    drawText("resource that matters, and the hardest single hit either", 80.f, 276.f, 14,
             sf::Color(190, 190, 190));
    drawText("class has.", 80.f, 294.f, 14, sf::Color(190, 190, 190));

    drawText("3. Archer", 60.f, 330.f, 20, sf::Color(120, 230, 140));
    drawText("Pure Dexterity. The lowest hp of any class, but the highest", 80.f, 358.f, 14,
             sf::Color(190, 190, 190));
    drawText("possible dodge chance -- survives by not getting hit at all.", 80.f, 376.f, 14,
             sf::Color(190, 190, 190));
    drawText("Vault Kick lets you strike an adjacent enemy and leap back", 80.f, 394.f, 14,
             sf::Color(190, 190, 190));
    drawText("out of melee range in the same motion.", 80.f, 412.f, 14, sf::Color(190, 190, 190));

    drawText("Press 1, 2 or 3 to begin.", 60.f, 460.f, 16, sf::Color(150, 150, 150));
}

void Application::renderGameOver() {
    if (wonGame_) {
        drawText("Victory!", 60.f, 60.f, 32, sf::Color(255, 215, 0));
        drawText("You have slain the Goblin Warlord.", 60.f, 120.f, 18, sf::Color(210, 210, 210));
    } else {
        drawText("You Died", 60.f, 60.f, 32, sf::Color(200, 50, 50));
        drawText("The dungeon claims another.", 60.f, 120.f, 18, sf::Color(210, 210, 210));
    }
    drawText("Press Enter to return to class selection.", 60.f, 180.f, 16,
              sf::Color(150, 150, 150));
}

void Application::render() {
    window_.clear(sf::Color(10, 10, 14));

    if (mode_ == GameMode::ClassSelection) {
        renderClassSelection();
        window_.display();
        return;
    }

    if (mode_ == GameMode::GameOver) {
        renderGameOver();
        window_.display();
        return;
    }

    for (int y = 0; y < map_.height(); ++y) {
        for (int x = 0; x < map_.width(); ++x) {
            const Visibility vis = exploredMap_.at(x, y);
            if (vis == Visibility::Hidden) {
                continue;
            }

            const sf::Color baseColor = map_.tileAt(x, y).type == TileType::Wall
                                             ? sf::Color(45, 45, 52)
                                             : sf::Color(90, 90, 100);

            sf::RectangleShape tileShape({kTileSize - 1.f, kTileSize - 1.f});
            tileShape.setPosition(
                {static_cast<float>(x) * kTileSize, static_cast<float>(y) * kTileSize});
            tileShape.setFillColor(vis == Visibility::Visible ? baseColor : dim(baseColor));
            window_.draw(tileShape);
        }
    }

    for (auto& m : monsters_) {
        if (m->stats().hp <= 0) {
            continue;
        }
        if (exploredMap_.at(m->position().x, m->position().y) != Visibility::Visible) {
            continue; // only draw what the player can currently see -- see Prompt 7 notes
        }

        sf::RectangleShape monsterShape({kTileSize - 1.f, kTileSize - 1.f});
        monsterShape.setPosition({static_cast<float>(m->position().x) * kTileSize,
                                   static_cast<float>(m->position().y) * kTileSize});
        monsterShape.setFillColor(monsterColor(m->name()));
        window_.draw(monsterShape);

        const float hpFraction =
            static_cast<float>(m->stats().hp) / static_cast<float>(m->stats().maxHp);
        sf::RectangleShape hpBack({kTileSize - 1.f, 4.f});
        hpBack.setPosition({static_cast<float>(m->position().x) * kTileSize,
                             static_cast<float>(m->position().y) * kTileSize - 6.f});
        hpBack.setFillColor(sf::Color(40, 20, 20));
        window_.draw(hpBack);

        sf::RectangleShape hpFront({(kTileSize - 1.f) * hpFraction, 4.f});
        hpFront.setPosition({static_cast<float>(m->position().x) * kTileSize,
                              static_cast<float>(m->position().y) * kTileSize - 6.f});
        hpFront.setFillColor(sf::Color(220, 60, 60));
        window_.draw(hpFront);
    }

    sf::RectangleShape playerShape({kTileSize - 1.f, kTileSize - 1.f});
    playerShape.setPosition({static_cast<float>(player_.position().x) * kTileSize,
                              static_cast<float>(player_.position().y) * kTileSize});
    playerShape.setFillColor(sf::Color(240, 200, 60));
    window_.draw(playerShape);

    constexpr float kBarWidth = 200.f;
    constexpr float kBarHeight = 14.f;

    const float hpFraction =
        static_cast<float>(player_.stats().hp) / static_cast<float>(player_.stats().maxHp);
    sf::RectangleShape hpBack({kBarWidth, kBarHeight});
    hpBack.setPosition({10.f, 10.f});
    hpBack.setFillColor(sf::Color(40, 20, 20));
    window_.draw(hpBack);
    sf::RectangleShape hpFront({kBarWidth * hpFraction, kBarHeight});
    hpFront.setPosition({10.f, 10.f});
    hpFront.setFillColor(sf::Color(200, 50, 50));
    window_.draw(hpFront);
    {
        std::ostringstream oss;
        oss << player_.stats().hp << '/' << player_.stats().maxHp;
        drawText(oss.str(), 10.f + kBarWidth + 8.f, 10.f, 13, sf::Color::White);
    }

    const float manaFraction = player_.stats().maxMana > 0
                                    ? static_cast<float>(player_.stats().mana) /
                                          static_cast<float>(player_.stats().maxMana)
                                    : 0.f;
    sf::RectangleShape manaBack({kBarWidth, kBarHeight});
    manaBack.setPosition({10.f, 28.f});
    manaBack.setFillColor(sf::Color(20, 20, 40));
    window_.draw(manaBack);
    sf::RectangleShape manaFront({kBarWidth * manaFraction, kBarHeight});
    manaFront.setPosition({10.f, 28.f});
    manaFront.setFillColor(sf::Color(50, 90, 220));
    window_.draw(manaFront);
    {
        std::ostringstream oss;
        oss << player_.stats().mana << '/' << player_.stats().maxMana;
        drawText(oss.str(), 10.f + kBarWidth + 8.f, 28.f, 13, sf::Color::White);
    }

    // Talent list -- names + live cooldown status, replacing "console
    // only" as the sole way to know what's on cooldown. Dimmed while on
    // cooldown, full brightness once ready, same "Visible vs Remembered"
    // dimming *idea* as the map (Prompt 6), applied to UI state instead
    // of fog-of-war. A semi-transparent backing panel sits behind it --
    // this corner of the map is visible dungeon floor, not empty
    // background, and text alone (however legible up close) read as
    // genuinely unfinished sitting directly on top of tiles with no
    // separation. Confirmed by actually looking at a screenshot, not
    // just trusting the draw order would look fine.
    {
        constexpr float kTalentListX = 10.f;
        constexpr float kTalentListY = 58.f;
        constexpr float kTalentLineHeight = 16.f;
        const std::vector<Talent>& talents = player_.talents().knownTalents();

        sf::RectangleShape panel(
            {230.f, static_cast<float>(talents.size()) * kTalentLineHeight + 6.f});
        panel.setPosition({kTalentListX - 4.f, kTalentListY - 4.f});
        panel.setFillColor(sf::Color(0, 0, 0, 160));
        window_.draw(panel);

        for (std::size_t i = 0; i < talents.size(); ++i) {
            const bool ready = player_.talents().isReady(i);
            std::ostringstream oss;
            oss << (i + 1) << ". " << talents[i].name;
            if (ready) {
                oss << " [Ready]";
            } else {
                oss << " [" << player_.talents().cooldownRemaining(i) << "]";
            }
            drawText(oss.str(), kTalentListX, kTalentListY + static_cast<float>(i) * kTalentLineHeight,
                      13, ready ? sf::Color(215, 215, 215) : sf::Color(110, 110, 110));
        }
    }

    // On-screen combat log -- bottom-left, oldest message at top of the
    // block so new lines settle at the bottom, matching how a chat/log
    // window conventionally reads. Same backing-panel reasoning as the
    // talent list above.
    {
        constexpr float kLogX = 10.f;
        constexpr float kLogLineHeight = 16.f;
        const float logBottomY =
            static_cast<float>(kWindowHeight) - 10.f - kLogLineHeight;
        float logY = logBottomY - static_cast<float>(logMessages_.size() - 1) * kLogLineHeight;
        if (logMessages_.empty()) {
            logY = logBottomY;
        }

        if (!logMessages_.empty()) {
            sf::RectangleShape panel(
                {600.f, static_cast<float>(logMessages_.size()) * kLogLineHeight + 6.f});
            panel.setPosition({kLogX - 4.f, logY - 4.f});
            panel.setFillColor(sf::Color(0, 0, 0, 160));
            window_.draw(panel);
        }

        for (const std::string& message : logMessages_) {
            drawText(message, kLogX, logY, 13, sf::Color(210, 210, 210));
            logY += kLogLineHeight;
        }
    }

    // A prominent, top-center boss bar -- distinct from the small
    // floating per-monster bars -- only shown when the boss is alive AND
    // currently visible, same consistency rule as everything else
    // (Prompt 7's "only render what's currently visible").
    if (boss_ != nullptr && boss_->stats().hp > 0 &&
        exploredMap_.at(boss_->position().x, boss_->position().y) == Visibility::Visible) {
        constexpr float kBossBarWidth = 500.f;
        constexpr float kBossBarHeight = 20.f;
        const float bossX = (static_cast<float>(kWindowWidth) - kBossBarWidth) / 2.f;

        const float bossHpFraction =
            static_cast<float>(boss_->stats().hp) / static_cast<float>(boss_->stats().maxHp);
        sf::RectangleShape bossBack({kBossBarWidth, kBossBarHeight});
        bossBack.setPosition({bossX, 10.f});
        bossBack.setFillColor(sf::Color(35, 30, 10));
        window_.draw(bossBack);
        sf::RectangleShape bossFront({kBossBarWidth * bossHpFraction, kBossBarHeight});
        bossFront.setPosition({bossX, 10.f});
        bossFront.setFillColor(sf::Color(255, 215, 0));
        window_.draw(bossFront);
        drawText(boss_->name(), bossX, 10.f - 18.f, 14, sf::Color(255, 215, 0));
    }

    window_.display();
}

} // namespace engine
