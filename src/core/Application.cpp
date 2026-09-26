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
constexpr float kTileSize = static_cast<float>(playLayout::tileSize);
constexpr unsigned int kMapWidth = playLayout::mapWidth;
constexpr unsigned int kMapHeight = playLayout::mapHeight;
constexpr float kMapTop = static_cast<float>(playLayout::mapTop);
constexpr int kSightRadius = 8;
constexpr unsigned int kInitialSeed = 1337;

// The multi-floor dungeon progression: floors 1 through kFinalFloor,
// each a separate generated dungeon reached by walking through the
// previous floor's door. Only kFirstBossFloor and kFinalFloor generate
// with a boss room at all -- see regenerateLevel(). kFinalFloor's boss
// currently reuses GoblinWarlord as a placeholder (see that function's
// own comment) -- the actual Lich is separate, later work.
constexpr int kFirstBossFloor = 5;
constexpr int kFinalFloor = 10;

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

// No sprite/tile art exists yet (see ARCHITECTURE_DECISIONS.md) -- each
// enemy type gets a distinct flat color so the roster is at least
// visually distinguishable at a glance.
// Keyed by MonsterType, not the display name string -- Prompt 22's
// Elite/Nightmare tiers prefix the name ("Elite Goblin", "Nightmare
// Goblin"), which would silently fail an exact-string match like
// `name == "Goblin"` and fall through to the default gray for every
// tiered monster. MonsterType is stable regardless of tier or display
// name, so this can't have the same failure mode again.
sf::Color monsterColor(MonsterType type) {
    switch (type) {
        case MonsterType::Goblin: return sf::Color(200, 60, 60);
        case MonsterType::Spider: return sf::Color(120, 200, 60);
        case MonsterType::Ogre: return sf::Color(140, 90, 50);
        case MonsterType::Archer: return sf::Color(210, 170, 60);
        case MonsterType::Shaman: return sf::Color(170, 70, 210);
        case MonsterType::Bomber: return sf::Color(230, 110, 30);
        case MonsterType::GoblinWarlord: return sf::Color(255, 215, 0);
        case MonsterType::Lich: return sf::Color(140, 220, 210); // pale, ghostly teal
        case MonsterType::Skeleton: return sf::Color(220, 220, 200); // bone white
    }
    return sf::Color(190, 190, 190); // unreachable -- all enum values handled above
}

