#include "core/Application.hpp"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "ai/Chaser.hpp"
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

// Fixed so every fresh launch starts from the same layout (useful for
// consistent debugging); press R in-game for a new random one.
constexpr unsigned int kInitialSeed = 1337;

// Scales an sf::Color's RGB down for "remembered but not currently
// visible" tiles. A free function rather than duplicated hand-picked
// dimmed colors, so the visible/remembered relationship stays explicit
// and consistent no matter how many tile colors exist later.
sf::Color dim(sf::Color c) {
    constexpr float kDimFactor = 0.35f;
    return sf::Color(static_cast<std::uint8_t>(c.r * kDimFactor),
                      static_cast<std::uint8_t>(c.g * kDimFactor),
                      static_cast<std::uint8_t>(c.b * kDimFactor));
}

bool isAdjacent(Position a, Position b) {
    const int dx = std::abs(a.x - b.x);
    const int dy = std::abs(a.y - b.y);
    return (dx + dy) == 1; // exactly one cardinal step away, matching 4-directional movement
}

bool withinRadius(Position a, Position b, int radius) {
    const int dx = a.x - b.x;
    const int dy = a.y - b.y;
    return dx * dx + dy * dy <= radius * radius; // circular, matching FOV's own radius convention
}

// Walks up to `maxDistance` tiles from `start` in `direction`, stopping
// just before a wall/map edge rather than failing outright if the exact
// destination is blocked -- more forgiving than requiring the full
// distance to be clear.
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
      }()),
      goblin_("Goblin", 'g', Position{0, 0},
              [] {
                  Stats stats;
                  stats.hp = 25;
                  stats.maxHp = 25;
                  return stats;
              }(),
              std::make_unique<Chaser>()) {
    // Auto-flush cout after every insertion, rather than relying on each
    // diagnostic line to remember std::endl individually -- fixes the
    // same stdout-buffering gap hit back in Prompt 5 (console output not
    // visible if the process is interrupted rather than exiting
    // normally), for every current and future diagnostic line at once.
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
                // Talents: 1-4 are Blade (melee), 5-8 are Flame
                // (ranged/AoE/utility) -- matching spellbladeTalents()'s
                // declaration order.
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
    return true;
}

bool Application::tryUseTalent(std::size_t talentIndex) {
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

    // Resolve who/where this talent affects. Application does this (not
    // Talent/TalentEffects) because it's the one thing that currently
    // knows about every Actor in the level.
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
        if (goblinAlive() && withinRadius(player_.position(), goblin_.position(), talent.areaRadius)) {
            affected.push_back(&goblin_);
        }
    } else {
        // SingleTarget and AreaAroundTarget both need an anchor target,
        // resolved by the talent's targeting mode.
        Actor* anchor = nullptr;
        const bool goblinValidAdjacent =
            goblinAlive() && isAdjacent(player_.position(), goblin_.position());
        const bool goblinValidRanged =
            goblinAlive() &&
            exploredMap_.at(goblin_.position().x, goblin_.position().y) == Visibility::Visible;

        if (talent.targeting == TargetingMode::AdjacentEnemy && goblinValidAdjacent) {
            anchor = &goblin_;
        } else if (talent.targeting == TargetingMode::RangedEnemyInSight && goblinValidRanged) {
            anchor = &goblin_;
        }

        if (anchor != nullptr) {
            affected.push_back(anchor);
            // AreaAroundTarget would expand to anyone else within
            // talent.areaRadius of the anchor here. With only one
            // monster in the level right now there's never anyone else
            // to add -- the mechanism is real, just not visibly
            // different from SingleTarget until Prompt 10 adds more
            // enemies.
        }
    }

    if (talent.shape != EffectShape::Movement && affected.empty()) {
        std::cout << "No valid target for " << talent.name << "\n";
        return false;
    }

    // All checks passed -- commit: deduct costs, apply effect(s), start cooldown.
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
            if (target == &goblin_ && !goblinAlive()) {
                std::cout << target->name() << " dies!\n";
                scheduler_.remove(goblin_);
            }
        }
    }

    player_.talents().startCooldown(talentIndex);
    player_.talents().tickCooldowns();
    updateFieldOfView(); // in case Blink moved the player

    currentActor_ = &scheduler_.nextTurn();
    processMonsterTurns();
    return true;
}

