#include "core/Application.hpp"

#include <iostream>
#include <optional>
#include <string>
#include <vector>

namespace engine {

namespace {
constexpr unsigned int kWindowWidth = 1280;
constexpr unsigned int kWindowHeight = 720;
constexpr char kWindowTitle[] = "Roguelike Engine - Dev Window";
constexpr float kTileSize = 32.f;

// Hardcoded test level (see ARCHITECTURE_DECISIONS.md for why this is a
// source literal, not a data/ file, at this stage). '#' = wall, '.' =
// floor, '@' = player start (parsed as floor). All rows must be the same
// length -- parseAsciiMap() throws immediately at startup if they aren't,
// which is exactly the safety net a hand-typed grid like this needs.
const std::vector<std::string> kTestMapRows = {
    "####################",
    "#..................#",
    "#..................#",
    "#..................#",
    "#........@.........#",
    "#......####........#",
    "#......####........#",
    "#..................#",
    "#..................#",
    "####################",
};
} // namespace

Application::Application()
    : window_(sf::VideoMode({kWindowWidth, kWindowHeight}), kWindowTitle),
      player_(Position{0, 0}, Stats{}) {
    window_.setFramerateLimit(60);

    Position playerStart;
    map_ = parseAsciiMap(kTestMapRows, playerStart);
    player_.setPosition(playerStart);

    std::cout << "Loaded test map " << map_.width() << 'x' << map_.height()
              << ", player start (" << playerStart.x << ',' << playerStart.y
              << ")" << std::endl;

    scheduler_.add(player_);
    currentActor_ = &scheduler_.nextTurn();
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

    // currentActor_ will always be &player_ right now (nothing else is
    // registered with the scheduler). Once monsters exist (Prompt 7+),
    // this is the spot that needs to branch: if nextTurn() returns
    // something other than the player, consult its AIBehavior instead of
    // waiting for another key press.
    currentActor_ = &scheduler_.nextTurn();
    return true;
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
            sf::RectangleShape tileShape({kTileSize - 1.f, kTileSize - 1.f});
            tileShape.setPosition(
                {static_cast<float>(x) * kTileSize, static_cast<float>(y) * kTileSize});
            tileShape.setFillColor(map_.tileAt(x, y).type == TileType::Wall
                                        ? sf::Color(45, 45, 52)
                                        : sf::Color(90, 90, 100));
            window_.draw(tileShape);
        }
    }

    sf::RectangleShape playerShape({kTileSize - 1.f, kTileSize - 1.f});
    playerShape.setPosition({static_cast<float>(player_.position().x) * kTileSize,
                              static_cast<float>(player_.position().y) * kTileSize});
    playerShape.setFillColor(sf::Color(240, 200, 60));
    window_.draw(playerShape);

    window_.display();
}

} // namespace engine