// A visual border color for Elite/Nightmare monsters (Prompt 22) --
// drawn as a slightly larger square behind the monster's own type-
// colored tile, so a tiered monster is identifiable at a glance without
// needing to read the combat log. Base tier returns no value: nothing
// extra is drawn, a tier-0 monster looks exactly as it always has.
std::optional<sf::Color> tierBorderColor(MonsterTier tier) {
    switch (tier) {
        case MonsterTier::Base:
            return std::nullopt;
        case MonsterTier::Elite:
            return sf::Color(255, 200, 60); // amber
        case MonsterTier::Nightmare:
            // Stark white, not a saturated color -- a deep red border
            // (an earlier attempt) blended almost invisibly into the
            // Goblin's own red tile color when checked against an
            // actual screenshot, not just reasoned about in the
            // abstract. White has to contrast against every monster
            // color in the roster at once (red, green, brown, tan,
            // purple, orange), not just look distinct in isolation.
            return sf::Color(255, 255, 255);
    }
    return std::nullopt; // unreachable
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

void Application::drawText(const std::string& text, float x, float y, unsigned int size,
                            sf::Color color) {
    sf::Text sfText(font_);
    sfText.setString(text);
    sfText.setCharacterSize(size);
    sfText.setFillColor(color);
    sfText.setPosition({x, y});
    window_.draw(sfText);
}

void Application::updateCamera() {
    // Viewport size in whole tiles -- derived from the window/tile
    // constants rather than hardcoded again, so this stays correct if
    // either ever changes.
    constexpr int kViewportWidthTiles = static_cast<int>(kMapWidth / kTileSize);
    constexpr int kViewportHeightTiles = static_cast<int>(kMapHeight / kTileSize);

    const int desiredX = player_.position().x - kViewportWidthTiles / 2;
    const int desiredY = player_.position().y - kViewportHeightTiles / 2;

    // Clamped to [0, map dimension - viewport dimension] so the camera
    // never scrolls past the map's own edges and shows empty space
    // beyond it. If the map is smaller than the viewport in either
    // dimension (not expected at the current 60x32 default, but not
    // assumed impossible either), max(0, ...) keeps the clamp range
    // valid instead of inverting.
    const int maxCameraX = std::max(0, map_.width() - kViewportWidthTiles);
    const int maxCameraY = std::max(0, map_.height() - kViewportHeightTiles);

    cameraX_ = std::clamp(desiredX, 0, maxCameraX);
    cameraY_ = std::clamp(desiredY, 0, maxCameraY);
}

sf::Vector2f Application::worldToScreen(int tileX, int tileY) const {
    return {static_cast<float>(tileX - cameraX_) * kTileSize,
            kMapTop + static_cast<float>(tileY - cameraY_) * kTileSize};
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
                    soundManager_.play(SoundEffect::Dodge);
                } else {
                    int damage = decision.attackPower;
                    const int statValue = statValueForScalingStat(
                        decision.scalingStat, actor.stats().strength, actor.stats().dexterity,
                        actor.stats().intelligence);
                    // Filler tier (cooldown 0): MonsterAttackProfile-backed
                    // attacks have no cooldown concept at all, and "every
                    // eligible turn" is the closest existing tier to that
                    // -- see MonsterAttackProfile's own comment.
                    damage += abilityDamageBonus(decision.scalingStat, statValue, /*cooldownTurns=*/0);
                    if (actor.statusEffects().has(StatusEffectType::Empowered)) {
                        damage += actor.statusEffects().magnitudeOf(StatusEffectType::Empowered);
                    }
                    // Global crit (per design: every creature, player and
                    // monster alike, rolls it) -- multiplies the result of
                    // everything above.
                    bool crit = false;
                    if (rollCrit(actor.stats().dexterity)) {
                        crit = true;
                        damage = static_cast<int>(static_cast<float>(damage) * critDamageMultiplier());
                    }
                    decision.target->stats().hp -= damage;
                    log(actor.name(), crit ? " critically hits " : " hits ", decision.target->name(),
                        " for ", damage, " (", decision.target->stats().hp, "/",
                        decision.target->stats().maxHp, " hp left)");
                    soundManager_.play(SoundEffect::Hit);
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

        case AIActionType::Summon: {
            // The only action type that creates a new Monster rather
            // than acting on an existing one -- LichBehavior only
            // decided to do this (and picked movePosition), everything
            // about actually bringing the monster into existence
            // happens here, the same "AIBehavior decides, Application
            // executes" split every other action type already follows.
            // Safe to push into monsters_/scheduler_ mid-turn-
            // processing: monsters_ is vector<unique_ptr<Monster>>, so
            // the underlying Monster objects' addresses never move even
            // if the vector itself reallocates, and
            // processMonsterTurns()'s own loop only ever holds
            // currentActor_ (a single Actor*), never an iterator into
            // monsters_, so nothing here can invalidate what that loop
            // is holding onto.
            if (map_.isWalkable(decision.movePosition.x, decision.movePosition.y) &&
                !isOccupied(decision.movePosition, &actor)) {
                std::unique_ptr<Monster> summoned =
                    createMonster(decision.summonType, decision.movePosition, decision.summonTier);
                summoned->setRewardsEligible(false);
                scheduler_.add(*summoned);
                monsters_.push_back(std::move(summoned));
            }
            if (decision.abilityIndex < actor.talents().knownTalents().size()) {
                actor.talents().startCooldown(decision.abilityIndex);
            }
            break;
        }

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

    soundManager_.play(SoundEffect::Death);

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

    auto* defeated = dynamic_cast<Monster*>(&actor);
    if (defeated && !defeated->claimDeath()) return;
    if (defeated) rewardMonster(*defeated, &actor == boss_);

    if (&actor == boss_) {
        boss_ = nullptr; // must clear before removeDeadMonsters() erases the underlying object
        scheduler_.remove(actor);
        grantXpAndAnnounce(actor.xpReward());
        if (currentFloor_ == kFinalFloor) {
            log(actor.name(), " falls! Victory is yours!");
            defeatedBossName_ = actor.name(); // renderGameOver() needs this after actor is gone
            // Deliberately NOT setting mode_ = GameOver here directly --
            // the grantXpAndAnnounce() call just above already ran the
            // full level-up sequence internally, and may have left
            // mode_ on AttributeAllocation or AbilityChoice if this
            // kill's XP crossed a level-up threshold with its own
            // pending choice. pendingFinalVictory_ just records that a
            // victory is waiting; resumeLevelUpSequence() itself (called
            // again by the AttributeAllocation/AbilityChoice key
            // handling once each pause resolves, or already reached its
            // own "nothing left pending" point just now if this kill
            // triggered no pause at all) is what actually makes the
            // GameOver transition, once every earned choice has
            // genuinely been offered.
            pendingFinalVictory_ = true;
            if (mode_ == GameMode::Playing) {
                // Nothing was left pending by the call above -- the
                // sequence already reached its own end without this
                // flag existing yet, so trigger the transition directly
                // rather than waiting for a key handler that will never
                // fire.
                mode_ = GameMode::GameOver;
                wonGame_ = true;
                pendingFinalVictory_ = false;
            }
        } else {
            // Any earlier boss floor (currently just kFirstBossFloor) --
            // defeating it doesn't end the run at all. The door
            // occupying this same position (see regenerateLevel()) is
            // now reachable, since the boss no longer blocks it; play
            // continues normally until the player chooses to step
            // through.
            log(actor.name(), " falls! The way forward opens.");
        }
        return;
    }

    log(actor.name(), " dies!");
    grantXpAndAnnounce(actor.xpReward());
    scheduler_.remove(actor);
    // Actual erase from monsters_ happens in removeDeadMonsters(), after
    // the current processMonsterTurns() loop finishes -- never mid-loop,
    // to avoid invalidating pointers still in use this turn.
}

void Application::grantXpAndAnnounce(int amount) {
    if (amount <= 0) return; // summons must not disturb an already pending level-up sequence
    // Captures level before/after specifically to detect a level-up
    // and announce it -- grantXp() itself is a plain void function
    // (data + logic, no logging; see PlayerLeveling.hpp), so this is
    // the one place XP gain actually becomes visible to the player. A
    // single large grant (the boss's 200 XP, well above any individual
    // level's threshold) can cross more than one level at once --
    // grantXp() loops internally to handle that, and this still only
    // prints one summary line, not one per level crossed.
    const int levelBefore = player_.level();
    grantXp(player_, amount);
    log("Gained ", amount, " XP.");
    if (player_.level() > levelBefore) {
        log("Level up! You are now level ", player_.level(), ".");
        soundManager_.play(SoundEffect::LevelUp);
    }

    pendingLevelUpFromLevel_ = levelBefore + 1;
    resumeLevelUpSequence();
}

void Application::resumeLevelUpSequence() {
    // Attribute allocation always resolves first, before any talent
    // unlocks or hybrid choices for the levels just gained -- a
    // deliberate ordering choice (see ARCHITECTURE_DECISIONS.md), not
    // an arbitrary one: it keeps every pause for a single XP grant in
    // one predictable sequence rather than interleaving two different
    // kinds of choice level-by-level.
    if (player_.unspentAttributePoints() > 0) {
        mode_ = GameMode::AttributeAllocation;
        return; // paused; the AttributeAllocation key handling calls this again once resolved
    }
    processLevelUpEffects(pendingLevelUpFromLevel_);

    // The sequence has now genuinely reached its own end -- either
    // nothing was ever pending, or every attribute point and hybrid
    // choice this XP grant produced has actually been offered and
    // resolved. Only now is it safe to make the deferred final-boss
    // victory transition (see checkAndHandleDeath) -- doing it any
    // earlier risked silently skipping a choice the player had genuinely
    // earned.
    if (mode_ == GameMode::Playing && pendingFinalVictory_) {
        pendingFinalVictory_ = false;
        mode_ = GameMode::GameOver;
        wonGame_ = true;
    }
}

void Application::processLevelUpEffects(int fromLevel) {
    // Prompt 23/24: checks every level actually crossed, not just the
    // final level reached -- a multi-level jump (the boss's XP against
    // an early character) must not skip a talent unlock or a hybrid
    // choice sitting at an intermediate level just because the grant
    // blew past it in one step.
    for (int level = fromLevel; level <= player_.level(); ++level) {
        if (const std::optional<Talent> unlocked = talentUnlockedAtLevel(playerClass_, level);
            unlocked.has_value()) {
            player_.talents().learnTalent(*unlocked);
            log("New talent unlocked: ", unlocked->name, "!");
        }

        offerHybridChoiceIfEligible(level);
        if (mode_ == GameMode::AbilityChoice) {
            // Paused for a real decision -- stop here. pendingLevelUpFromLevel_
            // is updated so resumeLevelUpSequence() (called by the
            // AbilityChoice key handling once the person responds)
            // continues from the *next* level, not from the start of
            // this call -- any further level in this same grant still
            // gets checked rather than silently skipped.
            pendingLevelUpFromLevel_ = level + 1;
            return;
        }
    }
}

void Application::offerHybridChoiceIfEligible(int level) {
    if (!isHybridEligible(playerClass_)) {
        return; // Thief/Spellblade -- no hybrid path at all, see HybridSpec.hpp
    }

    if (level == 5 && !player_.hybridSpecced()) {
        // The one-time spec-in decision -- level 5 is crossed exactly
        // once per character (levels only ever go up), so there's no
        // need for a separate "already declined" flag: if they decline
        // here, hybridSpecced() stays false and level 5 simply never
        // comes around again for this character.
        pendingHybridChoices_ = availableHybridPicks(playerClass_, player_.talents());
        pendingHybridChoiceIsSpecIn_ = true;
        pendingHybridChoiceLevel_ = level;
        mode_ = GameMode::AbilityChoice;
        return;
    }

    if (level >= 6 && level <= 10 && player_.hybridSpecced()) {
        const std::vector<Talent> available = availableHybridPicks(playerClass_, player_.talents());
        if (available.empty()) {
            return; // the whole opposing kit has already been picked -- nothing left to offer
        }
        pendingHybridChoices_ = available;
        pendingHybridChoiceIsSpecIn_ = false;
        pendingHybridChoiceLevel_ = level;
        mode_ = GameMode::AbilityChoice;
    }
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

void Application::saveGame() {
    SaveGameState state;
    state.map = map_;
    state.exploredMap = exploredMap_;
    state.playerPosition = player_.position();
    state.playerClass = playerClass_;
    state.playerLevel = player_.level();
    state.playerXp = player_.xp();
    state.currentFloor = currentFloor_;
    state.playerStats = player_.baseStats();
    state.playerStats.hp = player_.stats().hp;
    state.playerStats.mana = player_.stats().mana;
    state.unspentAttributePoints = player_.unspentAttributePoints();
    state.nextItemId = nextItemId_;
    state.lootRngState = loot_.state();
    state.chestPosition = chestPosition_; state.chestExists = chestExists_; state.chestClaimed = chestClaimed_;
    state.ordinaryDrops = ordinaryDrops_;
    const auto saveItem = [&](const Item& item, int location) {
        state.items.push_back({item.definition()->id, item.instanceId(), location,
                              location == -2 ? item.position() : Position{}, item.rollTier(), item.affixes()});
    };
    for (const auto& item : groundItems_) saveItem(*item, -2);
    for (const auto& item : player_.inventory().items()) saveItem(*item, -1);
    for (int slot = 0; slot < 3; ++slot) {
        if (const auto* item = player_.inventory().equipped(static_cast<EquipmentSlot>(slot)))
            saveItem(*item, slot);
    }
    state.lastMoveDirection = lastMoveDirection_;

    const auto& talents = player_.talents().knownTalents();
    for (std::size_t i = 0; i < talents.size(); ++i)
        state.playerTalents.push_back({talents[i].id, player_.talents().cooldownRemaining(i)});
    state.runes = player_.talents().runes();
    state.runeChoiceAvailable = runeChoiceAvailable_;
    state.playerStatusEffects = player_.statusEffects().active();
    state.playerHybridSpecced = player_.hybridSpecced();

    for (auto& m : monsters_) {
        SaveGameState::MonsterSaveData data;
        data.type = m->type();
        data.position = m->position();
        data.hp = m->stats().hp;
        data.maxHp = m->stats().maxHp;
        data.isBoss = (m.get() == boss_);
        data.tier = m->tier();
        data.rewardsEligible = m->rewardsEligible();
        data.statusEffects = m->statusEffects().active();
        const auto& known = m->talents().knownTalents();
        for (std::size_t i = 0; i < known.size(); ++i)
            data.talents.push_back({known[i].id, m->talents().cooldownRemaining(i)});
        state.monsters.push_back(std::move(data));
    }

    if (engine::saveGame(state, kSaveFilePath)) {
        log("Game saved.");
    } else {
        log("Failed to save game (could not write ", kSaveFilePath, ").");
    }
}

void Application::loadGame() {
    runesOpen_ = false;
    inventoryOpen_ = false;
    inventorySelection_ = 0;
    cancelTargeting();
    mousePixel_.reset();
    talentPage_ = 0;
    const std::optional<SaveGameState> loaded = engine::loadGame(kSaveFilePath);
    if (!loaded.has_value()) {
        log("No valid save file found (", kSaveFilePath, ").");
        return;
    }
    const SaveGameState& state = *loaded;
    auto catalog = fullKitForClass(state.playerClass);
    if (isHybridEligible(state.playerClass)) {
        const auto hybrid = fullKitForClass(hybridPoolClass(state.playerClass));
        catalog.insert(catalog.end(), hybrid.begin(), hybrid.end());
    }
    TalentSet restoredTalents;
    for (const auto& saved : state.playerTalents) {
        const auto found = std::find_if(catalog.begin(), catalog.end(), [&](const auto& talent) { return talent.id == saved.id; });
        if (found == catalog.end()) { log("Save contains an unknown player talent."); return; }
        restoredTalents.learnTalent(*found);
        restoredTalents.setCooldownRemaining(restoredTalents.knownTalents().size()-1, saved.cooldown);
    }
    if (restoredTalents.empty()) { log("Save contains no player talents."); return; }
    for (const auto& rune : state.runes) {
        if (!rune.talentId.empty()) {
            const auto& known = restoredTalents.knownTalents();
            const auto found = std::find_if(known.begin(), known.end(), [&](const auto& talent) { return talent.id == rune.talentId; });
            if (found == known.end() || !runeUnavailableReason(*found, rune.definitionId).empty()) {
                log("Save contains an incompatible rune attachment."); return;
            }
        }
    }
    restoredTalents.runes() = state.runes;
    std::vector<std::unique_ptr<Monster>> restoredMonsters;
    Monster* restoredBoss = nullptr;
    for (const auto& savedMonster : state.monsters) {
        auto monster = createMonster(savedMonster.type, savedMonster.position, savedMonster.tier);
        const auto& known = monster->talents().knownTalents();
        if (savedMonster.talents.size() != known.size()) { log("Save contains an incomplete enemy talent kit."); return; }
        for (const auto& saved : savedMonster.talents) {
            const auto found = std::find_if(known.begin(), known.end(), [&](const auto& talent) { return talent.id == saved.id; });
            if (found == known.end()) { log("Save contains an unknown enemy talent."); return; }
            monster->talents().setCooldownRemaining(static_cast<std::size_t>(found-known.begin()), saved.cooldown);
        }
        monster->setRewardsEligible(savedMonster.rewardsEligible);
        monster->stats().hp = savedMonster.hp;
        monster->stats().maxHp = savedMonster.maxHp;
        monster->statusEffects().active() = savedMonster.statusEffects;
        if (savedMonster.isBoss) {
            if (restoredBoss) { log("Save contains multiple bosses."); return; }
            restoredBoss = monster.get();
        }
        restoredMonsters.push_back(std::move(monster));
    }



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
    player_.talents() = std::move(restoredTalents);
    runeChoiceAvailable_ = state.runeChoiceAvailable;
    player_.level() = state.playerLevel;
    player_.xp() = state.playerXp;
    currentFloor_ = state.currentFloor;
    player_.inventory() = Inventory{};
    groundItems_.clear();
    nextItemId_ = state.nextItemId;
    loot_.restore(state.lootRngState);
    chestPosition_ = state.chestPosition; chestExists_ = state.chestExists; chestClaimed_ = state.chestClaimed;
    ordinaryDrops_ = state.ordinaryDrops;
    player_.baseStats() = state.playerStats;
    player_.stats() = state.playerStats;
    player_.unspentAttributePoints() = state.unspentAttributePoints;
    pendingFinalVictory_ = false;
    pendingHybridChoices_.clear();
    pendingLevelUpFromLevel_ = player_.level() + 1;
    // Validation in loadGame guarantees unique IDs, known definitions and slot compatibility.
    for (const auto& savedItem : state.items) {
        auto item = std::make_unique<Item>(*findItemDefinition(savedItem.definitionId),
                                          savedItem.instanceId, savedItem.position, savedItem.affixes, savedItem.rollTier);
        if (savedItem.location == -2) groundItems_.push_back(std::move(item));
        else {
            player_.inventory().add(std::move(item));
            if (savedItem.location >= 0)
                player_.inventory().equip(player_.inventory().items().size() - 1);
        }
    }
    player_.refreshEquipmentStats();
    lastMoveDirection_ = state.lastMoveDirection;

    player_.hybridSpecced() = state.playerHybridSpecced;
    player_.statusEffects().active() = state.playerStatusEffects;

    boss_ = restoredBoss;
    monsters_ = std::move(restoredMonsters);

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

    if (player_.unspentAttributePoints() > 0) resumeLevelUpSequence();
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

    drawText("1. Fighter", 60.f, 130.f, 20, sf::Color(230, 120, 90));
    drawText("Pure Strength. A free spammable basic attack, hp as the", 80.f, 158.f, 14,
             sf::Color(190, 190, 190));
    drawText("resource that matters, and the hardest single hit of any", 80.f, 176.f, 14,
             sf::Color(190, 190, 190));
    drawText("base class.", 80.f, 194.f, 14, sf::Color(190, 190, 190));

    drawText("2. Sorcerer", 60.f, 230.f, 20, sf::Color(170, 120, 230));
    drawText("Pure Intelligence. The largest mana pool of any class and", 80.f, 258.f, 14,
             sf::Color(190, 190, 190));
    drawText("the lowest hp -- a true glass cannon. Mind Shatter can", 80.f, 276.f, 14,
             sf::Color(190, 190, 190));
    drawText("stun an enemy, turning a status effect only monsters", 80.f, 294.f, 14,
             sf::Color(190, 190, 190));
    drawText("could inflict before now back around on them.", 80.f, 312.f, 14,
             sf::Color(190, 190, 190));

    drawText("3. Thief", 60.f, 348.f, 20, sf::Color(120, 230, 140));
    drawText("Pure Dexterity. Low hp, but the highest possible dodge", 80.f, 376.f, 14,
             sf::Color(190, 190, 190));
    drawText("chance -- survives by not getting hit at all. Vault Kick", 80.f, 394.f, 14,
             sf::Color(190, 190, 190));
    drawText("lets you strike an adjacent enemy and leap back out of", 80.f, 412.f, 14,
             sf::Color(190, 190, 190));
    drawText("melee range in the same motion.", 80.f, 430.f, 14, sf::Color(190, 190, 190));

    drawText("Press 1, 2 or 3 to begin.", 60.f, 478.f, 16, sf::Color(150, 150, 150));
}

void Application::renderGameOver() {
    if (wonGame_) {
        drawText("Victory!", 60.f, 60.f, 32, sf::Color(255, 215, 0));
        drawText("You have slain the " + defeatedBossName_ + ".", 60.f, 120.f, 18,
                  sf::Color(210, 210, 210));
    } else {
        drawText("You Died", 60.f, 60.f, 32, sf::Color(200, 50, 50));
        drawText("The dungeon claims another.", 60.f, 120.f, 18, sf::Color(210, 210, 210));
    }
    drawText("Press Enter to return to class selection.", 60.f, 180.f, 16,
              sf::Color(150, 150, 150));
}

void Application::renderAbilityChoice() {
    const PlayerClass poolClass = hybridPoolClass(playerClass_);
    const char* poolClassName = poolClass == PlayerClass::Mage ? "Mage" : "Warrior";

    if (pendingHybridChoiceIsSpecIn_) {
        drawText("A new path opens...", 60.f, 60.f, 28, sf::Color(230, 230, 230));
        drawText("You've grown strong enough to begin drawing on a second discipline.",
                 60.f, 104.f, 15, sf::Color(190, 190, 190));
        std::ostringstream oss;
        oss << "Spec into the hybrid path? Pick a " << poolClassName
            << " ability now, and one more every level from here on.";
        drawText(oss.str(), 60.f, 124.f, 15, sf::Color(190, 190, 190));
    } else {
        drawText("Choose your next ability", 60.f, 60.f, 28, sf::Color(230, 230, 230));
        std::ostringstream oss;
        oss << "Pick one more " << poolClassName << " ability to add to your kit.";
        drawText(oss.str(), 60.f, 104.f, 15, sf::Color(190, 190, 190));
    }

    float y = 170.f;
    for (std::size_t i = 0; i < pendingHybridChoices_.size(); ++i) {
        const Talent& talent = pendingHybridChoices_[i];
        std::ostringstream label;
        label << (i + 1) << ". " << talent.name;
        drawText(label.str(), 60.f, y, 18, sf::Color(120, 200, 230));
        drawText(talent.description, 80.f, y + 24.f, 14, sf::Color(180, 180, 180));
        y += 60.f;
    }

    if (pendingHybridChoiceIsSpecIn_) {
        drawText("0. No thanks -- stay on your current path", 60.f, y + 10.f, 16,
                 sf::Color(150, 150, 150));
    }
}

void Application::renderAttributeAllocation() {
    drawText("Level up!", 60.f, 60.f, 28, sf::Color(230, 230, 230));

    std::ostringstream subtitle;
    subtitle << "You have " << player_.unspentAttributePoints()
              << (player_.unspentAttributePoints() == 1 ? " point" : " points")
              << " to spend. Choose one:";
    drawText(subtitle.str(), 60.f, 104.f, 16, sf::Color(190, 190, 190));

    const Stats& stats = player_.stats();

    std::ostringstream strLine;
    strLine << "1. Strength (currently " << stats.strength << ")";
    drawText(strLine.str(), 60.f, 160.f, 18, sf::Color(230, 140, 100));
    drawText("+1 Max HP. Scales Strength-based abilities.", 80.f, 184.f, 14,
             sf::Color(180, 180, 180));

    std::ostringstream dexLine;
    dexLine << "2. Dexterity (currently " << stats.dexterity << ")";
    drawText(dexLine.str(), 60.f, 224.f, 18, sf::Color(120, 220, 140));
    drawText("+0.5% Dodge (cap 25%), +0.5% Crit. Scales Dexterity-based abilities.", 80.f,
             248.f, 14, sf::Color(180, 180, 180));

    std::ostringstream intLine;
    intLine << "3. Intelligence (currently " << stats.intelligence << ")";
    drawText(intLine.str(), 60.f, 288.f, 18, sf::Color(140, 170, 230));
    drawText("+1 Max Mana. Scales Intelligence-based abilities.", 80.f, 312.f, 14,
             sf::Color(180, 180, 180));
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

    if (mode_ == GameMode::AbilityChoice) {
        renderAbilityChoice();
        window_.display();
        return;
    }

    if (mode_ == GameMode::AttributeAllocation) {
        renderAttributeAllocation();
        window_.display();
        return;
    }

    updateCamera();

    // Only the camera-visible range, not the whole map -- both a real
    // performance win now that the map (60x32, Prompt 18) is bigger
    // than the ~40x22-tile viewport, and it naturally avoids needing a
    // separate "is this tile on screen" check per tile. +1 on each
    // upper bound covers the partially-visible tile at the viewport's
    // trailing edge.
    const int viewStartX = cameraX_;
    const int viewEndX = std::min(map_.width(), cameraX_ + static_cast<int>(kMapWidth / kTileSize));
    const int viewStartY = cameraY_;
    const int viewEndY = std::min(map_.height(), cameraY_ + static_cast<int>(kMapHeight / kTileSize));

    for (int y = viewStartY; y < viewEndY; ++y) {
        for (int x = viewStartX; x < viewEndX; ++x) {
            const Visibility vis = exploredMap_.at(x, y);
            if (vis == Visibility::Hidden) {
                continue;
            }

            const TileType tileType = map_.tileAt(x, y).type;
            sf::Color baseColor;
            if (tileType == TileType::Wall) {
                baseColor = sf::Color(45, 45, 52);
            } else if (tileType == TileType::Door) {
                baseColor = sf::Color(180, 40, 40); // red -- the floor-transition door
            } else {
                baseColor = sf::Color(90, 90, 100);
            }

            sf::RectangleShape tileShape({kTileSize - 1.f, kTileSize - 1.f});
            tileShape.setPosition(worldToScreen(x, y));
            tileShape.setFillColor(vis == Visibility::Visible ? baseColor : dim(baseColor));
            window_.draw(tileShape);
        }
    }

    renderGroundItems();
    for (auto& m : monsters_) {
        if (m->stats().hp <= 0) {
            continue;
        }
        if (exploredMap_.at(m->position().x, m->position().y) != Visibility::Visible) {
            continue; // only draw what the player can currently see -- see Prompt 7 notes
        }

        const sf::Vector2f screenPos = worldToScreen(m->position().x, m->position().y);
        if (screenPos.x < 0.f || screenPos.x >= kMapWidth ||
            screenPos.y < kMapTop || screenPos.y >= kMapTop + kMapHeight) continue;

        // Elite/Nightmare border (Prompt 22): a slightly larger square
        // drawn first, in the tier's color, so the monster's own type-
        // colored tile (drawn on top, its normal size) reads as sitting
        // inside a visible outline. Base tier returns nullopt -- nothing
        // extra drawn, looks exactly as it always has.
        if (const std::optional<sf::Color> borderColor = tierBorderColor(m->tier());
            borderColor.has_value()) {
            constexpr float kBorderThickness = 3.f;
            sf::RectangleShape border(
                {kTileSize - 1.f + kBorderThickness * 2.f, kTileSize - 1.f + kBorderThickness * 2.f});
            border.setPosition({screenPos.x - kBorderThickness, screenPos.y - kBorderThickness});
            border.setFillColor(*borderColor);
            window_.draw(border);
        }

        sf::RectangleShape monsterShape({kTileSize - 1.f, kTileSize - 1.f});
        monsterShape.setPosition(screenPos);
        monsterShape.setFillColor(monsterColor(m->type()));
        window_.draw(monsterShape);

        const float hpFraction =
            static_cast<float>(m->stats().hp) / static_cast<float>(m->stats().maxHp);
        sf::RectangleShape hpBack({kTileSize - 1.f, 4.f});
        hpBack.setPosition({screenPos.x, screenPos.y - 6.f});
        hpBack.setFillColor(sf::Color(40, 20, 20));
        window_.draw(hpBack);

        sf::RectangleShape hpFront({(kTileSize - 1.f) * hpFraction, 4.f});
        hpFront.setPosition({screenPos.x, screenPos.y - 6.f});
        hpFront.setFillColor(sf::Color(220, 60, 60));
        window_.draw(hpFront);
    }

    sf::RectangleShape playerShape({kTileSize - 1.f, kTileSize - 1.f});
    playerShape.setPosition(worldToScreen(player_.position().x, player_.position().y));
    playerShape.setFillColor(sf::Color(240, 200, 60));
    window_.draw(playerShape);

    renderTargetingOverlay();

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

    // Level/XP -- Prompt 20. "MAX" instead of a fraction once level 10
    // is reached, since grantXp() zeroes xp() there and "X/0" would
    // read as a bug, not a deliberate cap.
    {
        std::ostringstream oss;
        oss << "Level " << player_.level();
        if (player_.level() < 10) {
            oss << "  (" << player_.xp() << '/' << xpForNextLevel(player_.level()) << " XP)";
        } else {
            oss << "  (MAX)";
        }
        drawText(oss.str(), 10.f, 46.f, 13, sf::Color(200, 200, 160));
    }

    // Floor -- the multi-floor dungeon progression. Small and
    // unobtrusive, same styling as the Level line above it, just one
    // more fact about where the character currently stands.
    {
        std::ostringstream oss;
        oss << "Floor " << currentFloor_ << " / " << kFinalFloor;
        drawText(oss.str(), 290.f, 46.f, 13, sf::Color(180, 180, 200));
    }

    renderTargetingPanel();

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
        const float bossX = 370.f;

        const float bossHpFraction =
            static_cast<float>(boss_->stats().hp) / static_cast<float>(boss_->stats().maxHp);
        sf::RectangleShape bossBack({kBossBarWidth, kBossBarHeight});
        bossBack.setPosition({bossX, 27.f});
        bossBack.setFillColor(sf::Color(35, 30, 10));
        window_.draw(bossBack);
        sf::RectangleShape bossFront({kBossBarWidth * bossHpFraction, kBossBarHeight});
        bossFront.setPosition({bossX, 27.f});
        bossFront.setFillColor(sf::Color(255, 215, 0));
        window_.draw(bossFront);
        drawText(boss_->name(), bossX, 5.f, 14, sf::Color(255, 215, 0));
    }

    if (inventoryOpen_) renderInventory();
    if (runesOpen_) renderRunes();
    window_.display();
}

} // namespace engine