bool Application::goblinAlive() const {
    return goblin_.stats().hp > 0;
}

void Application::processMonsterTurns() {
    // Runs AI turns until it's the player's turn again. With only one
    // monster registered this typically runs 0 or 1 times per player
    // move, but is written as a loop so it scales once more monsters
    // exist (Prompt 10) without restructuring.
    while (currentActor_ != &player_) {
        AIBehavior* ai = currentActor_->ai();
        if (ai != nullptr) {
            const std::optional<Position> move =
                ai->decideMove(*currentActor_, map_, player_.position());
            if (move && map_.isWalkable(move->x, move->y)) {
                currentActor_->setPosition(*move);
            }
        }
        currentActor_ = &scheduler_.nextTurn();
    }
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

    goblin_.setPosition(dungeon.monsterStart);
    goblin_.stats().hp = goblin_.stats().maxHp;

    exploredMap_ = ExploredMap(map_);

    // Fresh scheduler rather than trying to reset the existing one --
    // TurnScheduler has no clear() method, and reassignment is simpler
    // than adding API surface just for this.
    scheduler_ = TurnScheduler{};
    scheduler_.add(player_);
    scheduler_.add(goblin_);
    currentActor_ = &scheduler_.nextTurn();
    processMonsterTurns();
    updateFieldOfView();

    std::cout << "Generated dungeon (seed " << seed << "): " << map_.width() << 'x'
              << map_.height() << ", " << dungeon.roomCount << " rooms, player start ("
              << dungeon.playerStart.x << ',' << dungeon.playerStart.y << "), goblin start ("
              << dungeon.monsterStart.x << ',' << dungeon.monsterStart.y << ")" << std::endl;
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

    // Monster rendering respects the player's FOV and is alive -- only
    // draw the goblin when it's both alive and currently visible.
    if (goblinAlive() &&
        exploredMap_.at(goblin_.position().x, goblin_.position().y) == Visibility::Visible) {
        sf::RectangleShape monsterShape({kTileSize - 1.f, kTileSize - 1.f});
        monsterShape.setPosition({static_cast<float>(goblin_.position().x) * kTileSize,
                                   static_cast<float>(goblin_.position().y) * kTileSize});
        monsterShape.setFillColor(sf::Color(200, 60, 60));
        window_.draw(monsterShape);

        // A small floating hp bar above the goblin -- no font/text
        // rendering exists yet (see ARCHITECTURE_DECISIONS.md), so this
        // is the cheapest way to show its health without text.
        const float hpFraction = static_cast<float>(goblin_.stats().hp) /
                                  static_cast<float>(goblin_.stats().maxHp);
        sf::RectangleShape goblinHpBack({kTileSize - 1.f, 4.f});
        goblinHpBack.setPosition({static_cast<float>(goblin_.position().x) * kTileSize,
                                   static_cast<float>(goblin_.position().y) * kTileSize - 6.f});
        goblinHpBack.setFillColor(sf::Color(40, 20, 20));
        window_.draw(goblinHpBack);

        sf::RectangleShape goblinHpFront({(kTileSize - 1.f) * hpFraction, 4.f});
        goblinHpFront.setPosition({static_cast<float>(goblin_.position().x) * kTileSize,
                                    static_cast<float>(goblin_.position().y) * kTileSize - 6.f});
        goblinHpFront.setFillColor(sf::Color(220, 60, 60));
        window_.draw(goblinHpFront);
    }

    sf::RectangleShape playerShape({kTileSize - 1.f, kTileSize - 1.f});
    playerShape.setPosition({static_cast<float>(player_.position().x) * kTileSize,
                              static_cast<float>(player_.position().y) * kTileSize});
    playerShape.setFillColor(sf::Color(240, 200, 60));
    window_.draw(playerShape);

    // Player hp/mana HUD bars -- top-left corner, fixed screen position
    // (not world-space like the goblin's bar above).
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
