#pragma once

#include <deque>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "core/TurnScheduler.hpp"
#include "entities/AIBehavior.hpp"
#include "entities/Monster.hpp"
#include "entities/Player.hpp"
#include "entities/PlayerClass.hpp"
#include "world/ExploredMap.hpp"
#include "world/Map.hpp"

namespace engine {

// Which top-level screen the game is currently showing. Introduced at
// Prompt 15 alongside the multi-class system -- before this, the
// constructor went straight into Playing (always as the Spellblade,
// the only class that existed). ClassSelection is deliberately simple:
// a text menu, not a separate scene/state-machine framework -- this
// project's established minimal-but-real UI approach (Prompt 13), just
// applied to one more screen.
enum class GameMode {
    ClassSelection,
    Playing,
    GameOver, // Prompt 17: either death or victory -- see Application::wonGame_
};

// Owns the window and the top-level loop shell.
//
// This is deliberately the ONLY class in the codebase allowed to know about
// SFML types. Game logic (Actor and its components: Stats, AIBehavior,
// Inventory, TalentSet, StatusEffects) depends on this class's public
// interface, never on sf:: directly.
//
// As of Prompt 10, Application owns a full monster roster (monsters_, a
// vector -- the single hardcoded goblin_ member is gone) and resolves
// everything about combat: player/monster targeting, AoE membership,
// Empowered damage bonuses, status-effect ticking (including
// stun-skipping turns), death for both sides. As of Prompt 11, it also
// tracks a boss_ pointer into monsters_ (for the set-piece encounter and
// victory detection) and enforces tile occupancy -- the integration-pass
// audit found that movement only ever checked terrain walkability, never
// whether another actor already stood there. This is still genuinely the
// "game state" concern flagged as overdue since Prompt 5 -- still not
// extracted into its own class (that refactor stays deliberately
// deferred; nothing here demands it be done *this* prompt, just noted
// that the case for it keeps getting stronger).
//
// As of Prompt 13, Application also owns a loaded sf::Font and every
// call site that used to print only to the console now goes through
// log() instead, which does both: prints to std::cout exactly as
// before (so existing verification-by-console-output still works
// unchanged) and keeps a rolling on-screen log buffer render() draws
// each frame.
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
    // wall, the map edge, or another actor's tile does nothing and
    // consumes no turn.
    bool tryMovePlayer(int dx, int dy);

    // Attempts to activate the player's talent at `talentIndex` (0-7).
    // Validates cooldown/mana/hp/target before committing anything.
    bool tryUseTalent(std::size_t talentIndex);

    // Walks up to `maxDistance` tiles from the player's position in
    // `direction` for Blink, stopping just before a wall or an
    // actor-occupied tile rather than requiring the full distance clear.
    Position resolveBlinkDestination(Position direction, int maxDistance);

    // Recomputes FOV from the player's current position.
    void updateFieldOfView();

    // Generates a fresh dungeon from the given seed and resets
    // everything (map, player, the full monster roster including the
    // boss if the layout has room for one, scheduler, exploredMap_) to
    // match it.
    void regenerateLevel(unsigned int seed);

    // Applies `cls` to player_ (stats and talents both, via
    // PlayerClassFactory), records it as playerClass_ (so a later R
    // regenerates as the same class, and save/load knows which kit a
    // loaded save's cooldowns belong to), switches mode_ to Playing, and
    // generates the first dungeon. The one place a class selection
    // actually takes effect -- processEvents() only reads input and
    // calls this, it doesn't touch player_ itself.
    void selectClass(PlayerClass cls);

    // Draws the ClassSelection screen: a plain text menu, not a
    // separate scene graph -- this project's established minimal HUD
    // approach (Prompt 13), just for one more screen instead of the
    // gameplay HUD.
    void renderClassSelection();

    // Draws the GameOver screen: death or victory, distinguished by
    // wonGame_. Same standalone-screen approach as renderClassSelection
    // -- replaces the whole view rather than overlaying the game world.
    void renderGameOver();

    // Gathers current map/player/monster/exploredMap_ state into a
    // SaveGameState and writes it via engine::saveGame(). Prints whether
    // it succeeded; never throws or crashes on I/O failure.
    void saveGame();

    // Reads a save file via engine::loadGame() and, if valid, replaces
    // map_/player_/monsters_/boss_/exploredMap_/scheduler_ with the
    // loaded state -- structurally the same "replace everything"
    // approach regenerateLevel() uses, just from saved data instead of
    // fresh generation. Prints a friendly message and changes nothing if
    // no valid save exists.
    void loadGame();

    // Runs turns (status-effect ticks, AI decisions) until it's the
    // player's turn again.
    void processMonsterTurns();

    // Once it's genuinely the player's turn per the scheduler, this
    // handles their own status effects (poison, stun) -- a stunned
    // player doesn't get to act this turn either, so this skips forward
    // (still ticking status effects, still consuming turns) until the
    // player can actually act or the game ends.
    void advanceTurnsUntilPlayerCanAct();

