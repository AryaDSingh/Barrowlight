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
#include "entities/MonsterFactory.hpp"
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

// Relative to wherever the executable is launched from -- same
// reasoning as avoiding data/ file loading elsewhere in this project
// (see ARCHITECTURE_DECISIONS.md): resolving the executable's own
// directory needs platform-specific APIs this project has deliberately
// avoided needing so far.
constexpr const char* kSaveFilePath = "savegame.txt";

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
      player_(Position{0, 0}, [] {
          Stats stats;
          stats.hp = 30;
          stats.maxHp = 30;
          stats.mana = 20;
          stats.maxMana = 20;
          stats.strength = 12;
          stats.dexterity = 12;
          return stats;
      }()) {
    // Auto-flush cout after every insertion (see ARCHITECTURE_DECISIONS.md,
    // Prompt 9) -- fixes stdout buffering for every diagnostic/combat-log
    // line at once.
    std::cout << std::unitbuf;

    window_.setFramerateLimit(60);
    regenerateLevel(kInitialSeed);
}

void Application::run() {
    while (window_.isOpen()) {
        processEvents();
        update();
        render();
    }
}

void Application::processEvents() {
    while (const std::optional event = window_.pollEvent()) {
        if (event->is<sf::Event::Closed>()) {
            window_.close();
        }

        if (const auto* keyPressed = event->getIf<sf::Event::KeyPressed>()) {
            switch (keyPressed->code) {
                case sf::Keyboard::Key::Escape:
                    window_.close();
                    break;
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
                case sf::Keyboard::Key::F5:
                    saveGame();
                    break;
                case sf::Keyboard::Key::F9:
                    loadGame();
                    break;
                default:
                    break;
            }
        }
    }
}

bool Application::isOccupied(Position pos, const Actor* exclude) const {
    // The integration-pass bug this prompt fixed: movement previously
    // only checked map_.isWalkable() (terrain), never whether another
    // actor already stood on the destination tile. Never surfaced in
    // earlier testing because every scripted test walked to a tile
    // *adjacent* to a target, never onto it -- but nothing stopped a
    // real player (or two monsters converging from different angles)
    // from sharing a tile.
    if (&player_ != exclude && player_.position().x == pos.x && player_.position().y == pos.y) {
        return true;
    }
    for (const auto& m : monsters_) {
        if (m.get() == exclude || m->stats().hp <= 0) {
            continue;
        }
        if (m->position().x == pos.x && m->position().y == pos.y) {
            return true;
        }
    }
    return false;
}

