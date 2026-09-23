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
#include "entities/HybridSpec.hpp"
#include "entities/PlayerClassFactory.hpp"
#include "entities/PlayerLeveling.hpp"
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
    constexpr int kViewportWidthTiles = static_cast<int>(kWindowWidth / kTileSize);
    constexpr int kViewportHeightTiles = static_cast<int>(kWindowHeight / kTileSize);

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
            static_cast<float>(tileY - cameraY_) * kTileSize};
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
                // applies yet. As of Prompt 19: only the 3 base classes
                // are offered here -- Spellblade still exists in
                // PlayerClassFactory (see PlayerClass.hpp), just not as
                // a starting option anymore.
                if (keyPressed->code == sf::Keyboard::Key::Num1) {
                    selectClass(PlayerClass::Fighter);
                } else if (keyPressed->code == sf::Keyboard::Key::Num2) {
                    selectClass(PlayerClass::Sorcerer);
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
                static constexpr std::array<sf::Keyboard::Key, 6> kChoiceKeys{
                    sf::Keyboard::Key::Num1, sf::Keyboard::Key::Num2, sf::Keyboard::Key::Num3,
                    sf::Keyboard::Key::Num4, sf::Keyboard::Key::Num5, sf::Keyboard::Key::Num6,
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
                    const int resumeFrom = pendingHybridChoiceLevel_ + 1;
                    mode_ = GameMode::Playing;
                    processLevelUpEffects(resumeFrom);
                } else if (pendingHybridChoiceIsSpecIn_ &&
                           keyPressed->code == sf::Keyboard::Key::Num0) {
                    log("You decide to stay on your current path.");
                    soundManager_.play(SoundEffect::Select);
                    const int resumeFrom = pendingHybridChoiceLevel_ + 1;
                    mode_ = GameMode::Playing;
                    processLevelUpEffects(resumeFrom);
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
                    soundManager_.play(SoundEffect::Dodge);
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

    if (&actor == boss_) {
        log(actor.name(), " falls! You have slain the Goblin Warlord!");
        boss_ = nullptr; // must clear before removeDeadMonsters() erases the underlying object
        scheduler_.remove(actor);
        grantXpAndAnnounce(actor.xpReward());
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
    grantXpAndAnnounce(actor.xpReward());
    scheduler_.remove(actor);
    // Actual erase from monsters_ happens in removeDeadMonsters(), after
    // the current processMonsterTurns() loop finishes -- never mid-loop,
    // to avoid invalidating pointers still in use this turn.
}

void Application::grantXpAndAnnounce(int amount) {
    // Prompt 20: captures level before/after specifically to detect a
    // level-up and announce it -- grantXp() itself is a plain void
    // function (data + logic, no logging; see PlayerLeveling.hpp), so
    // this is the one place XP gain actually becomes visible to the
    // player. A single large grant (the boss's 200 XP, well above any
    // individual level's threshold) can cross more than one level at
    // once -- grantXp() loops internally to handle that, and this still
    // only prints one summary line, not one per level crossed.
    const int levelBefore = player_.level();
    grantXp(player_, amount);
    log("Gained ", amount, " XP.");
    if (player_.level() > levelBefore) {
        log("Level up! You are now level ", player_.level(), ".");
        soundManager_.play(SoundEffect::LevelUp);
    }

    processLevelUpEffects(levelBefore + 1);
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
            // Paused for a real decision -- stop here. The AbilityChoice
            // key handling resumes this same loop (from level + 1) once
            // the person responds, so any further level in this same
            // grant still gets checked rather than silently skipped.
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
    soundManager_.play(SoundEffect::Select);
    playerClass_ = cls;
    player_.stats() = statsForClass(cls);
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
        // above -- see createMonster()'s GoblinWarlord case for why.
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
    state.playerLevel = player_.level();
    state.playerXp = player_.xp();
    state.playerStats = player_.stats();
    state.lastMoveDirection = lastMoveDirection_;

    const std::vector<Talent>& talents = player_.talents().knownTalents();
    state.playerCooldowns.resize(talents.size());
    for (std::size_t i = 0; i < talents.size(); ++i) {
        state.playerCooldowns[i] = player_.talents().cooldownRemaining(i);
    }
    state.playerStatusEffects = player_.statusEffects().active();

    // Prompt 24: only the hybrid picks need saving explicitly -- the
    // base class's own level-4/level-7 unlocks are re-derived from
    // playerLevel on load instead (see loadGame() and
    // SaveGameState::playerHybridPickNames's own comment for why).
    state.playerHybridSpecced = player_.hybridSpecced();
    for (const Talent& picked : pickedHybridTalents(playerClass_, player_.talents())) {
        state.playerHybridPickNames.push_back(picked.name);
    }

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
    // the save was made as. Loading a Fighter save while player_ was
    // still configured as a Spellblade (or vice versa) would otherwise
    // apply a Slam's cooldown value to whatever the Spellblade's talent
    // 0 happens to be, silently wrong rather than caught by anything.
    player_.talents() = talentSetForClass(playerClass_);
    player_.level() = state.playerLevel;
    player_.xp() = state.playerXp;
    player_.stats() = state.playerStats;
    lastMoveDirection_ = state.lastMoveDirection;

    // Prompt 24 (fixing a real gap found while building it): both of
    // these must happen *before* the cooldown-restoration loop below,
    // since that loop needs player_.talents().knownTalents() to already
    // match its saved size (base kit + unlocks + hybrid picks) for the
    // positional cooldown indices to mean the right thing.
    //
    // Base-class level-4/level-7 unlocks are re-derived from the saved
    // level, not stored in the save file at all -- fully deterministic,
    // so storing them again would just be redundant state.
    if (const std::optional<Talent> levelFour = talentUnlockedAtLevel(playerClass_, 4);
        player_.level() >= 4 && levelFour.has_value()) {
        player_.talents().learnTalent(*levelFour);
    }
    if (const std::optional<Talent> levelSeven = talentUnlockedAtLevel(playerClass_, 7);
        player_.level() >= 7 && levelSeven.has_value()) {
        player_.talents().learnTalent(*levelSeven);
    }
    // Hybrid picks genuinely can't be re-derived (which specific
    // abilities were chosen is a real player decision) -- looked up by
    // name against the full hybrid pool and re-learned in the same
    // order they were saved.
    player_.hybridSpecced() = state.playerHybridSpecced;
    const std::vector<Talent> hybridPool = fullKitForClass(hybridPoolClass(playerClass_));
    for (const std::string& pickedName : state.playerHybridPickNames) {
        for (const Talent& candidate : hybridPool) {
            if (candidate.name == pickedName) {
                player_.talents().learnTalent(candidate);
                break;
            }
        }
    }

    for (std::size_t i = 0;
         i < state.playerCooldowns.size() && i < player_.talents().knownTalents().size(); ++i) {
        player_.talents().setCooldownRemaining(i, state.playerCooldowns[i]);
    }
    player_.statusEffects().active() = state.playerStatusEffects;

    boss_ = nullptr;
    monsters_.clear();
    // Prompt 22: reconstructed at the tier matching the *saved* player
    // level, not the Base default createMonster() would otherwise use.
    // Without this, a loaded Elite/Nightmare monster's hp/maxHp would
    // still be correctly overwritten below (they're explicit save
    // fields), but its name and damage output (baked into the
    // AIBehavior at construction time, not stored directly) would
    // silently revert to Base tier -- restored hp that doesn't match
    // restored damage. Not a perfect reconstruction in every case (if
    // the player leveled up mid-dungeon after these monsters were
    // originally spawned, this derives a newer tier than they actually
    // spawned at), but a consistent one: every field on the
    // reconstructed monster matches some real tier's numbers, not a
    // mismatched mix of two different tiers' data.
    const MonsterTier loadedTier = tierForLevel(state.playerLevel);
    for (const SaveGameState::MonsterSaveData& m : state.monsters) {
        std::unique_ptr<Monster> monster = createMonster(m.type, m.position, loadedTier);
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
        drawText("You have slain the Goblin Warlord.", 60.f, 120.f, 18, sf::Color(210, 210, 210));
    } else {
        drawText("You Died", 60.f, 60.f, 32, sf::Color(200, 50, 50));
        drawText("The dungeon claims another.", 60.f, 120.f, 18, sf::Color(210, 210, 210));
    }
    drawText("Press Enter to return to class selection.", 60.f, 180.f, 16,
              sf::Color(150, 150, 150));
}

void Application::renderAbilityChoice() {
    const PlayerClass poolClass = hybridPoolClass(playerClass_);
    const char* poolClassName = poolClass == PlayerClass::Sorcerer ? "Sorcerer" : "Fighter";

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

    updateCamera();

    // Only the camera-visible range, not the whole map -- both a real
    // performance win now that the map (60x32, Prompt 18) is bigger
    // than the ~40x22-tile viewport, and it naturally avoids needing a
    // separate "is this tile on screen" check per tile. +1 on each
    // upper bound covers the partially-visible tile at the viewport's
    // trailing edge.
    const int viewStartX = cameraX_;
    const int viewEndX = std::min(map_.width(), cameraX_ + static_cast<int>(kWindowWidth / kTileSize) + 1);
    const int viewStartY = cameraY_;
    const int viewEndY = std::min(map_.height(), cameraY_ + static_cast<int>(kWindowHeight / kTileSize) + 1);

    for (int y = viewStartY; y < viewEndY; ++y) {
        for (int x = viewStartX; x < viewEndX; ++x) {
            const Visibility vis = exploredMap_.at(x, y);
            if (vis == Visibility::Hidden) {
                continue;
            }

            const sf::Color baseColor = map_.tileAt(x, y).type == TileType::Wall
                                             ? sf::Color(45, 45, 52)
                                             : sf::Color(90, 90, 100);

            sf::RectangleShape tileShape({kTileSize - 1.f, kTileSize - 1.f});
            tileShape.setPosition(worldToScreen(x, y));
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

        const sf::Vector2f screenPos = worldToScreen(m->position().x, m->position().y);

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
        constexpr float kTalentListY = 72.f; // pushed down from 58.f (Prompt 20) to leave
                                              // clean room for the level/XP line above it
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
