#include "core/Application.hpp"

#include <cstdint>
#include <iostream>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "ai/Chaser.hpp"
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
} // namespace

Application::Application()
    : window_(sf::VideoMode({kWindowWidth, kWindowHeight}), kWindowTitle),
      player_(Position{0, 0}, Stats{}),
      goblin_("Goblin", 'g', Position{0, 0}, Stats{}, std::make_unique<Chaser>()) {
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
    updateFieldOfView();

    currentActor_ = &scheduler_.nextTurn();
    processMonsterTurns();
    return true;
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
            // Defensive walkability check, same philosophy as
            // tryMovePlayer -- Chaser's own moves are always walkable by
            // construction (they come from findPath), but Application
            // shouldn't blindly trust arbitrary AIBehavior output.
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
    goblin_.setPosition(dungeon.monsterStart);

    exploredMap_ = ExploredMap(map_);

    // Fresh scheduler rather than trying to reset the existing one --
    // TurnScheduler has no clear() method (no prior need for one), and a
    // plain reassignment is simpler than adding API surface just for
    // this. TurnScheduler's own default constructor is already implicit
    // (its only member is a vector), so this needs no changes there.
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
    // Nothing here yet -- movement is applied directly in
    // processEvents()/tryMovePlayer() since this is a turn-based game
    // with no time-based simulation to advance between player inputs.
}

void Application::render() {
    window_.clear(sf::Color(10, 10, 14));

    for (int y = 0; y < map_.height(); ++y) {
        for (int x = 0; x < map_.width(); ++x) {
            const Visibility vis = exploredMap_.at(x, y);
            if (vis == Visibility::Hidden) {
                continue; // never seen -- draw nothing, background shows through
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

    // Monster rendering respects the player's FOV -- only draw the
    // goblin when the player can currently see it. Deliberately checks
    // Visible, not Remembered: unlike static terrain, a monster that
    // moved away from a remembered tile shouldn't still appear to be
    // standing there.
    if (exploredMap_.at(goblin_.position().x, goblin_.position().y) == Visibility::Visible) {
        sf::RectangleShape monsterShape({kTileSize - 1.f, kTileSize - 1.f});
        monsterShape.setPosition({static_cast<float>(goblin_.position().x) * kTileSize,
                                   static_cast<float>(goblin_.position().y) * kTileSize});
        monsterShape.setFillColor(sf::Color(200, 60, 60));
        window_.draw(monsterShape);
    }

    // The player's own tile is always Visibility::Visible (computeFieldOfView
    // always includes the origin), so no separate visibility check is needed
    // here.
    sf::RectangleShape playerShape({kTileSize - 1.f, kTileSize - 1.f});
    playerShape.setPosition({static_cast<float>(player_.position().x) * kTileSize,
                              static_cast<float>(player_.position().y) * kTileSize});
    playerShape.setFillColor(sf::Color(240, 200, 60));
    window_.draw(playerShape);

    window_.display();
}

} // namespace engine
