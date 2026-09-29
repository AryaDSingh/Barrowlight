#include "entities/ArmourTalents.hpp"
#include "entities/RunProgression.hpp"
#include "entities/HiddenCombat.hpp"
#include "entities/HiddenTrees.hpp"
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
#include "ai/BossBehavior.hpp"
#include "core/PlayLayout.hpp"
#include "entities/AttributeFormulas.hpp"
#include "entities/MonsterFactory.hpp"
#include "entities/TalentProgression.hpp"
#include "entities/PlayerClassFactory.hpp"
#include "entities/PlayerLeveling.hpp"
#include "entities/StatusEffectLogic.hpp"
#include "entities/Stealth.hpp"
#include "entities/TalentEffects.hpp"
#include "world/DungeonGenerator.hpp"
#include "world/EncounterPlan.hpp"
#include "world/FloorTheme.hpp"
#include "world/FieldOfView.hpp"
#include "world/LineOfFire.hpp"

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
constexpr int kFinalFloor = kRunFinalFloor;

// Passive regen, applied once per player turn (see
// advanceTurnsUntilPlayerCanAct) regardless of what action was taken --
// without it, a fight that outlasts the player's starting mana pool
// leaves them with talents they can see are "off cooldown" but can never
// actually afford again. Modest on purpose: enough to matter over a
// multi-turn fight, not enough to make mana cost meaningless.
constexpr int kManaRegenOutsideCombat = 2;
constexpr int kManaRegenInCombat = 1;

// Relative to wherever the executable is launched from -- same
// reasoning as avoiding data/ file loading elsewhere in this project
// (see ARCHITECTURE_DECISIONS.md): resolving the executable's own
// directory needs platform-specific APIs this project has deliberately
// avoided needing so far.
constexpr const char* kSaveFilePath = "savegame.txt";

// Same relative-path reasoning as kSaveFilePath.
constexpr const char* kFontPath = "assets/fonts/DejaVuSansMono.ttf";
const sf::FloatRect kClassCards[]{{{50,155},{920,90}},{{50,265},{920,90}},{{50,375},{920,90}}};
const sf::FloatRect kAttributeChoices[]{{{50,150},{920,58}},{{50,218},{920,58}},{{50,286},{920,58}}};
const sf::FloatRect kDeathModeButton{{650,580},{550,42}}, kReviveButton{{60,245},{500,48}};
const sf::FloatRect kRestartButton{{60,175},{410,48}}, kEndCodexButton{{60,370},{300,44}},
    kStartLoadButton{{60,580},{260,42}}, kStartCodexButton{{340,580},{260,42}};

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
        case MonsterType::GoblinRaider: return sf::Color(235,125,70);
        case MonsterType::SkeletonArcher: return sf::Color(135,210,245);
        case MonsterType::SkeletonGuard: return sf::Color(170,185,205);
        case MonsterType::Bonecaller: return sf::Color(180,125,220);
        case MonsterType::GoblinCaptain: return sf::Color(255,185,60);
        case MonsterType::OssuaryWarden: return sf::Color(255,185,60);
        case MonsterType::GoblinBulwark: case MonsterType::CryptSentinel: return sf::Color(110,150,195);
        case MonsterType::GoblinMedic: case MonsterType::GraveMender: return sf::Color(95,230,140);
        case MonsterType::GoblinStalker: case MonsterType::CryptShade: return sf::Color(180,110,200);
        case MonsterType::GoblinSlinger: return sf::Color(225,155,95);
        case MonsterType::FrostAcolyte: return sf::Color(120,220,250);
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
              TalentSet({basicAttack(),basicCleanse()})) {
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

    if (!codex_.load("codex.txt")) log(codex_.error());
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
    while (const auto event=window_.pollEvent()) handleEvent(*event);
}