Position Application::resolveBlinkDestination(Position direction, int maxDistance) const {
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

    if (!map_.isWalkable(target.x, target.y) || isOccupied(target, &player_)) {
        return false; // wall, map edge, or another actor's tile -- no turn consumed
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
        std::cout << talent.name << " is on cooldown ("
                  << player_.talents().cooldownRemaining(talentIndex) << " turns left)\n";
        return false;
    }
    if (player_.stats().mana < talent.manaCost) {
        std::cout << "Not enough mana for " << talent.name << " (" << player_.stats().mana
                  << "/" << talent.manaCost << " needed)\n";
        return false;
    }
    if (talent.hpCost > 0 && player_.stats().hp <= talent.hpCost) {
        std::cout << "Not enough hp to safely cast " << talent.name << "\n";
        return false;
    }

    std::vector<Actor*> affected;
    Position blinkDestination = player_.position();

    if (talent.shape == EffectShape::Movement) {
        blinkDestination = resolveBlinkDestination(lastMoveDirection_, talent.moveDistance);
        if (blinkDestination.x == player_.position().x &&
            blinkDestination.y == player_.position().y) {
            std::cout << talent.name << " has nowhere to go that direction\n";
            return false;
        }
    } else if (talent.shape == EffectShape::AreaAroundSelf) {
        affected = actorsWithinRadius(player_.position(), talent.areaRadius);
    } else {
        Actor* anchor = nullptr;
        if (talent.targeting == TargetingMode::AdjacentEnemy) {
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
        std::cout << "No valid target for " << talent.name << "\n";
        return false;
    }

    player_.stats().mana -= talent.manaCost;
    player_.stats().hp -= talent.hpCost;

    if (talent.shape == EffectShape::Movement) {
        player_.setPosition(blinkDestination);
        std::cout << player_.name() << " uses " << talent.name << ", blinks to ("
                  << blinkDestination.x << ',' << blinkDestination.y << ")\n";
    } else {
        for (Actor* target : affected) {
            applyTalentDamage(talent, *target);
            std::cout << player_.name() << " uses " << talent.name << " on " << target->name()
                      << " (" << target->stats().hp << "/" << target->stats().maxHp
                      << " hp left)\n";
            checkAndHandleDeath(*target);
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
            std::cout << actor->name() << " takes poison damage (" << actor->stats().hp << "/"
                      << actor->stats().maxHp << " hp left)\n";
        }
        checkAndHandleDeath(*actor);

        if (actor->stats().hp > 0) {
            if (stunned) {
                std::cout << actor->name() << " is stunned and loses a turn!\n";
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
        const bool hadPoison = player_.statusEffects().has(StatusEffectType::Poison);
        const bool stunned = tickStatusEffects(player_);
        if (hadPoison && player_.stats().hp > 0) {
            std::cout << "Poison deals damage (" << player_.stats().hp << "/"
                      << player_.stats().maxHp << " hp left)\n";
        }
        checkAndHandleDeath(player_);
        if (!window_.isOpen()) {
            return;
        }
        if (!stunned) {
            return; // genuinely the player's turn now
        }
        std::cout << "You are stunned and lose a turn!\n";
        currentActor_ = &scheduler_.nextTurn();
        processMonsterTurns();
    }
}

void Application::executeAIDecision(Actor& actor, const AIDecision& decision) {
    if (!decision.announcement.empty()) {
        std::cout << decision.announcement << '\n';
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

            if (decision.attackPower > 0) {
                int damage = decision.attackPower;
                if (actor.statusEffects().has(StatusEffectType::Empowered)) {
                    damage += actor.statusEffects().magnitudeOf(StatusEffectType::Empowered);
                }
                decision.target->stats().hp -= damage;
                std::cout << actor.name() << " hits " << decision.target->name() << " for "
                          << damage << " (" << decision.target->stats().hp << "/"
                          << decision.target->stats().maxHp << " hp left)\n";
                checkAndHandleDeath(*decision.target);
            }

            if (decision.effectToApply.has_value() && decision.target->stats().hp > 0) {
                decision.target->statusEffects().apply(*decision.effectToApply);
                std::cout << decision.target->name() << " is affected by "
                          << (decision.effectToApply->type == StatusEffectType::Poison ? "poison"
                              : decision.effectToApply->type == StatusEffectType::Stun ? "a stun"
                                                                                        : "a buff")
                          << "!\n";
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
    if (actor.stats().hp > 0) {
        return;
    }

    if (&actor == &player_) {
        std::cout << "You have died!\n";
        // No game-over screen or restart flow exists yet (Prompt 12
        // territory) -- closing cleanly beats leaving the game running
        // in a broken, still-controllable-but-dead state.
        window_.close();
        return;
    }

    if (&actor == boss_) {
        std::cout << actor.name() << " falls! You have slain the Goblin Warlord!\n";
        boss_ = nullptr; // must clear before removeDeadMonsters() erases the underlying object
        scheduler_.remove(actor);
        return;
    }

    std::cout << actor.name() << " dies!\n";
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
}

void Application::saveGame() {
    SaveGameState state;
    state.map = map_;
    state.exploredMap = exploredMap_;
    state.playerPosition = player_.position();
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
        std::cout << "Game saved.\n";
    } else {
        std::cout << "Failed to save game (could not write " << kSaveFilePath << ").\n";
    }
}

void Application::loadGame() {
    const std::optional<SaveGameState> loaded = engine::loadGame(kSaveFilePath);
    if (!loaded.has_value()) {
        std::cout << "No valid save file found (" << kSaveFilePath << ").\n";
        return;
    }
    const SaveGameState& state = *loaded;

    map_ = state.map;
    exploredMap_ = state.exploredMap;

    player_.setPosition(state.playerPosition);
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

    std::cout << "Game loaded: " << monsters_.size() << " monsters"
              << (boss_ != nullptr ? " (boss present)" : "") << ", player at ("
              << player_.position().x << ',' << player_.position().y << "), "
              << player_.stats().hp << '/' << player_.stats().maxHp << " hp." << std::endl;
}

void Application::update() {
    // Nothing here yet -- movement and talents are applied directly in
    // processEvents() since this is a turn-based game with no
    // time-based simulation to advance between player inputs.
}

void Application::render() {
    window_.clear(sf::Color(10, 10, 14));

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
    }

    window_.display();
}

} // namespace engine
