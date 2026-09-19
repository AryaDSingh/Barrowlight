#pragma once

#include <SFML/Graphics.hpp>

#include "core/TurnScheduler.hpp"
#include "entities/Player.hpp"
#include "world/Map.hpp"

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
// As of Prompt 5, Application also directly owns the Map, the Player, and
// the TurnScheduler -- the simplest wiring that proves those three
// systems (plus rendering and input) actually integrate. Expect this to
// be pulled out into a dedicated "game state" concept once there's enough
// state to justify separating "what's being simulated" from "the window"
// (multiple monsters, level transitions -- probably around Prompt 7/8).
class Application {
public:
    Application();

    // Runs the main loop until the window is closed. Blocks until then.
    void run();

private:
    void processEvents();
    void update();
    void render();

    // Attempts to move the player by (dx, dy) in tiles. Bumping into a
    // wall or the map edge does nothing and consumes no turn -- standard
    // roguelike behavior. Returns whether the move actually happened.
    bool tryMovePlayer(int dx, int dy);

    sf::RenderWindow window_;
    Map map_;
    Player player_;
    TurnScheduler scheduler_;

    // Whose turn it currently is, per the scheduler. Only ever the player
    // right now (nothing else is registered), but routed through the real
    // scheduler API rather than skipped, so this doesn't need restructuring
    // once monsters exist (Prompt 7+).
    Actor* currentActor_ = nullptr;
};

} // namespace engine