void Application::handleEvent(const sf::Event& input) {
    const auto* event=&input;
    if (event->is<sf::Event::Closed>()) {
        window_.close();
    }
    if(const auto* moved=event->getIf<sf::Event::MouseMoved>()) mousePixel_=moved->position;

    if (autoExploring_ && (event->is<sf::Event::KeyPressed>() ||
        event->is<sf::Event::MouseButtonPressed>() || event->is<sf::Event::FocusLost>())) {
        stopAutoExplore("interrupted by input or focus change.");
        return;
    }
    if (codexOpen_) {
        if (const auto* key=event->getIf<sf::Event::KeyPressed>()) handleCodexKey(key->code);
        else handleCodexMouse(*event);
        return;
    }
    if (const auto* click=event->getIf<sf::Event::MouseButtonPressed>(); click && click->button==sf::Mouse::Button::Left) {
        const auto point=sf::Vector2f(click->position);
        if(mode_==GameMode::GameOver) {
            if(kReviveButton.contains(point)) { reviveInTown(); return; }
            if(kRestartButton.contains(point)) mode_=GameMode::ClassSelection;
            else if(kEndCodexButton.contains(point)) codexOpen_=true;
            return;
        }
        if(mode_==GameMode::ClassSelection) {
            if(kDeathModeButton.contains(point)) { adventureMode_=!adventureMode_; return; }
            if(kStartLoadButton.contains(point)) { loadGame(); return; }
            if(kStartCodexButton.contains(point)) { codexOpen_=true; return; }
            for(std::size_t i=0;i<3;++i) if(kClassCards[i].contains(point)) {
                selectClass(i==0?PlayerClass::Warrior:i==1?PlayerClass::Mage:PlayerClass::Thief);
                break;
            }
            return;
        }
        if(mode_==GameMode::AttributeAllocation) {
            for(std::size_t i=0;i<3;++i) if(kAttributeChoices[i].contains(point)) {
                allocateAttribute(static_cast<unsigned int>(i));
                break;
            }
            return;
        }
    }
    if (const auto* key=event->getIf<sf::Event::KeyPressed>(); key && key->code==sf::Keyboard::Key::J) {
        restTurns_=0; cancelTargeting(); codexOpen_=true; return;
    }

    if (event->is<sf::Event::KeyPressed>() || event->is<sf::Event::MouseButtonPressed>()) {
        if (restTurns_>0) { restTurns_=0; return; }
    }
    if (inventoryOpen_ && !event->is<sf::Event::KeyPressed>()) {
        handleInventoryMouse(*event);
        return; // inventory mouse actions must never click through onto the map
    }
    if(mode_==GameMode::Town && !event->is<sf::Event::KeyPressed>()) {
        handleTownMouse(*event);
        return;
    }
    if (mode_ == GameMode::AbilityChoice && !event->is<sf::Event::KeyPressed>()) {
        handleTreeMouse(*event);
        return; // closing a menu must not also activate the map underneath
    }
    if(exitMenu_ && !event->is<sf::Event::KeyPressed>()) { handleTravelMouse(*event); return; }
    if(vaultMenu_ && !event->is<sf::Event::KeyPressed>()) { handleVaultMouse(*event); return; }
    if (mode_ == GameMode::Playing && !inventoryOpen_ && !vaultMenu_ && !exitMenu_) handleTargetingMouse(*event);

    if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
        if (mode_==GameMode::Town) { handleTownKey(keyPressed->code); return; }
        if (exitMenu_) {
            handleTravelKey(keyPressed->code);
            return;
        }
        if (vaultMenu_) { handleVaultKey(keyPressed->code); return; }
        if (keyPressed->code == sf::Keyboard::Key::Escape) {
            if (mode_ == GameMode::AbilityChoice) { handleTreeKey(keyPressed->code,keyPressed->shift); return; }
            if (inventoryOpen_) {
                inventoryOpen_ = false;
                return;
            }
            if (mode_ == GameMode::Playing && (aimingTalent_ || inspecting_)) {
                cancelTargeting();
                return;
            }
            window_.close();
            return;
        }

        if (keyPressed->code == sf::Keyboard::Key::F9) {
            // Reachable from either mode -- a person who wants to
            // continue a previous run shouldn't have to pick a class
            // first just to reach a point where loading is possible.
            // loadGame() determines the actual class from the save
            // file itself and switches mode_ to Playing on success.
            loadGame();
            return;
        }

        if (mode_ == GameMode::ClassSelection) {
            // Deliberately only these keys handled here -- this
            // screen doesn't need arrow keys, talent keys, or save,
            // so nothing else in the Playing-mode switch below even
            // applies yet. As of Prompt 19: only the 3 base classes
            // are offered here -- Spellblade still exists in
            // PlayerClassFactory (see PlayerClass.hpp), just not as
            // a starting option anymore.
            if (keyPressed->code == sf::Keyboard::Key::M) { adventureMode_=!adventureMode_; return; }
            if (keyPressed->code == sf::Keyboard::Key::Num1) {
                selectClass(PlayerClass::Warrior);
            } else if (keyPressed->code == sf::Keyboard::Key::Num2) {
                selectClass(PlayerClass::Mage);
            } else if (keyPressed->code == sf::Keyboard::Key::Num3) {
                selectClass(PlayerClass::Thief);
            }
            return;
        }

        if (mode_ == GameMode::GameOver) {
            if (keyPressed->code == sf::Keyboard::Key::R) { reviveInTown(); return; }
            // selectClass() (reached via the ClassSelection screen
            // this leads back to) does the actual reset -- this
            // mode transition alone doesn't need to touch
            // map_/monsters_/player_ itself.
            if (keyPressed->code == sf::Keyboard::Key::Enter) {
                mode_ = GameMode::ClassSelection;
            }
            return;
        }

        if (mode_ == GameMode::AbilityChoice) {
            handleTreeKey(keyPressed->code, keyPressed->shift);
            return;
        }
        if (keyPressed->code == sf::Keyboard::Key::F5) { saveGame(); return; }

        if (mode_ == GameMode::AttributeAllocation) {
            if (keyPressed->code == sf::Keyboard::Key::Num1) {
                allocateAttribute(0);
            } else if (keyPressed->code == sf::Keyboard::Key::Num2) {
                allocateAttribute(1);
            } else if (keyPressed->code == sf::Keyboard::Key::Num3) {
                allocateAttribute(2);
            }
            return;
        }

        if (inventoryOpen_) {
            handleInventoryKey(keyPressed->code);
            return;
        }
        if (keyPressed->code == sf::Keyboard::Key::B) {
            openInventory();
            return;
        }
        if (keyPressed->code == sf::Keyboard::Key::T) { openTalentTrees(); return; }
        if (keyPressed->code == sf::Keyboard::Key::C) {
            for (std::size_t i=0;i<player_.talents().knownTalents().size();++i)
                if (player_.talents().knownTalents()[i].id=="basic.cleanse") { requestTalent(i); break; }
            return;
        }
        if (keyPressed->code == sf::Keyboard::Key::Space) {
            player_.statusEffects().apply({StatusEffectType::Opening,2,0});
            finishInventoryTurn(); return;
        }
        if (keyPressed->code == sf::Keyboard::Key::G) {
            pickupItem();
            return;
        }
        if (handleTargetingKey(keyPressed->code, keyPressed->shift)) return;

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
            case sf::Keyboard::Key::H:
                returnToTown();
                break;
            case sf::Keyboard::Key::Z:
                startAutoExplore();
                break;
            case sf::Keyboard::Key::R:
                startRest();
                break;
            case sf::Keyboard::Key::Num1:
                requestHotbar(talentPage_ * 9);
                break;
            case sf::Keyboard::Key::Num2:
                requestHotbar(talentPage_ * 9 + 1);
                break;
            case sf::Keyboard::Key::Num3:
                requestHotbar(talentPage_ * 9 + 2);
                break;
            case sf::Keyboard::Key::Num4:
                requestHotbar(talentPage_ * 9 + 3);
                break;
            case sf::Keyboard::Key::Num5:
                requestHotbar(talentPage_ * 9 + 4);
                break;
            case sf::Keyboard::Key::Num6:
                requestHotbar(talentPage_ * 9 + 5);
                break;
            case sf::Keyboard::Key::Num7:
                requestHotbar(talentPage_ * 9 + 6);
                break;
            case sf::Keyboard::Key::Num8:
                requestHotbar(talentPage_ * 9 + 7);
                break;
            case sf::Keyboard::Key::Num9:
                requestHotbar(talentPage_ * 9 + 8);
                break;
            case sf::Keyboard::Key::F5:
                saveGame();
                break;
            default:
                break;
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
    if (auto* ally=dynamic_cast<Monster*>(blocker); ally && ally->allied) {
        ally->setPosition(current); player_.setPosition(target); finishInventoryTurn(); return true;
    }
    if (blocker != nullptr) {
        lastMoveDirection_ = Position{dx, dy};
        for (std::size_t i=0;i<player_.talents().knownTalents().size();++i)
            if (player_.talents().knownTalents()[i].id=="basic.attack") return tryUseTalent(i,target);
        return false;
    }

    player_.setPosition(target);
    for (const auto& item : groundItems_) {
        if (item->position().x == target.x && item->position().y == target.y)
            log("You see ", item->name(), ". G: pick up (1 turn).");
    }
    lastMoveDirection_ = Position{dx, dy};
    advanceEnemyIntents();
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
    if (!autoExploring_ && mode_==GameMode::Playing && target.x==floorExit_.x && target.y==floorExit_.y) {
        cancelTargeting(); exitMenu_=true;
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
    Talent talent = combatTalent(player_,player_.talents().effectiveTalent(talentIndex));

    const auto unavailable = talentUnavailableReason(player_, talentIndex);
    if (!unavailable.empty()) {
        log(unavailable);
        return false;
    }

    const TalentTarget target = targetPreview(talentIndex, cursor, true);
    if (!target.valid) {
        log(target.message);
        return false;
    }
    const auto& affected = target.affected;
    const Actor* chainedTarget = target.chainedTarget;
    const Position blinkDestination = target.destination;

    const auto* equippedWeapon=player_.inventory().equipped(EquipmentSlot::Weapon);
    talent.committedBloodlust=equippedWeapon && equippedWeapon->definition()->weaponKind==WeaponKind::TwoHanded &&
        player_.stats().hp*2<=player_.stats().maxHp ? player_.talents().passiveValue(PassiveKind::Bloodlust) : 0;
    const Position beforeMovement=player_.position();
    const int wasConcealed=player_.statusEffects().magnitudeOf(StatusEffectType::Concealed);
    bool landedAny=false, killedAny=false;
    // Commit point: validation and preview above are side-effect free.
    if (talent.effectKind==TalentEffectKind::Damage) combatThisTurn_=true;
    cancelTargeting();

    player_.stats().mana -= talent.manaCost;
    player_.stats().hp -= talent.hpCost;

    if (talent.boneSwap) {
        if (!affected.empty()) { affected.front()->setPosition(beforeMovement); player_.setPosition(blinkDestination); applyMovementTalents(beforeMovement); }
    } else if (talent.summonCount) {
        summonMinions(talent);
    } else if (talent.shape == EffectShape::Movement) {
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
        if (talent.restoreMana) log("Mana: ",player_.stats().mana,"/",player_.stats().maxMana);
        if (talent.restoreHpPercent) log("HP: ",player_.stats().hp,"/",player_.stats().maxHp);
        if (talent.cleanse) log("Poison, Burn, Chill, Marked and curses removed. Other effects remain.");
    } else {
        if (affected.empty()) log(player_.name(), " uses ", talent.name, " on empty ground.");
        for (Actor* target : affected) {
            Talent hitTalent = talent;
            if (target == chainedTarget) hitTalent.damagePercent /= 2;
            std::string combo;
            if (target->statusEffects().has(StatusEffectType::Marked)) combo+="Marked +25%; charge consumed. ";
            if (talent.consumeBurn && target->statusEffects().has(StatusEffectType::Burn)) combo+="Burn consumed: +50%. ";
            if (talent.consumeShock && target->statusEffects().has(StatusEffectType::Shock)) combo+="Shock consumed: +50%. ";
            if (talent.consumeChill && target->statusEffects().has(StatusEffectType::Chill)) combo+="Chill consumed: +50%. ";
            if (target->statusEffects().has(StatusEffectType::Guard)) combo+="Guard reduces direct damage. ";
            const bool couldStun=target->statusEffects().canReceiveStun();
            const int hpBefore=target->stats().hp;
            if (applyTalentDamage(hitTalent, player_, *target)) {
                landedAny=true; killedAny=killedAny || target->stats().hp<=0;
                if (!combo.empty()) log(target->name(), ": ", combo, "Hit dealt ",hpBefore-target->stats().hp," damage.");
                if (!couldStun && (talent.consumeChill || (talent.onHitEffect && talent.onHitEffect->type==StatusEffectType::Stun)))
                    log(target->name(), " is protected from another stun.");
                if (talent.onHitEffect && talent.onHitEffect->type==StatusEffectType::Marked && target->stats().hp>0)
                    log(target->name(), " is Marked: next direct hit +25%.");
                log(player_.name(), " uses ", talent.name, " on ", target->name(), " (",
                    target->stats().hp, "/", target->stats().maxHp, " hp left)");
                soundManager_.play(SoundEffect::Hit);
                if (target->stats().hp>0 && talent.pushDistance>0) {
                    const auto from=player_.position(), origin=target->position();
                    const Position direction{(origin.x>from.x)-(origin.x<from.x),(origin.y>from.y)-(origin.y<from.y)};
                    for (int step=0;step<talent.pushDistance;++step) {
                        const auto pos=target->position();
                        const Position dest{pos.x+direction.x,pos.y+direction.y};
                        if (!map_.isWalkable(dest.x,dest.y) || isOccupied(dest,target) ||
                            (dest.x!=pos.x && dest.y!=pos.y && (!map_.isWalkable(dest.x,pos.y) || !map_.isWalkable(pos.x,dest.y)))) break;
                        target->setPosition(dest);
                    }
                }
                checkAndHandleDeath(*target);
            } else {
                log(target->name(), " dodges ", player_.name(), "'s ", talent.name, "!");
                soundManager_.play(SoundEffect::Dodge);
            }
        }

        // Retreat follows the aimed direction even when the kick hits air.
        if (talent.retreatDistance > 0) {
            const Position targetPos = cursor;
            const Position awayDirection{player_.position().x - targetPos.x,
                                          player_.position().y - targetPos.y};
            const Position retreatDestination = target.destination;
            player_.setPosition(retreatDestination);
            lastMoveDirection_ = awayDirection;
            log(player_.name(), " vaults back to (", retreatDestination.x, ',',
                retreatDestination.y, ")");
        }
    }

    afterHiddenCast(talent,landedAny,killedAny,wasConcealed);
    if (talent.selfBuffEffect && !talent.returnConcealed && talent.effectKind!=TalentEffectKind::SelfBuff) player_.statusEffects().apply(*talent.selfBuffEffect);
    if (talent.shape==EffectShape::Movement || talent.retreatDistance>0) { applyMovementTalents(beforeMovement); }
    player_.talents().startCooldown(talentIndex);
    advanceEnemyIntents();
    player_.talents().tickCooldowns();
    updateFieldOfView(); // in case Blink moved the player

    currentActor_ = &scheduler_.nextTurn();
    processMonsterTurns();
    advanceTurnsUntilPlayerCanAct();
    return true;
}

void Application::advanceEnemyIntents() {
    for (auto& m:monsters_) {
        if (m->allied && m->remainingLife>0 && --m->remainingLife==0) { m->stats().hp=0; scheduler_.remove(*m); log("A temporary skeleton dissolves."); }
        if (m->intent() && m->intent()->playerActionsRemaining>0) --m->intent()->playerActionsRemaining;
        if (m->recoveryActions>0) --m->recoveryActions;
    }
    removeDeadMonsters();
}

void Application::processMonsterTurns() {
    while (window_.isOpen() && currentActor_ != &player_ && mode_!=GameMode::GameOver) {
        Actor* actor = currentActor_;

        auto* monster=dynamic_cast<Monster*>(actor);
        bool interrupted=false;
        if (monster && monster->intent()) {
            const auto& intent=*monster->intent();
            if (actor->statusEffects().has(StatusEffectType::Stun) ||
                actor->position().x!=intent.origin.x || actor->position().y!=intent.origin.y) {
                log(actor->name(), "'s wind-up is interrupted!");
                monster->intent().reset();
                if (monster->type()==MonsterType::GoblinWarlord || monster->type()==MonsterType::Lich) monster->recoveryActions=1;
                interrupted=true;
            }
        }
        const bool hadPoison = actor->statusEffects().has(StatusEffectType::Poison);
        const int chillMagnitude=actor->statusEffects().magnitudeOf(StatusEffectType::Chill);
        bool chilledMove=false;
        for (const auto& e:actor->statusEffects().active()) if (e.type==StatusEffectType::Chill) chilledMove=e.turnsRemaining%2==1;
        for (const auto& effect:actor->statusEffects().active()) {
            if (isCurse(effect.type) && monster && monster->allied) combatThisTurn_=true;
            if (effect.type==StatusEffectType::Doom && effect.turnsRemaining==1)
                log(actor->name()," suffers ",effect.magnitude," damage from Doom!");
        }
        const bool stunned = tickStatusEffects(*actor);
        if (hadPoison && actor->stats().hp > 0) {
            log(actor->name(), " takes poison damage (", actor->stats().hp, "/",
                actor->stats().maxHp, " hp left)");
        }
        checkAndHandleDeath(*actor);

        if (actor->stats().hp > 0) {
            if (stunned) {
                log(actor->name(), " is stunned and loses a turn!");
            } else if (interrupted) {
                // Displacement cancels the attack and spends this enemy action.
            } else if (monster && monster->recoveryActions>0) {
                // A full player action is guaranteed before the boss acts again.
            } else if (monster && monster->intent()) {
                if (monster->intent()->playerActionsRemaining==0) {
                    const EnemyIntent intent=*monster->intent();
                    monster->intent().reset();
                    if (monster->type()==MonsterType::GoblinWarlord || monster->type()==MonsterType::SkeletonGuard || intent.kind==IntentKind::Summon) {
                        monster->recoveryActions=1;
                        log(actor->name(), " will recover for one player action after this attack.");
                    }
                    if (intent.kind==IntentKind::Summon) {
                        AIDecision summon;
                        summon.type=AIActionType::Summon; summon.movePosition=intent.target;
                        // The committed attempt count is saved, so the reinforcement
                        // order survives interruption, loading, and floor travel.
                        summon.summonType=monster->summonsCommitted==2?MonsterType::SkeletonArcher:MonsterType::SkeletonGuard;
                        summon.abilityIndex=actor->talents().knownTalents().size(); // cooldown reserved at commitment
                        if (isOccupied(intent.target,actor)) log("The occupied ritual tile disrupts the summon!");
                        else { log(actor->name(), " completes its ritual!"); executeAIDecision(*actor,summon,chillMagnitude); }
                    } else {
                        log(actor->name(), " releases its committed attack!");
                        if (intent.contains(player_.position()) && hasLineOfFire(map_,intent.target,player_.position())) {
                            AIDecision hit;
                            hit.type=AIActionType::Attack; hit.target=&player_; hit.attackPower=intent.attackPower;
                            if (intent.kind==IntentKind::MagicStrike) hit.scalingStat=ScalingStat::Intelligence;
                            else if (intent.kind==IntentKind::StunStrike) hit.effectToApply=StatusEffectInstance{StatusEffectType::Stun,1,0};
                            executeAIDecision(*actor,hit,chillMagnitude);
                        } else log("You escaped the marked area.");
                        std::vector<Actor*> victims;
                        for (auto& m:monsters_) if (m.get()!=actor && m->stats().hp>0 && intent.contains(m->position()) && hasLineOfFire(map_,intent.target,m->position())) victims.push_back(m.get());
                        for (auto* ally:victims) {
                            if (mode_==GameMode::GameOver || actor->stats().hp<=0) break;
                            if (ally->stats().hp<=0) continue;
                            AIDecision hit; hit.type=AIActionType::Attack; hit.target=ally; hit.attackPower=intent.attackPower;
                            if (intent.kind==IntentKind::MagicStrike) hit.scalingStat=ScalingStat::Intelligence;
                            if (intent.kind==IntentKind::StunStrike) hit.effectToApply=StatusEffectInstance{StatusEffectType::Stun,1,0};
                            executeAIDecision(*actor,hit,chillMagnitude);
                        }
                    }
                }
            } else if (monster && monster->allied) {
                actMinion(*monster,chilledMove);
            } else if (actor->ai() != nullptr) {
                bool hidden=player_.statusEffects().has(StatusEffectType::Concealed);
                if (hidden) {
                    const float chance=enemyStealthDetectionChance(*actor);
                    // Exactly one check on this enemy's real, non-stunned turn.
                    // Previewing, waiting at a menu and out-of-sight enemies never roll.
                    if (chance>0.f && rollChance(chance)) {
                        player_.statusEffects().remove(StatusEffectType::Concealed);
                        hidden=false;
                        log(actor->name(), " spots you! Concealment breaks.");
                    }
                }
                if (monster) {
                    auto* opponent=nearestOpponent(*actor,hidden);
                    const AIDecision decision=enemyDecision(*monster,opponent);
                    const bool warlord=monster && monster->type()==MonsterType::GoblinWarlord;
                    const bool lich=monster && monster->type()==MonsterType::Lich;
                    const bool blast=monster && (monster->type()==MonsterType::Bomber || monster->type()==MonsterType::OssuaryWarden || warlord) && decision.type==AIActionType::UseAbility;
                    const bool slam=monster && monster->type()==MonsterType::Ogre && decision.type==AIActionType::Attack &&
                        decision.effectToApply && decision.effectToApply->type==StatusEffectType::Stun;
                    const bool guard=monster && monster->type()==MonsterType::SkeletonGuard;
                    const bool heavy=decision.type==AIActionType::Attack && (guard || (warlord && actor->stats().hp*10<=actor->stats().maxHp*3));
                    const bool bolt=lich && decision.type==AIActionType::Attack;
                    const bool summon=lich && decision.type==AIActionType::Summon;
                    if (blast || slam || heavy || bolt || summon) {
                        const Position aim=summon?decision.movePosition:opponent->position();
                        // Both caster and warning tile must be visible before commitment.
                        const auto* allyTarget=dynamic_cast<const Monster*>(opponent);
                        if ((allyTarget && allyTarget->allied) ||
                            (exploredMap_.at(actor->position().x,actor->position().y)==Visibility::Visible &&
                             exploredMap_.at(aim.x,aim.y)==Visibility::Visible)) {
                            const auto kind=summon?IntentKind::Summon:blast||bolt?IntentKind::MagicStrike:heavy?IntentKind::HeavyStrike:IntentKind::StunStrike;
                            const int radius=blast?2:heavy?1:0;
                            const int warning=blast?3:(summon || heavy)?2:1;
                            monster->intent()=EnemyIntent{actor->position(),aim,radius,warning,decision.attackPower,kind};
                            if (!decision.announcement.empty()) log(decision.announcement);
                            if (blast || summon) actor->talents().startCooldown(decision.abilityIndex);
                            if (summon) {
                                ++monster->summonsCommitted;
                                log("Ritual: 2 actions to interrupt or occupy the purple tile. Attempt ",monster->summonsCommitted,"/3.");
                            } else log(actor->name()," prepares a strike: ",warning," action(s) to leave the marked tiles. It can hit other enemies.");
                        }
                    } else if (!(chilledMove && decision.type==AIActionType::Move)) executeAIDecision(*actor,decision,chillMagnitude);
                }
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
        const int manaRegen=combatThisTurn_ || dangerNearby() ? kManaRegenInCombat : kManaRegenOutsideCombat;
        player_.stats().mana =
            std::min(player_.stats().mana + manaRegen, player_.stats().maxMana);

        const bool hadPoison = player_.statusEffects().has(StatusEffectType::Poison);
        if (hadPoison || player_.statusEffects().has(StatusEffectType::Burn) ||
            player_.statusEffects().has(StatusEffectType::ManaDrain) || player_.statusEffects().has(StatusEffectType::Doom)) combatThisTurn_=true;
        for (const auto& effect:player_.statusEffects().active()) {
            if (effect.type==StatusEffectType::Doom && effect.turnsRemaining==1)
                log("Doom erupts for ",effect.magnitude," damage!");
        }
        const bool stunned = tickStatusEffects(player_);
        if (hadPoison && player_.stats().hp > 0) {
            log("Poison deals damage (", player_.stats().hp, "/", player_.stats().maxHp,
                " hp left)");
        }
        checkAndHandleDeath(player_);
        if (!window_.isOpen() || mode_==GameMode::GameOver) {
            return;
        }
        if (!stunned) {
            recordQuietTurn();
            return; // genuinely the player's turn now
        }
        log("You are stunned and lose a turn!");
        player_.talents().tickCooldowns();
        currentActor_ = &scheduler_.nextTurn();
        processMonsterTurns();
    }
}

void Application::executeAIDecision(Actor& actor, const AIDecision& decision, int chillMagnitude) {
    if (decision.type==AIActionType::Attack || decision.type==AIActionType::UseAbility) {
        const auto* attacker=dynamic_cast<const Monster*>(&actor);
        const auto* target=dynamic_cast<const Monster*>(decision.target);
        if (decision.target==&player_ || (attacker && attacker->allied) || (target && target->allied)) combatThisTurn_=true;
    }
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

            if(decision.healAmount>0 && decision.target->stats().hp>0) {
                const int healed=std::min(decision.healAmount,decision.target->stats().maxHp-decision.target->stats().hp);
                decision.target->stats().hp+=healed;
                if(auto* patient=dynamic_cast<Monster*>(decision.target)) patient->lastObservedHp=patient->stats().hp;
                if(auto* healer=dynamic_cast<Monster*>(&actor)) --healer->tactics.heals;
                if(exploredMap_.at(actor.position().x,actor.position().y)==Visibility::Visible)
                    log(actor.name()," heals ",decision.target->name()," for ",healed," HP.");
            }
            bool dodged = false;
            if (decision.attackPower > 0) {
                dodged = rollChance(std::min(.60f,dodgeChance(decision.target->stats().dexterity)+(decision.target->statusEffects().magnitudeOf(StatusEffectType::Evasion)+armourDodgeBonus(*decision.target))/100.f));
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
                    if (chillMagnitude>0) damage=damage*(100-chillMagnitude)/100;
                    const bool marked=decision.target->statusEffects().has(StatusEffectType::Marked);
                    if (marked) damage=damage*(100+kMarkedDamagePercent)/100;
                    bool crit = false;
                    if (rollCrit(actor.stats().dexterity)) {
                        crit = true;
                        damage = static_cast<int>(static_cast<float>(damage) * critDamageMultiplier());
                    }
                    int guard=decision.target->statusEffects().magnitudeOf(StatusEffectType::Guard);
                    if (guard && decision.target->inventory().equipped(EquipmentSlot::OffHand)) guard+=decision.target->talents().passiveValue(PassiveKind::ShieldTraining);
                    guard+=armourGuardBonus(*decision.target);
                    damage=std::max(0,damage-guard);
                    decision.target->stats().hp -= damage;
                    if (marked) { decision.target->statusEffects().consumeMark(); log("Marked consumed: +25% direct damage before Guard."); }
                    if (guard) log("Guard reduced the incoming hit by up to ",guard," damage.");
                    if (damage>0) decision.target->statusEffects().remove(StatusEffectType::Concealed);
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
                const bool blocked=decision.effectToApply->type==StatusEffectType::Stun && !decision.target->statusEffects().canReceiveStun();
                const bool armourBlocked=decision.effectToApply->type==StatusEffectType::Stun && !blocked && armourResistsStun(*decision.target);
                if (!armourBlocked) decision.target->statusEffects().apply(*decision.effectToApply);
                if (armourBlocked) log(decision.target->name(), " resists stun with Unyielding!");
                else if (blocked) log(decision.target->name(), " resists stun: recovery or an existing stun prevents reapplication.");
                else {
                    log(decision.target->name(), " is affected by ",statusName(decision.effectToApply->type),"!");
                    if (decision.effectToApply->type==StatusEffectType::Doom)
                        log("Doom: ",decision.effectToApply->magnitude," delayed HP damage. C removes it; Guard and dodge do not prevent it.");
                    if (decision.effectToApply->type==StatusEffectType::ManaDrain)
                        log("Mana Drain: loses ",decision.effectToApply->magnitude," mana each tick. C removes it.");
                }
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
                scaleDungeonMonster(*summoned,currentFloor_);
                summoned->setRewardsEligible(false);
                summoned->recoveryActions=1; // no immediate attack before a player response
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
        if(auto* m=dynamic_cast<Monster*>(&actor); m && !m->allied) {
            if(m->stats().hp<m->lastObservedHp) {
                m->tactics.concealed=false;
                if(m->tactics.alert==0) alertEnemyGroup(*m,m->position());
            }
            m->lastObservedHp=m->stats().hp;
        }
        return;
    }

    soundManager_.play(SoundEffect::Death);

    if (&actor == &player_) {
        if (player_.talents().passiveValue(PassiveKind::Deathless) &&
            std::find(player_.deathlessSpentFloors.begin(),player_.deathlessSpentFloors.end(),currentFloor_)==player_.deathlessSpentFloors.end()) {
            player_.deathlessSpentFloors.push_back(currentFloor_); player_.stats().hp=1;
            const int guard=player_.talents().passiveValue(PassiveKind::Deathless)-1;
            if (guard) player_.statusEffects().apply({StatusEffectType::Guard,2,guard});
            log("Deathless saves you at 1 HP! Spent on this floor."); return;
        }
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
    if (defeated && defeated->allied) {
        scheduler_.remove(actor);
        const int explosion=player_.talents().passiveValue(PassiveKind::GravePact);
        if (explosion) for (auto& enemy:monsters_) if (!enemy->allied && enemy->stats().hp>0 &&
            std::abs(enemy->position().x-actor.position().x)+std::abs(enemy->position().y-actor.position().y)<=1) {
            enemy->stats().hp-=explosion; checkAndHandleDeath(*enemy);
        }
        return;
    }
    if (defeated) { discoverMonsterLore(*defeated); rewardMonster(*defeated, &actor == boss_); }

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

    if (player_.level()>levelBefore) progressionReviewPending_=true;
    resumeLevelUpSequence();
}

void Application::resumeLevelUpSequence() {
    if (mode_==GameMode::GameOver) return;
    if (player_.unspentAttributePoints()>0) { mode_=GameMode::AttributeAllocation; return; }
    if (progressionReviewPending_) { openTalentTrees(); return; }
    if (pendingFinalVictory_) {
        pendingFinalVictory_=false; mode_=GameMode::GameOver; wonGame_=true;
    }
}

std::vector<Actor*> Application::aliveAllies(const Actor* exclude) {
    std::vector<Actor*> allies;
    for (auto& m : monsters_) {
        if (m.get() != exclude && m->stats().hp > 0 && !m->allied) {
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
    extraLives_=adventureMode_?2:0;
    playerClass_ = cls;
    player_.inventory() = Inventory{};
    player_.baseStats() = statsForClass(cls);
    player_.stats() = player_.baseStats();
    player_.unspentAttributePoints() = 0;
    player_.statusEffects().active().clear();
    nextItemId_ = 1;
    player_.trees().clear();
    player_.bloodRelic=false; player_.animationRelic=false; player_.deathlessSpentFloors.clear();
    player_.treePoints()=1; player_.abilityPoints()=3;
    pendingFinalVictory_ = false;

    progressionReviewPending_=true;
    loot_.restore(std::random_device{}());
    player_.talents() = TalentSet({basicAttack(),basicCleanse()});
    // A fresh class selection is a genuinely new character -- starts at
    // level 1 with 0 XP, same as anyone picking up the game for the
    // first time, regardless of what level a previous run (via this
    // same long-lived player_ object) happened to reach. Loading a save
    // instead (see loadGame()) is the only path that should ever set
    // these to anything else.
    player_.level() = 1;
    player_.xp() = 0;

    floorCache_.clear(); gold_=0; quietTurns_=0; restTurns_=0; exitMenu_=false; combatThisTurn_=false;
    currentFloor_ = 1;
    dungeonMenu_=false;
    mode_ = GameMode::Playing;
    regenerateLevel(kInitialSeed);
    treeSelection_=0;
    while (treeSelection_+1<kTalentTrees.size() && !startingTreeAllowed(cls,kTalentTrees[treeSelection_].tree)) ++treeSelection_;
    abilitySelection_=0; openTalentTrees();
}

void Application::regenerateLevel(unsigned int seed) {
    autoExploring_=false; exploreSeenInterests_.clear();
    inventoryOpen_ = false;
    vaultMenu_=0;
    inventorySelection_ = 0;
    groundItems_.clear();
    cancelTargeting();
    mousePixel_.reset();
    talentPage_ = 0;
    const auto theme=floorTheme(currentFloor_);
    DungeonGenerationParams params;
    params.minRoomSize=theme.minRoomSize;
    params.maxRoomSize=theme.maxRoomSize;
    // Only specific floors generate with a boss room at all -- every
    // other floor is a pure "clear it, find the door" dungeon. kFinalFloor
    // (10) is a placeholder using the same GoblinWarlord as
    // kFirstBossFloor (5) for now -- the actual Lich (see ROADMAP.md)
    // is a separate, later piece of work; this gets the full 10-floor
    // structure and victory gating correct end to end first.
    params.includeBossRoom = (currentFloor_ == kFirstBossFloor || currentFloor_ == 10 || currentFloor_ == kFinalFloor);
    params.includeVault=currentFloor_>=3 && !params.includeBossRoom && nextItemId_<=std::numeric_limits<std::uint64_t>::max()-3;
    const GeneratedDungeon dungeon = generateDungeon(params, seed);
    vaultExists_=dungeon.hasVault; vaultOpened_=false; vaultClaimed_=false;
    vaultCenter_=dungeon.vaultCenter; vaultEntrance_=dungeon.vaultEntrance;
    vaultRewards_.clear();

    map_ = dungeon.map;

    player_.setPosition(dungeon.playerStart);
    floorEntrance_=dungeon.playerStart; floorExit_={-1,-1};

    boss_ = nullptr; // clear before repopulating -- see checkAndHandleDeath for why this matters
    monsters_.clear();
    for (const auto& spawn:planEncounters(dungeon,currentFloor_))
        monsters_.push_back(createMonster(spawn.type,spawn.position,spawn.tier));
    if (vaultExists_) {
        auto guard=createMonster(MonsterType::Goblin,{vaultCenter_.x-1,vaultCenter_.y},MonsterTier::Elite);
        guard->vaultGuard=true; monsters_.push_back(std::move(guard));
        guard=createMonster(MonsterType::Archer,{vaultCenter_.x+1,vaultCenter_.y});
        guard->vaultGuard=true; monsters_.push_back(std::move(guard));
        const auto lootTheme=currentFloor_<=3?LootTheme::Barracks:currentFloor_<=6?LootTheme::Sanctum:LootTheme::Crypts;
        for (int i=0;i<3;++i) {
            auto reward=loot_.generate(currentFloor_,1,nextItemId_++,vaultCenter_,ItemRarity::Rare,lootTheme);
            // Prefer different bases without an unbounded generation loop.
            for (int retry=0;retry<16 && std::any_of(vaultRewards_.begin(),vaultRewards_.end(),[&](const auto& item){return item->definition()==reward->definition();});++retry)
                reward=loot_.generate(currentFloor_,1,reward->instanceId(),vaultCenter_,ItemRarity::Rare,lootTheme);
            vaultRewards_.push_back(std::move(reward));
        }
    }
    if (dungeon.hasBossRoom) {
        // The boss always spawns at Base tier regardless of `tier`
        // above -- see createMonster()'s GoblinWarlord/Lich cases for
        // why. Which boss depends on which floor this is: the Lich is
        // the true final fight, not a placeholder like it was before
        // this was actually built.
        const MonsterType bossType =
            (currentFloor_ >= 10) ? MonsterType::Lich : MonsterType::GoblinWarlord;
        Position bossPosition=dungeon.bossRoomCenter;
        auto startDistance=[&](Position p) {
            const int dx=p.x-dungeon.playerStart.x,dy=p.y-dungeon.playerStart.y;
            return dx*dx+dy*dy;
        };
        // The central 7x7 is guaranteed floor in every generated boss room.
        // If its center is near the start, use its far side to prevent an
        // opening attack. Keep the progression door under the boss below.
        if (startDistance(bossPosition)<=64)
            for (int y=dungeon.bossRoomCenter.y-3;y<=dungeon.bossRoomCenter.y+3;++y)
                for (int x=dungeon.bossRoomCenter.x-3;x<=dungeon.bossRoomCenter.x+3;++x)
                    if (map_.isWalkable(x,y) && startDistance({x,y})>startDistance(bossPosition)) bossPosition={x,y};
        monsters_.push_back(createMonster(bossType, bossPosition));
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
            dungeon.hasBossRoom ? boss_->position() : dungeon.otherRoomCenters.back();
        floorExit_=doorPosition;
        map_.setTile(doorPosition.x, doorPosition.y, Tile{TileType::Door, true, true});
    }

    for (auto& m:monsters_) scaleDungeonMonster(*m,currentFloor_);
    spawnFixedItems();
    spawnFloorChest();
    exploredMap_ = ExploredMap(map_);
    log("Entering ", theme.name, ". ", theme.description);

    scheduler_ = TurnScheduler{};
    scheduler_.add(player_);
    for (auto& m : monsters_) {
        scheduler_.add(*m);
    }

    currentActor_ = &scheduler_.nextTurn();
    // Arrival is not a wait action: no healing, cooldown ticks or quiet turns.
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

SaveGameState Application::captureState(bool includeFloors) {
    SaveGameState state;
    state.adventureMode=adventureMode_; state.extraLives=extraLives_;
    state.map = map_;
    state.exploredMap = exploredMap_;
    state.playerPosition = player_.position();
    state.playerClass = playerClass_;
    state.playerLevel = player_.level();
    state.bloodRelic=player_.bloodRelic; state.animationRelic=player_.animationRelic; state.deathlessSpentFloors=player_.deathlessSpentFloors;
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
    state.vaultExists=vaultExists_; state.vaultOpened=vaultOpened_; state.vaultClaimed=vaultClaimed_;
    state.vaultCenter=vaultCenter_; state.vaultEntrance=vaultEntrance_;
    const auto saveItem = [&](const Item& item, int location) {
        state.items.push_back({item.definition()->id, item.instanceId(), location,
                              location == -2 ? item.position() : Position{}, item.rollTier(), item.affixes()});
    };
    for (std::size_t i=0;i<vaultRewards_.size();++i) saveItem(*vaultRewards_[i],-3-static_cast<int>(i));
    for (const auto& item : groundItems_) saveItem(*item, -2);
    for (const auto& item : player_.inventory().items()) saveItem(*item, -1);
    for (int slot = 0; slot < kEquipmentSlotCount; ++slot) {
        if (const auto* item = player_.inventory().equipped(static_cast<EquipmentSlot>(slot)))
            saveItem(*item, slot);
    }
    state.lastMoveDirection = lastMoveDirection_;

    const auto& talents = player_.talents().knownTalents();
    for (std::size_t i = 0; i < talents.size(); ++i)
        state.playerTalents.push_back({talents[i].id, player_.talents().cooldownRemaining(i), player_.talents().rank(i)});
    state.treePoints=player_.treePoints(); state.abilityPoints=player_.abilityPoints();
    state.trees=player_.trees(); state.hotbar=player_.talents().hotbar();
    state.progressionReviewPending=progressionReviewPending_; state.pendingFinalVictory=pendingFinalVictory_;
    state.defeatedBossName=defeatedBossName_;
    state.playerStatusEffects = player_.statusEffects().active();

    for (auto& m : monsters_) {
        SaveGameState::MonsterSaveData data;
        data.tactics=m->tactics;
        data.type = m->type();
        data.allied=m->allied; data.summonRank=m->summonRank; data.summonIntelligence=m->summonIntelligence; data.remainingLife=m->remainingLife;
        data.position = m->position();
        data.hp = m->stats().hp;
        data.maxHp = m->stats().maxHp;
        data.isBoss = (m.get() == boss_);
        data.tier = m->tier();
        data.rewardsEligible = m->rewardsEligible();
        data.intent = m->intent();
        data.vaultGuard = m->vaultGuard;
        data.recoveryActions=m->recoveryActions; data.summonsCommitted=m->summonsCommitted;
        if (const auto* behavior=dynamic_cast<const BossBehavior*>(m->ai())) {
            data.announcedPhase=behavior->announcedPhase(); data.enraged=behavior->enraged();
        }
        data.statusEffects = m->statusEffects().active();
        const auto& known = m->talents().knownTalents();
        for (std::size_t i = 0; i < known.size(); ++i)
            data.talents.push_back({known[i].id, m->talents().cooldownRemaining(i)});
        state.monsters.push_back(std::move(data));
    }

    state.floorEntrance=floorEntrance_; state.floorExit=floorExit_;
    state.gold=gold_; state.quietTurns=quietTurns_; state.inTown=mode_==GameMode::Town;
    if (includeFloors) for (const auto& entry:floorCache_) if (entry.first!=currentFloor_) state.savedFloors.push_back(entry.second);
    return state;
}

void Application::saveGame() {
    if (engine::saveGame(captureState(),kSaveFilePath)) log("Game saved, including visited floors.");
    else log("Failed to save game.");
}

void Application::loadGame() {
    const auto loaded=engine::loadGame(kSaveFilePath);
    if (!loaded) { log("No valid save file found."); return; }
    restoreState(*loaded);
}

bool Application::restoreState(const SaveGameState& state, bool includeFloors) {
    if(state.extraLives<0 || state.extraLives>2 || (!state.adventureMode && state.extraLives!=0)) { log("Invalid death-mode state."); return false; }
    autoExploring_=false; exploreSeenInterests_.clear();
    inventoryDragSource_.reset();
    inventoryOpen_=false; vaultMenu_=0; exitMenu_=false; restTurns_=0;
    inventorySelection_=0; cancelTargeting(); mousePixel_.reset(); talentPage_=0;
    TalentSet restoredTalents;
    for (const auto& saved:state.playerTalents) {
        const auto* d=findTalentDefinition(saved.id);
        if (!d && saved.id!="basic.attack" && saved.id!="basic.cleanse") { log("Unknown saved ability."); return false; }
        restoredTalents.learnTalent(d ? d->ranks[0] : saved.id=="basic.cleanse" ? basicCleanse() : basicAttack());
        const auto index=restoredTalents.knownTalents().size()-1;
        restoredTalents.setRank(index,saved.rank);
        restoredTalents.setCooldownRemaining(index,saved.cooldown);
    }
    restoredTalents.hotbar()=state.hotbar;
    std::vector<std::unique_ptr<Monster>> restoredMonsters;
    Monster* restoredBoss = nullptr;
    for (const auto& savedMonster : state.monsters) {
        auto monster = createMonster(savedMonster.type, savedMonster.position, savedMonster.tier);
        if (savedMonster.allied) {
            configureMinion(*monster,savedMonster.summonRank,savedMonster.summonIntelligence);
            monster->remainingLife=savedMonster.remainingLife;
        } else scaleDungeonMonster(*monster,state.currentFloor);
        const auto& known = monster->talents().knownTalents();
        if (savedMonster.talents.size() != known.size()) { log("Save contains an incomplete enemy talent kit."); return false; }
        for (const auto& saved : savedMonster.talents) {
            const auto found = std::find_if(known.begin(), known.end(), [&](const auto& talent) { return talent.id == saved.id; });
            if (found == known.end()) { log("Save contains an unknown enemy talent."); return false; }
            monster->talents().setCooldownRemaining(static_cast<std::size_t>(found-known.begin()), saved.cooldown);
        }
        monster->tactics=savedMonster.tactics;
        monster->setRewardsEligible(savedMonster.rewardsEligible);
        monster->intent() = savedMonster.intent;
        monster->vaultGuard = savedMonster.vaultGuard;
        monster->recoveryActions=savedMonster.recoveryActions; monster->summonsCommitted=savedMonster.summonsCommitted;
        if (auto* behavior=dynamic_cast<BossBehavior*>(monster->ai())) behavior->restoreState(savedMonster.announcedPhase,savedMonster.enraged);
        monster->stats().hp = savedMonster.hp;
        monster->lastObservedHp=savedMonster.hp;
        monster->stats().maxHp = savedMonster.maxHp;
        monster->statusEffects().active() = savedMonster.statusEffects;
        for (auto& effect:monster->statusEffects().active()) if (effect.type==StatusEffectType::Stun)
            effect.turnsRemaining=std::min(effect.turnsRemaining,monster->statusEffects().maxStunDuration());
        if (savedMonster.isBoss) {
            if (restoredBoss) { log("Save contains multiple bosses."); return false; }
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
    player_.trees()=state.trees; player_.treePoints()=state.treePoints; player_.abilityPoints()=state.abilityPoints;
    progressionReviewPending_=state.progressionReviewPending;
    defeatedBossName_=state.defeatedBossName;
    adventureMode_=state.adventureMode; extraLives_=state.extraLives;
    player_.level() = state.playerLevel;
    player_.bloodRelic=state.bloodRelic; player_.animationRelic=state.animationRelic; player_.deathlessSpentFloors=state.deathlessSpentFloors;
    player_.xp() = state.playerXp;
    currentFloor_ = state.currentFloor;
    dungeonMenu_=false;
    player_.inventory() = Inventory{};
    groundItems_.clear();
    nextItemId_ = state.nextItemId;
    loot_.restore(state.lootRngState);
    chestPosition_ = state.chestPosition; chestExists_ = state.chestExists; chestClaimed_ = state.chestClaimed;
    ordinaryDrops_ = state.ordinaryDrops;
    vaultExists_=state.vaultExists; vaultOpened_=state.vaultOpened; vaultClaimed_=state.vaultClaimed;
    vaultCenter_=state.vaultCenter; vaultEntrance_=state.vaultEntrance;
    vaultRewards_.clear(); if (vaultExists_ && !vaultClaimed_) vaultRewards_.resize(3);
    player_.baseStats() = state.playerStats;
    player_.stats() = state.playerStats;
    player_.unspentAttributePoints() = state.unspentAttributePoints;
    pendingFinalVictory_ = state.pendingFinalVictory;
    // Validation in loadGame guarantees unique IDs, known definitions and slot compatibility.
    for (const auto& savedItem : state.items) {
        auto item = std::make_unique<Item>(*findItemDefinition(savedItem.definitionId),
                                          savedItem.instanceId, savedItem.position, savedItem.affixes, savedItem.rollTier);
        if (savedItem.location<=-3) vaultRewards_[static_cast<std::size_t>(-3-savedItem.location)]=std::move(item);
        else if (savedItem.location == -2) groundItems_.push_back(std::move(item));
        else {
            player_.inventory().add(std::move(item));
            if (savedItem.location >= 0)
                player_.inventory().equip(player_.inventory().items().size() - 1,static_cast<EquipmentSlot>(savedItem.location));
        }
    }
    player_.refreshEquipmentStats();
    lastMoveDirection_ = state.lastMoveDirection;

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

    if (player_.stats().hp<=0) { mode_=GameMode::GameOver; wonGame_=false; }
    else resumeLevelUpSequence();
    log("Game loaded: ", monsters_.size(), " monsters",
        (boss_ != nullptr ? " (boss present)" : ""), ", player at (", player_.position().x, ',',
        player_.position().y, "), ", player_.stats().hp, '/', player_.stats().maxHp, " hp.");
    floorEntrance_=state.floorEntrance; floorExit_=state.floorExit;
    gold_=state.gold; quietTurns_=state.quietTurns; combatThisTurn_=false;
    if (includeFloors) {
        floorCache_.clear();
        for (const auto& floor:state.savedFloors) floorCache_[floor.currentFloor]=floor;
    }
    if (state.inTown && mode_==GameMode::Playing) mode_=GameMode::Town;
    enforceMinionCap(); refreshHiddenDiscoveries();
    updateFieldOfView();
    return true;
}

void Application::update() {
    if (autoExploring_) { stepAutoExplore(); return; }
    if (codexOpen_) return;
    if (restTurns_<=0) return;
    if (mode_!=GameMode::Playing || inventoryOpen_ || vaultMenu_ || exitMenu_ || dangerNearby()) {
        restTurns_=0; log("Rest stopped."); return;
    }
    bool ready=quietTurns_>=10 && player_.stats().hp>=player_.stats().maxHp && player_.stats().mana>=player_.stats().maxMana;
    for (std::size_t i=0;i<player_.talents().knownTalents().size();++i) ready=ready && player_.talents().isReady(i);
    if (ready) { restTurns_=0; log("Rest complete: fully recovered. Waystone ready."); return; }
    if (restClock_.getElapsedTime().asMilliseconds()<60) return;
    restClock_.restart(); --restTurns_;
    player_.statusEffects().apply({StatusEffectType::Opening,2,0});
    finishInventoryTurn();
}

void Application::renderClassSelection() {
    const sf::Color colors[]{sf::Color(230,150,110),sf::Color(130,170,240),sf::Color(130,220,160)};
    const char* names[]{"1. Warrior","2. Mage","3. Thief"};
    const char* stats[]{"STR 6 / DEX 2 / INT 2   HP 30 / Mana 10","STR 2 / DEX 2 / INT 6   HP 20 / Mana 20","STR 2 / DEX 6 / INT 2   HP 25 / Mana 15"};
    const char* pools[]{"One-Handed, Two-Handed, Shield","Fire, Ice, Lightning, Arcane","Stealth, Bow, Acrobatics"};
    drawText("Choose your starting class",60,60,30,sf::Color(110,220,220));
    drawText("Class determines your starting attributes and first tree choices.",60,110,18,sf::Color(220,220,230));
    for(std::size_t i=0;i<3;++i) {
        sf::RectangleShape card(kClassCards[i].size); card.setPosition(kClassCards[i].position);
        const bool hover=mousePixel_ && kClassCards[i].contains(sf::Vector2f(*mousePixel_));
        card.setFillColor(hover?sf::Color(42,57,70):sf::Color(24,32,44));
        card.setOutlineThickness(hover?2.f:1.f); card.setOutlineColor(colors[i]); window_.draw(card);
        drawText(names[i],70,164+110.f*i,21,colors[i]);
        drawText(stats[i],300,164+110.f*i,17,sf::Color(225,228,238));
        drawText(pools[i],90,198+110.f*i,16,sf::Color(190,198,214));
    }
    drawText("Start with 1 tree point and 3 ability points. Other core trees open at level 5; hidden trees require discovery.",60,490,17,sf::Color(220,220,230));
    drawText("Click a class or press 1-3. J: Codex   F9: load a version-9 through 22 save.",60,535,16,sf::Color(190,190,210));
    for(const auto& rect:{kStartLoadButton,kStartCodexButton,kDeathModeButton}) {
        sf::RectangleShape button(rect.size); button.setPosition(rect.position);
        button.setFillColor(mousePixel_ && rect.contains(sf::Vector2f(*mousePixel_))?sf::Color(44,65,80):sf::Color(28,40,55)); window_.draw(button);
    }
    drawText(adventureMode_?"[M] Adventure: 2 extra lives":"[M] Roguelike: one life",660,590,17,sf::Color::White);
    drawText("Choose mode before clicking a class. Revival returns you to town with your gear.",60,675,15,sf::Color(190,190,210));
    drawText("Load adventure [F9]",70,590,17,sf::Color::White);
    drawText("Open Codex [J]",350,590,17,sf::Color::White);
    if(!logMessages_.empty()) { float y=640; drawWrapped(logMessages_.back(),60,y,135,sf::Color(230,200,150),710); }
}

void Application::allocateAttribute(unsigned int attribute) {
    if(mode_!=GameMode::AttributeAllocation || player_.unspentAttributePoints()<=0 || attribute>2) return;
    if(attribute==0) {
        ++player_.baseStats().strength; ++player_.baseStats().maxHp; log("+1 Strength.");
    } else if(attribute==1) {
        ++player_.baseStats().dexterity; log("+1 Dexterity.");
    } else {
        ++player_.baseStats().intelligence; ++player_.baseStats().maxMana; log("+1 Intelligence.");
    }
    player_.refreshEquipmentStats();
    --player_.unspentAttributePoints();
    soundManager_.play(SoundEffect::Select);
    mode_=GameMode::Playing;
    resumeLevelUpSequence();
}

void Application::renderGameOver() {
    for(const auto& rect:{kRestartButton,kEndCodexButton}) {
        sf::RectangleShape button(rect.size); button.setPosition(rect.position);
        button.setFillColor(mousePixel_ && rect.contains(sf::Vector2f(*mousePixel_))?sf::Color(44,65,80):sf::Color(28,40,55)); window_.draw(button);
    }
    drawText("Open Codex [J]",70,382,17,sf::Color(110,220,220));
    if(!wonGame_ && adventureMode_ && extraLives_>0) {
        sf::RectangleShape button(kReviveButton.size); button.setPosition(kReviveButton.position);
        button.setFillColor(sf::Color(35,70,70)); window_.draw(button);
        drawText("Revive in town [R] - "+std::to_string(extraLives_)+" remaining",70,259,18,sf::Color::White);
        drawText("Full recovery. Keep gear and progress. Spend one extra life.",60,315,16,sf::Color(190,210,210));
    } else if(!wonGame_) drawText("No lives remain. This character has ended.",60,260,18,sf::Color(200,170,170));
    drawText("Collected lore survives this adventure.",60,430,16,sf::Color(170,190,205));
    if (wonGame_) {
        drawText("Victory!", 60.f, 60.f, 32, sf::Color(255, 215, 0));
        drawText("You have slain the " + defeatedBossName_ + ".", 60.f, 120.f, 18,
                  sf::Color(210, 210, 210));
    } else {
        drawText("You Died", 60.f, 60.f, 32, sf::Color(200, 50, 50));
        drawText("The dungeon claims another.", 60.f, 120.f, 18, sf::Color(210, 210, 210));
    }
    drawText("Choose a new character [Enter]", 70.f, 189.f, 18,
              sf::Color(150, 150, 150));
}

void Application::renderAttributeAllocation() {
    drawText("Level up!", 60.f, 60.f, 28, sf::Color(230, 230, 230));
    drawText("Click an option or press 1-3. Each click spends one attribute point.",60.f,132.f,15,sf::Color(165,180,200));

    std::ostringstream subtitle;
    subtitle << "You have " << player_.unspentAttributePoints()
              << (player_.unspentAttributePoints() == 1 ? " point" : " points")
              << " to spend. Choose one:";
    drawText(subtitle.str(), 60.f, 104.f, 16, sf::Color(190, 190, 190));

    const Stats& stats = player_.stats();

    const std::string labels[]{"1. Strength (currently "+std::to_string(stats.strength)+")",
        "2. Dexterity (currently "+std::to_string(stats.dexterity)+")",
        "3. Intelligence (currently "+std::to_string(stats.intelligence)+")"};
    const char* details[]{"+1 Max HP. Scales Strength-based abilities.",
        "+0.5% Dodge (cap 25%), +0.5% Crit. Scales Dexterity-based abilities.",
        "+1 Max Mana. Scales Intelligence-based abilities."};
    const sf::Color colors[]{sf::Color(230,140,100),sf::Color(120,220,140),sf::Color(140,170,230)};
    for(std::size_t i=0;i<3;++i) {
        sf::RectangleShape option(kAttributeChoices[i].size); option.setPosition(kAttributeChoices[i].position);
        const bool hover=mousePixel_ && kAttributeChoices[i].contains(sf::Vector2f(*mousePixel_));
        option.setFillColor(hover?sf::Color(42,57,70):sf::Color(24,32,44));
        option.setOutlineThickness(hover?2.f:1.f); option.setOutlineColor(colors[i]); window_.draw(option);
        drawText(labels[i],65,157+68.f*i,18,colors[i]);
        drawText(details[i],80,183+68.f*i,14,sf::Color(195,201,215));
    }
}

void Application::render() {
    window_.clear(sf::Color(10, 10, 14));

    if (codexOpen_) { renderCodex(); window_.display(); return; }
    if (mode_==GameMode::Town) { renderTown(); window_.display(); return; }
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
        renderTalentTrees();
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
    const auto theme=floorTheme(currentFloor_);
    auto themeColor=[](ThemeColor c) { return sf::Color(c.r,c.g,c.b); };
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
                baseColor = themeColor(theme.wall);
            } else if (tileType == TileType::Door) {
                baseColor = themeColor(theme.door);
            } else {
                baseColor = themeColor(theme.floor);
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
        if (m->tactics.concealed || exploredMap_.at(m->position().x, m->position().y) != Visibility::Visible) {
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
        if (const std::optional<sf::Color> borderColor = isUniqueMonster(m->type()) ? std::optional<sf::Color>(sf::Color(255,190,60)) : tierBorderColor(m->tier());
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
        monsterShape.setFillColor(m->allied ? sf::Color(90,220,220) : monsterColor(m->type()));
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
        if(m->tactics.retreat>0) drawText("Retreat",screenPos.x-8,screenPos.y+14,10,sf::Color(255,220,90));
    }

    sf::RectangleShape playerShape({kTileSize - 1.f, kTileSize - 1.f});
    playerShape.setPosition(worldToScreen(player_.position().x, player_.position().y));
    playerShape.setFillColor(sf::Color(240, 200, 60));
    window_.draw(playerShape);

    // Visible committed danger remains visible even if the caster leaves sight.
    for (const auto& m:monsters_) if (m->stats().hp>0 && m->intent()) {
        const auto& intent=*m->intent();
        for (int y=intent.target.y-intent.radius;y<=intent.target.y+intent.radius;++y)
            for (int x=intent.target.x-intent.radius;x<=intent.target.x+intent.radius;++x) {
                if (!intent.contains({x,y}) || !map_.isWalkable(x,y) || !hasLineOfFire(map_,intent.target,{x,y}) ||
                    exploredMap_.at(x,y)!=Visibility::Visible || x<viewStartX || x>=viewEndX || y<viewStartY || y>=viewEndY) continue;
                sf::RectangleShape warning({kTileSize-3.f,kTileSize-3.f});
                warning.setPosition(worldToScreen(x,y));
                warning.setFillColor(intent.kind==IntentKind::Summon ? sf::Color(195,100,255,160) : intent.radius ? sf::Color(255,145,30,145) : sf::Color(255,55,55,170));
                warning.setOutlineThickness(-2.f);
                warning.setOutlineColor(sf::Color::Yellow);
                window_.draw(warning);
                const auto at=worldToScreen(x,y);
                drawText(std::to_string(intent.playerActionsRemaining),at.x+2,at.y,12,sf::Color::White);
            }
    }

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

    drawText(adventureMode_?"Adventure | Lives +"+std::to_string(extraLives_):"Roguelike | One life",980,46,12,sf::Color(180,200,210));

    // Level/XP -- Prompt 20. "MAX" instead of a fraction once level 10
    // is reached, since grantXp() zeroes xp() there and "X/0" would
    // read as a bug, not a deliberate cap.
    {
        std::ostringstream oss;
        oss << "Level " << player_.level();
        if (player_.level() < kRunMaxLevel) {
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
        oss << dungeonName(dungeonIndex(currentFloor_)) << " " << (currentFloor_-1)%10+1 << "/10 | Depth level " << currentFloor_ << " - " << theme.name;
        drawText(oss.str(), 290.f, 46.f, 13, sf::Color(180, 180, 200));
    }

    renderTargetingPanel();
    renderBattleHud();

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

    renderTravel();
    renderVault();
    if (inventoryOpen_) renderInventory();
    window_.display();
}

} // namespace engine
