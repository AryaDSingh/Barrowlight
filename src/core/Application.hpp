#pragma once

#include <deque>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "core/SoundManager.hpp"
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
    GameOver,      // Prompt 17: either death or victory -- see Application::wonGame_
    AbilityChoice, // Prompt 24: the Fighter/Sorcerer hybrid path's pick-one screen --
                    // see Application::pendingHybridChoices_/pendingHybridChoiceIsSpecIn_
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

    // Draws the AbilityChoice screen (Prompt 24): lists
    // pendingHybridChoices_ with number-key selection, framed
    // differently depending on pendingHybridChoiceIsSpecIn_. Same
    // standalone-screen approach as renderClassSelection/renderGameOver.
    void renderAbilityChoice();

    // Prompt 24: called from grantXpAndAnnounce() for every level
    // crossed on a level-up. If `level` is a level where a hybrid pick
    // is actually available for playerClass_ (level 5, the one-time
    // spec-in decision; or level 6-10 once already specced, as long as
    // the opposing class's pool isn't fully picked yet), populates
    // pendingHybridChoices_/pendingHybridChoiceIsSpecIn_/
    // pendingHybridChoiceLevel_ and switches mode_ to AbilityChoice,
    // pausing normal play until the person chooses. A no-op for Thief/
    // Spellblade (not hybrid-eligible at all, see
    // HybridSpec::isHybridEligible) and for every level outside 5-10 or
    // where the pool is already exhausted.
    void offerHybridChoiceIfEligible(int level);

    // Grants every base-class talent unlock and offers every hybrid
    // choice from `fromLevel` through player_.level(), inclusive --
    // the actual body of what grantXpAndAnnounce() used to do inline,
    // pulled into its own resumable method once hybrid choices needed
    // to be able to pause partway through. Stops (returns) the instant
    // offerHybridChoiceIfEligible() switches mode_ to AbilityChoice --
    // the AbilityChoice key handling in processEvents() is responsible
    // for calling this again (with fromLevel == the paused level + 1)
    // once the person responds, so a single big XP grant that crosses
    // both a hybrid-choice level and a later base-class unlock level
    // doesn't lose track of the later one while waiting on the choice.
    void processLevelUpEffects(int fromLevel);

    // Recomputes cameraX_/cameraY_ (top-left of the viewport, in tile
    // units) to keep the player roughly centered, clamped so the
    // viewport never scrolls past the map's own edges. Called once at
    // the start of render()'s Playing-mode path -- purely a display
    // concern, computed fresh each frame from player_.position() and
    // map_'s current dimensions, not stored/restored anywhere else
    // (not part of SaveGameState -- it's fully derivable from state
    // that already is).
    void updateCamera();

    // Converts a tile-grid position to the pixel position it should
    // draw at, applying the current camera offset. Every draw call that
    // used to compute `{x * kTileSize, y * kTileSize}` directly (tiles,
    // monsters, the player) goes through this now instead.
    sf::Vector2f worldToScreen(int tileX, int tileY) const;

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
    // player, prints a message and switches to the GameOver screen
    // (Prompt 17); for the boss specifically, prints a victory message,
    // clears boss_, and *also* switches to GameOver -- defeating the
    // boss is a real win condition now, not "the window stays open and
    // you can keep playing" the way it briefly was before Prompt 17;
    // for a regular monster, prints a message and removes it from the
    // scheduler (the actual erase from monsters_ happens in a later
    // cleanup pass, never mid-iteration). Grants XP via
    // grantXpAndAnnounce() for the boss and regular-monster cases (not
    // the player's own death, which grants nothing).
    void checkAndHandleDeath(Actor& actor);

    // Applies `amount` XP to player_ via PlayerLeveling's grantXp(),
    // then logs it -- and, if it crossed a level threshold, a level-up
    // announcement too. The one place XP gain actually becomes visible;
    // grantXp() itself is silent (data + logic only, no logging).
    void grantXpAndAnnounce(int amount);

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
    SoundManager soundManager_; // Prompt 25 -- see SoundManager.hpp for the "sound is a
                                // presentation detail, never a hard requirement" design
    GameMode mode_ = GameMode::ClassSelection;
    PlayerClass playerClass_ = PlayerClass::Spellblade; // meaningless until selectClass() runs
    bool wonGame_ = false; // meaningless unless mode_ == GameOver -- see checkAndHandleDeath

    // Prompt 24: what the AbilityChoice screen is currently offering --
    // populated by offerHybridChoiceIfEligible(), read by
    // renderAbilityChoice() and the AbilityChoice key handling in
    // processEvents(). Meaningless outside mode_ == AbilityChoice.
    // pendingHybridChoiceIsSpecIn_ distinguishes the one-time level-5
    // "commit to the hybrid path or don't" framing (decline is offered)
    // from every later pick (level 6-10, no decline -- already
    // committed).
    std::vector<Talent> pendingHybridChoices_;
    bool pendingHybridChoiceIsSpecIn_ = false;
    // The level that triggered the currently-pending choice -- stored
    // so processLevelUpEffects() can resume from the *next* level once
    // the person responds, rather than losing track of any further
    // levels a single big XP grant also crossed (the boss's reward
    // against an early character, same scenario PlayerLeveling's
    // grantXp() loop exists for, can cross both a hybrid-choice level
    // and a base-class unlock level in one grant).
    int pendingHybridChoiceLevel_ = 0;

    // Top-left of the viewport, in tile units -- see updateCamera().
    // Recomputed every frame in Playing mode; 0,0 elsewhere (harmless,
    // since ClassSelection/GameOver don't draw the tile grid at all).
    int cameraX_ = 0;
    int cameraY_ = 0;

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
