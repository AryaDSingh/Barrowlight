#include "core/Application.hpp"

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

// The full roster, in the order rooms get populated (room 1 = kRoster[0],
// room 2 = kRoster[1], ...). Fewer than 6 non-player rooms means a
// partial roster, not a crash -- see regenerateLevel.
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

Position resolveBlinkDestination(const Map& map, Position start, Position direction,
                                  int maxDistance) {
    Position current = start;
    for (int i = 0; i < maxDistance; ++i) {
        const Position next{current.x + direction.x, current.y + direction.y};
        if (!map.isWalkable(next.x, next.y)) {
            break;
        }
        current = next;
    }
    return current;
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
                default:
                    break;
            }
        }
    }
}

bool Application::tryMovePlayer(int dx, int dy) {
    if (!window_.isOpen()) {
        return false;
    }

    const Position current = player_.position();
    const Position target{current.x + dx, current.y + dy};

    if (!map_.isWalkable(target.x, target.y)) {
        return false; // bumped a wall or the map edge -- no turn consumed
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
        blinkDestination = resolveBlinkDestination(map_, player_.position(), lastMoveDirection_,
                                                    talent.moveDistance);
        if (blinkDestination.x == player_.position().x &&
            blinkDestination.y == player_.position().y) {
            std::cout << talent.name << " has nowhere to go that direction\n";
            return false;
        }
    } else if (talent.shape == EffectShape::AreaAroundSelf) {
        affected = actorsWithinRadius(player_.position(), talent.areaRadius);
    } else {
        // SingleTarget or AreaAroundTarget both need an anchor target first.
        Actor* anchor = nullptr;
        if (talent.targeting == TargetingMode::AdjacentEnemy) {
            anchor = findAdjacentEnemy();
        } else if (talent.targeting == TargetingMode::RangedEnemyInSight) {
            anchor = findNearestVisibleEnemy();
        }

        if (anchor != nullptr) {
            if (talent.shape == EffectShape::AreaAroundTarget) {
                // Real AoE now that there's more than one monster to find
                // -- Fireball can genuinely hit several enemies at once.
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
    switch (decision.type) {
        case AIActionType::Move:
            if (map_.isWalkable(decision.movePosition.x, decision.movePosition.y)) {
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
        // No game-over screen or restart flow exists yet (Prompt 11/12
        // territory) -- closing cleanly beats leaving the game running
        // in a broken, still-controllable-but-dead state.
        window_.close();
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

    monsters_.clear();
    for (std::size_t i = 0; i < dungeon.otherRoomCenters.size() && i < kRoster.size(); ++i) {
        monsters_.push_back(createMonster(kRoster[i], dungeon.otherRoomCenters[i]));
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
              << " monsters, player start (" << dungeon.playerStart.x << ','
              << dungeon.playerStart.y << ")" << std::endl;
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

    window_.display();
}

} // namespace engine