    // Executes an AIDecision returned by some Actor's AIBehavior:
    // applies the move/attack/ability/self-buff, prints what happened
    // (including any announcement the behavior set, e.g. a boss phase
    // transition), and checks for death. `actor` is whoever made the
    // decision (never the player -- only monsters have an AIBehavior to
    // execute here).
    void executeAIDecision(Actor& actor, const AIDecision& decision);

    // If `actor`'s hp has dropped to 0 or below, handles it: for the
    // player, prints a message and closes the window (no game-over
    // screen exists yet -- Prompt 12 territory); for the boss
    // specifically, prints a victory message and clears boss_ (the
    // window stays open -- unlike death, victory isn't a dead end, the
    // person can keep exploring or press R for another run); for a
    // regular monster, prints a message and removes it from the
    // scheduler (the actual erase from monsters_ happens in a later
    // cleanup pass, never mid-iteration).
    void checkAndHandleDeath(Actor& actor);

    // True if `pos` is currently occupied by a living actor other than
    // `exclude` (the player, or any living monster). The integration-pass
    // bug this prompt fixed: movement previously only checked terrain,
    // never this -- see ARCHITECTURE_DECISIONS.md.
    bool isOccupied(Position pos, const Actor* exclude);

    // The living actor (other than `exclude`) standing at `pos`, or
    // nullptr. isOccupied() is this with the result reduced to a bool;
    // tryMovePlayer's bump-into-a-monster handling (Phase 2, Prompt 13)
    // needs the actor itself, to name it and to trigger its own turn.
    Actor* actorAt(Position pos, const Actor* exclude);

    // Every monster except `exclude`, still alive. Built fresh each time
    // an AIBehavior needs it (only Support actually uses it) rather than
    // maintained incrementally -- monsters_ is small enough that this
    // costs nothing worth optimizing.
    std::vector<Actor*> aliveAllies(const Actor* exclude);

    // The nearest living monster adjacent to the player, or nullptr.
    Actor* findAdjacentEnemy();
    // The nearest living monster currently visible to the player, or nullptr.
    Actor* findNearestVisibleEnemy();
    // Every living monster within `radius` of `center` -- real AoE
    // resolution now that there's more than one monster to find.
    std::vector<Actor*> actorsWithinRadius(Position center, int radius);

    // Erases any monsters_ entries whose hp has dropped to 0 or below.
    // Called once at the end of processMonsterTurns(), never mid-loop.
    void removeDeadMonsters();

    // Concatenates `args` (anything operator<< accepts, same as chaining
    // std::cout <<) into one message and routes it through logImpl() --
    // the single replacement for every std::cout call site that used to
    // exist directly in this file. A template so call sites read almost
    // exactly like the std::cout chains they replaced; the real work
    // (print + buffer) lives in logImpl() so it isn't duplicated per
    // instantiation.
    template <typename... Args>
    void log(Args&&... args) {
        std::ostringstream oss;
        (oss << ... << args);
        logImpl(oss.str());
    }

    // Prints `message` to std::cout (exactly as every direct std::cout
    // call here used to -- console-based verification from earlier
    // prompts still works unchanged) and appends it to logMessages_,
    // capped at kMaxLogMessages, for render() to draw on-screen.
    void logImpl(const std::string& message);

    // Draws one line of text at (x, y) in pixels. A thin wrapper around
    // sf::Text -- not a new abstraction layer, just avoids repeating
    // setString/setCharacterSize/setFillColor/setPosition/draw at every
    // call site in render().
    void drawText(const std::string& text, float x, float y, unsigned int size,
                   sf::Color color);

    sf::RenderWindow window_;
    sf::Font font_;
    GameMode mode_ = GameMode::ClassSelection;
    PlayerClass playerClass_ = PlayerClass::Spellblade; // meaningless until selectClass() runs
    bool wonGame_ = false; // meaningless unless mode_ == GameOver -- see checkAndHandleDeath
    Map map_;
    Player player_;
    std::vector<std::unique_ptr<Monster>> monsters_;
    Monster* boss_ = nullptr; // non-owning pointer into monsters_, if the level has a boss room
    TurnScheduler scheduler_;
    ExploredMap exploredMap_;

    // Rolling on-screen combat log -- oldest messages drop off the front
    // as new ones are appended. Deque specifically for cheap pop_front();
    // this is never indexed randomly, only iterated front-to-back.
    std::deque<std::string> logMessages_;
    static constexpr std::size_t kMaxLogMessages = 6;

    // Direction of the player's last successful move -- Blink teleports
    // in this direction.
    Position lastMoveDirection_{0, -1};

    // Whose turn it currently is, per the scheduler.
    Actor* currentActor_ = nullptr;
};

} // namespace engine
