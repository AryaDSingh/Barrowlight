#pragma once

#include <SFML/Graphics.hpp>

#include "core/TurnScheduler.hpp"
#include "entities/Monster.hpp"
#include "entities/Player.hpp"
#include "world/ExploredMap.hpp"
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
// systems (plus rendering and input) actually integrate. As of Prompt 6,
// it also owns an ExploredMap, recomputed from the real FOV algorithm
// after every player move. As of Prompt 7, it owns one Monster too, and
// drives its AI turn after each player move. Expect all of this to be
// pulled out into a dedicated "game state" concept once there's enough
// state to justify separating "what's being simulated" from "the window"
// -- getting closer with each prompt, likely due by Prompt 8 or once a
// second monster shows up in Prompt 10.
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

    // Recomputes FOV from the player's current position and folds it
    // into exploredMap_. Called once at startup and again after every
    // successful move.
    void updateFieldOfView();

    // Generates a fresh dungeon from the given seed and resets map_,
    // player_, goblin_, the scheduler, and exploredMap_ to match it.
    // Called once at startup (with a fixed seed, so every launch starts
    // identically) and again on the regenerate key (with a fresh random
    // seed) -- this is the "quickly inspect generated layouts" feature
    // Prompt 8 asked for.
    void regenerateLevel(unsigned int seed);

    // Runs AI turns until it's the player's turn again. With only one
    // monster registered, this typically runs 0 or 1 times per player
    // move, but is written as a loop so it naturally scales once more
    // monsters exist (Prompt 10) without restructuring.
    void processMonsterTurns();

    sf::RenderWindow window_;
    Map map_;
    Player player_;
    Monster goblin_;
    TurnScheduler scheduler_;
    ExploredMap exploredMap_;

    // Whose turn it currently is, per the scheduler.
    Actor* currentActor_ = nullptr;
};

} // namespace engine
