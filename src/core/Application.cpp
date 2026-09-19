#include "core/Application.hpp"

#include <optional>

namespace engine {

namespace {
constexpr unsigned int kWindowWidth = 1280;
constexpr unsigned int kWindowHeight = 720;
constexpr char kWindowTitle[] = "Roguelike Engine - Dev Window";
} // namespace

Application::Application()
    : window_(sf::VideoMode({kWindowWidth, kWindowHeight}), kWindowTitle) {
    window_.setFramerateLimit(60);
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
            if (keyPressed->code == sf::Keyboard::Key::Escape) {
                window_.close();
            }
        }
    }
}

void Application::update() {
    // Nothing yet. This is where the turn/tick loop will live once there's
    // an actual game state to advance.
}

void Application::render() {
    window_.clear(sf::Color(20, 20, 28)); // dark slate -- easy on the eyes, proves the clear/present cycle works
    window_.display();
}

} // namespace engine
