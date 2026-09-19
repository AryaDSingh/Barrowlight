#pragma once

#include <SFML/Graphics.hpp>

namespace engine {

// Owns the window and the top-level loop shell.
//
// This is deliberately the ONLY class in the codebase allowed to know about
// SFML types. Game logic (Actor and its components: Stats, AIBehavior,
// Inventory, TalentSet, StatusEffects) will depend on this class's public
// interface, never on sf:: directly. That keeps the rendering library
// swappable in principle, and -- more immediately useful -- keeps game
// logic testable without ever needing to open a window.
//
// No game logic lives here. This is pure engine plumbing: open a window,
// pump events, clear the screen, present it, close cleanly.
class Application {
public:
    Application();

    // Runs the main loop until the window is closed. Blocks until then.
    void run();

private:
    void processEvents();
    void update();
    void render();

    sf::RenderWindow window_;
};

} // namespace engine
