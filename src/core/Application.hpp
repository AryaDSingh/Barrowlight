#pragma once

#include <functional>

#include <climits>
#include <deque>
#include <set>
#include <random>
#include <unordered_map>
#include <memory>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <SFML/Graphics.hpp>

#include "core/SoundManager.hpp"
#include "core/SpriteAtlas.hpp"
#include "core/UiKit.hpp"
#include "world/Landmark.hpp"
#include "entities/Patrons.hpp"
#include "world/Surfaces.hpp"
#include "core/SaveGame.hpp"
#include "core/TurnScheduler.hpp"
#include "entities/AIBehavior.hpp"
#include "entities/Monster.hpp"
#include "entities/Player.hpp"
#include "entities/PlayerClass.hpp"
#include "entities/LootGenerator.hpp"
#include "world/ExploredMap.hpp"
#include "world/Map.hpp"
#include "world/TalentTargeting.hpp"

namespace engine {
Element talentElement(const Talent& t); // what an ability does to the ground (ApplicationSurfaces.cpp)

// Which top-level screen the game is currently showing. Introduced at
// Prompt 15 alongside the multi-class system -- before this, the
// constructor went straight into Playing (always as the Spellblade,
// the only class that existed). ClassSelection is deliberately simple:
// a text menu, not a separate scene/state-machine framework -- this
// project's established minimal-but-real UI approach (Prompt 13), just
// applied to one more screen.
// The Encounter Lab's three builds: Specialist, Hybrid, Broad.
const char* labBuildName(int build);

enum class GameMode {
    Town,
    ClassSelection,
    Playing,
    GameOver,             // either death or victory -- see Application::wonGame_
    AbilityChoice,        // Tree browser: unlock, specialize, learn and rank up.
    AttributeAllocation,  // spending earned attribute points -- see
                           // Application::offerAttributeAllocationIfPending()
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
    void handleEvent(const sf::Event& event);
    void update();
    void render();

    // Attempts to move the player by (dx, dy) in tiles. Bumping into a
    // wall, the map edge, or another actor's tile does nothing and
    // consumes no turn.
    bool tryMovePlayer(int dx, int dy);

    // Attempts to activate any learned talent at an explicitly chosen tile.
    // Validates cooldown/mana/hp/target before committing anything.
    bool tryUseTalent(std::size_t talentIndex, Position cursor);
    void requestTalent(std::size_t talentIndex);
    void cancelTargeting();
    void cycleTarget(int direction = 1);
    bool handleTargetingKey(sf::Keyboard::Key key, bool shift);
    void handleTargetingMouse(const sf::Event& event);
    void changeTalentPage(int direction);
    std::optional<Position> screenToWorld(sf::Vector2i pixel) const;
    std::optional<std::size_t> talentAtPixel(sf::Vector2i pixel) const;
    std::vector<Actor*> targetingEnemies() const;
    TalentTarget targetPreview(std::size_t talentIndex, Position cursor, bool includeConcealed=false);
    void renderTargetingOverlay();
    // Hover tooltips (talent, status, log line, enemy) and the mode banner.
    void renderHudTooltips();
    void renderMapHints();
    void renderMinimap(sf::FloatRect area);
    std::optional<Position> minimapTile(sf::Vector2f screen) const;
    bool onMap(sf::Vector2f screen) const;
    std::optional<std::size_t> hoveredLogLine() const;
    SpriteFrame playerSpriteFrame() const;
    // Contextual one-liners for the top-left of the map, gathered during
    // render() and drawn once by renderMapHints().
    std::vector<std::pair<std::string, sf::Color>> mapHints_;
    void drawWrapped(const std::string& text, float x, float& y,
                     std::size_t columns, sf::Color color, float bottom);

    // Coordinates/indexes only: no selection can retain an erased Actor*.
    std::optional<std::size_t> aimingTalent_;
    std::optional<std::size_t> hoveredTalent_;
    bool inspecting_ = false;
    int inspectionScroll_=0;
    int statusPage_=0, combatLogScroll_=0;
    void renderBattleHud();
    std::optional<StatusEffectInstance> hoveredStatus() const;
    std::optional<Position> inspectionAnchor_;
    Position targetCursor_;
    std::optional<sf::Vector2i> mousePixel_;
    std::size_t talentPage_ = 0;

    friend struct ApplicationTargetingTestAccess;
    friend struct ApplicationRewardsTestAccess;
    friend struct PlaytestBot;

    void scaleDeepMonster(Monster& monster,int floor);
    void configureMinion(Monster& monster,int rank,int intelligence);
    int minionCap() const;
    void enforceMinionCap();
    void dissolveMinions();
    void summonMinions(const Talent& talent);
    Actor* nearestOpponent(Actor& actor,bool playerHidden);
    void actMinion(Monster& minion);
    void afterHiddenCast(const Talent& talent,bool landed,bool killed,int concealed);
    std::size_t imbueSelection_=0;


    bool inventoryOpen_ = false;
    std::size_t inventorySelection_ = 0; // equipment slots, followed by bag rows
    std::size_t inventoryBagPage_ = 0;
    std::optional<std::size_t> inventoryDragSource_;
    std::optional<EquipmentSlot> inventoryEquipTarget_;
    std::vector<std::unique_ptr<Item>> groundItems_;
    std::uint64_t nextItemId_ = 1;
    void openInventory();
    void handleInventoryKey(sf::Keyboard::Key key);
    void renderInventory();
    void handleInventoryMouse(const sf::Event& event);
    void renderGroundItems();
    void pickupItem();
    // The floor's chest stands in the way; bumping it (or G beside it) opens it.
    bool chestAt(Position p) const { return chestExists_ && p.x == chestPosition_.x && p.y == chestPosition_.y; }
    bool besideChest() const;
    void spillLoot(Position from, ItemRarity rarity, int quality);
    void openChest();
    // keepOpen: an inventory action (equip, remove, drop) that takes a turn
    // but leaves the inventory open, so you can equip more.
    void finishInventoryTurn(bool keepOpen = false);
    void updateGlimpses();
    std::string aimingSummary();      // one line for the banner while you aim            // after the enemies act: who was just alerted
    void renderDraggedItem();         // the item riding on the cursor while you drag it
    void spawnFixedItems();
    LootGenerator loot_;
    Position chestPosition_;
    bool chestExists_ = false, chestClaimed_ = false, chestMimic_ = false;
    int ordinaryDrops_ = 0;
    void rewardMonster(Monster& monster, bool boss);
    void spawnFloorChest();
    // The Encounter Lab (ApplicationLab.cpp): a fixed, seeded run of three
    // rooms for comparing builds, logged turn by turn to encounter-lab/.
    bool labMode_ = false, labRun_ = false;
    int labBuild_ = 0;          // 0 Specialist, 1 Hybrid, 2 Broad
    unsigned labSeed_ = 1;
    int labTurn_ = 0, labRoom_ = 1;
    std::vector<std::string> labLog_;
    void startLab(PlayerClass cls);
    void buildLabCharacter();
    void buildLabMap();
    int labRoomAt(Position p) const;
    void labNote(const std::string& line);
    void labTurnBegins();
    void labEnemyDecision(const Monster& monster, const AIDecision& decision);
    void finishLab(const char* outcome);
    bool vaultExists_=false, vaultOpened_=false, vaultClaimed_=false;
    Position vaultCenter_{}, vaultEntrance_{};
    std::vector<std::unique_ptr<Item>> vaultRewards_;
    int vaultMenu_=0; // 0 closed, 1 entrance warning, 2 reward choice
    std::size_t vaultSelection_=0;
    bool vaultCleared() const;
    bool interactVault();
    void handleVaultKey(sf::Keyboard::Key key);
    void handleVaultMouse(const sf::Event& event);
    void renderVault();
    std::size_t treeSelection_ = 0, abilitySelection_ = 0;
    // A resonance picked from the strip above the details (an index into
    // resonances()); picking a tree node clears it.
    std::optional<std::size_t> resonanceSelection_;
    sf::FloatRect resonanceRect(std::size_t visible) const;
    std::vector<std::size_t> visibleResonances() const;
    void renderResonanceDetails();
    std::string treeFeedback_;
    bool bindingTalent_=false;
    bool progressionReviewPending_ = false;
    void openTalentTrees();
    void handleTreeKey(sf::Keyboard::Key key, bool shift);
    void handleTreeMouse(const sf::Event& event);
    void renderTalentTrees();
    // Screen rect of one ability icon on the talent screen (tests click it).
    sf::FloatRect talentTreeAbilityRect(std::size_t tree, std::size_t ability) const;
    // The tree columns scroll (mouse wheel, or by following the keyboard
    // selection) once there are more trees than fit. treeViewBottom_ is where
    // the scrolling area ends; tests shrink it to force an overflow.
    // Each column of the talent screen scrolls on its own.
    std::array<float, 3> treeScroll_{};
    float treeViewBottom_ = 712.f;
    float treeScrollMax(int column) const;
    void scrollTrees(int column, float pixels);
    int treeColumnOf(std::size_t tree) const;
    void revealSelectedTree();
    void closeTalentTrees();
    void requestHotbar(std::size_t slot);
    void applyMovementTalents(Position previous);

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
    void allocateAttribute(unsigned int attribute);

    // Draws the ClassSelection screen: a plain text menu, not a
    // separate scene graph -- this project's established minimal HUD
    // approach (Prompt 13), just for one more screen instead of the
    // gameplay HUD.
    void renderClassSelection();

    // Draws the GameOver screen: death or victory, distinguished by
    // wonGame_. Same standalone-screen approach as renderClassSelection
    // -- replaces the whole view rather than overlaying the game world.
    void renderGameOver();

    void resumeLevelUpSequence();
    bool pointsToSpend() const;
    void openLevelUp();

    // Ascendancy and its trials (ApplicationAscendancy.cpp). trial_ is the
    // trial arena the player is in (0 = none); trialReturnFloor_ the
    // dungeon floor waiting in floorCache_ to be put back afterwards.
    int trial_=0, trialReturnFloor_=0;
    bool ascendancyMenu_=false, trialMenu_=false;
    std::size_t ascendancySelection_=0;
    void onBossDefeated(const Monster& boss);
    void completeTrial(int trial);
    std::string trialAvailability(int trial) const; // empty when the trial can be entered
    bool enterTrial(int trial);
    bool leaveTrialState();
    void exitTrial();
    void openAscendancy();
    bool learnAscendancyNode(std::size_t node);
    void handleAscendancyKey(sf::Keyboard::Key key);
    void handleAscendancyMouse(const sf::Event& event);
    void renderAscendancy();
    void handleTrialMenuKey(sf::Keyboard::Key key);
    void handleTrialMenuMouse(const sf::Event& event);
    void renderTrialMenu();
    // Copies a cached floor's world (map, monsters, loot...) into `next`.
    static void importFloor(SaveGameState& next, const SaveGameState& floor);

    // Draws the AttributeAllocation screen: how many points remain,
    // and what each of Strength/Dexterity/Intelligence currently does
    // for this character. Same standalone-screen approach as every
    // other non-Playing mode.
    void renderAttributeAllocation();

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
    SaveGameState captureState(bool includeFloors=true);
    bool restoreState(const SaveGameState& state, bool includeFloors=true);
    std::map<int,SaveGameState> floorCache_;
    bool adventureMode_=false;
    int extraLives_=0;
    void reviveInTown();
    bool dungeonMenu_=false;
    int dungeonSelection_=0, dungeonDepth_=1;
    void renderDungeonSelection();
    void handleDungeonKey(sf::Keyboard::Key key);
    void handleDungeonMouse(const sf::Event& event);
    AIDecision enemyDecision(Monster& monster, Actor* opponent);
    void alertEnemyGroup(Monster& source, Position target);
    // ToME-style awareness: an enemy hunting you (alerted, not concealed)
    // stays shown out of sight until it loses track of you.
    bool sensedMonster(const Monster& m) const;
    // Landmark events that wake enemies: `count` monsters drawn from this
    // floor's own roster appear 3-7 steps from the altar, already hunting
    // the player. The first uses `firstTier`. Returns how many appeared.
    int spawnLandmarkFoes(int count, MonsterTier firstTier, MonsterTier restTier,
                          std::optional<MonsterType> firstType = std::nullopt);
    void landmarkReward(ItemRarity rarity);
    // Uniques: one the player doesn't already own when possible, into the
    // bag or onto the ground where a champion fell.
    const ItemDefinition& pickUnique();
    void grantUnique(std::optional<Position> ground);
    // A rare event's champion: Nightmare tier, half again its life.
    void raiseChampion(MonsterType type, int champion);
    void placeVampireLord();
    // A second and third event on a floor. The landmark nearest you is kept
    // in landmark_/landmarkAltar_/landmarkUsed_ (so every interaction works
    // as before); the others wait here and swap in as you approach.
    struct ExtraLandmark { LandmarkKind kind; Position altar; bool used; };
    std::vector<ExtraLandmark> extraLandmarks_;
    void swapLandmark(std::size_t index);
    void focusNearestLandmark();
    // The floor's events, announced on arrival (a banner over the map).
    std::string floorNotice_;
    sf::Clock floorNoticeClock_;
    std::size_t hintRows_ = 0; // map hints drawn this frame: the notice sits below them
    void announceFloor();
    // Roaming threats (ApplicationRoaming.cpp): patrols, a wandering
    // champion, and hunters that come for you if you linger on a floor.
    int floorTurns_ = 0;
    std::vector<Position> roamWaypoints() const;
    void planRoamers(unsigned seed);
    AIDecision roamStep(Monster& monster);
    std::optional<Position> flankStep(const Monster& monster, Position goal);
    void tickHunt();
    void tickWard();
    // The play screen (PlayLayout.hpp): as wide as the window's shape.
    sf::View playView_;
    float hudHintTop_ = 120.f;           // where map hints start, under the top-left cluster
    std::deque<float> logTimes_;         // when each log line was written, so old ones fade
    bool menuOpen() const;               // a 1280x720 menu is in front of the play screen
    // Wide screens dock a menu's two halves to the screen's edges: its left
    // part (design x below the split) to the left edge, the rest to the
    // right edge, with the play screen showing between. 0 = centred.
    float menuSplit() const;
    sf::Vector2f designFromPlay(sf::Vector2f p) const;
    void drawMenu(const std::function<void()>& draw);
    void beginMenu(std::uint8_t dim);
    void renderTownWings();
    // Esc: the pause menu (resume, options, save and exit). Options is a
    // placeholder for now.
    bool pauseMenu_ = false, pauseOptions_ = false;
    void openPause() { pauseMenu_ = true; pauseOptions_ = false; cancelTargeting(); }
    void handlePauseEvent(const sf::Event& event);
    void renderPause();
    sf::FloatRect pauseButton(int index) const;              // the town's street, on past the square on a wide screen    // dims the whole play screen, then draws in the menu's frame
    sf::FloatRect hotbarSlotRect(std::size_t slot) const;
    sf::FloatRect actionButtonRect(std::size_t index) const;
    sf::FloatRect cancelButtonRect() const;
    sf::FloatRect levelBadgeRect() const;
    std::vector<Decal> decals_; // what's left lying about in lived-in rooms
    std::vector<const ItemDefinition*> shopStock() const;
    void spawnHunters();
    static std::string wandererName(const Monster& monster); // "Ogre, the Wanderer": no tier prefix
    // Breach and Essence (ApplicationRifts.cpp).
    int breachTurns_ = 0;   // turns until the open breach closes; 0 = none open
    Position breachAt_{};
    int breachKills_ = 0;
    static std::string plainName(const Monster& monster);
    void dressEventMonster(Monster& monster);
    Captive essenceAt(Position altar) const;
    void releaseEssence();
    void touchLandmark();          // strongbox, breach, crystal: no menu, just what happens
    // How many times each crystal has been struck. Transient: a reload heals the cracks.
    std::vector<std::pair<Position, int>> crystalCracks_;
    int crystalHits(Position altar) const;
    void essenceStrike(const Monster& monster, AIDecision& decision) const;
    std::unique_ptr<Item> essenceItem(Essence essence, Position at);
    int breachRadius() const;
    void openBreach();
    void spawnRiftborn(int count, bool keeper);
    void tickBreach();
    void closeBreach(bool keeperSlain);
    void eventDeath(Monster& monster);
    void renderBreach();
    void drawBreachRift(sf::Vector2f at, float tile, sf::Color tint, bool active);
    void drawFrozenMonster(MonsterType type, sf::Vector2f at, float size, sf::Color tint);
    std::string landmarkLabel(LandmarkKind kind, Position altar) const;
    StrongboxVariant strongboxVariant(Position altar) const;
    int spawnAmbush(int count, int nightmares);      // foes burst out in a ring around you
    void spillLoot(ItemRarity rarity, int count, Position centre); // items scattered round a spot
    // Patron gods (ApplicationPatrons.cpp).
    Patron patron() const { return static_cast<Patron>(player_.patron); }
    bool patronBoon(Patron god) const { return patron() == god && player_.favor >= kFavorBoon; }
    void gainFavor(Patron god, int amount, const char* why);
    void patronWrath(Patron god);
    void swearTo(Patron god);
    void pray();
    Patron shrineGod() const;           // whose shrine this floor's Shrine is
    Patron godAt(Position altar) const; // whose shrine stands at this altar
    std::string landmarkTitle() const;  // the landmark's name, with its god for a shrine
    void judgeKill(const Monster& defeated);
    void judgeCast(const Talent& talent);            // the Blood Altar's sleeping guardian, and its blood pool
    bool vampireLordAlive() const;
    void scaleDungeonMonster(Monster& monster,int floor);
    Position floorEntrance_{}, floorExit_{};
    int gold_=0, quietTurns_=0, restTurns_=0;
    bool combatThisTurn_=false, exitMenu_=false, selling_=false;
    std::size_t shopSelection_=0;
    bool merchantOpen_=false; // the merchant list over the town square
    sf::Clock restClock_;
    bool autoExploring_=false;
    int exploreStepsLeft_=0;
    sf::Clock exploreClock_;
    std::vector<std::string> exploreSeenInterests_;
    void startAutoExplore();
    // Click-to-walk: auto-explore's stepping, aimed at one known tile. Returns
    // false when walking isn't possible right now (danger nearby), so the
    // caller can take a single step instead.
    bool startTravel(Position goal);
    std::optional<Position> travelGoal_;
    bool travelToAltar_ = false; // open the shrine on arriving beside its altar

    // This floor's landmark set piece (see world/Landmark.hpp) and its event.
    LandmarkKind landmark_ = LandmarkKind::None;
    Position landmarkAltar_{};
    bool landmarkUsed_ = false;
    bool shrineMenu_ = false;
    // Blocking props on this floor, and which one covers each tile (-1: none).
    std::vector<Prop> props_;
    std::vector<int> propAt_;
    void setProps(std::vector<Prop> props);
    int propIndexAt(int x, int y) const;
    bool nearAltar() const;
    void openShrine();
    struct LandmarkChoice { std::string name, icon, effect, cost; bool affordable; };
    std::vector<LandmarkChoice> landmarkChoices() const;
    sf::Color landmarkLightColor() const;
    void chooseBlessing(int choice);
    void handleShrineKey(sf::Keyboard::Key key);
    void handleShrineMouse(const sf::Event& event);
    void renderLandmark();
    void renderShrine();
    void stepAutoExplore();
    void stopAutoExplore(const char* reason);
    std::vector<std::string> visibleExploreInterests() const;
    bool dangerNearby() const;
    void recordQuietTurn();
    void startRest();
    void returnToTown(bool byStairs=false); // the stairs need no quiet turns, only no danger
    void travelFloor(int destination,bool fromTown=false,bool falling=false);
    bool cathedralOpen() const { return (player_.trialKeys & 1) != 0; } // the Warlord's sigil opens it
    bool interactStairs();
    void handleTownKey(sf::Keyboard::Key key);
    void handleTownMouse(const sf::Event& event);
    void renderTown();
    // Resolution independence: the game is laid out at 1280x720 and drawn
    // through uiView_, scaled evenly to fit the window with black bars
    // (letterboxing). Mouse events are converted into that layout before
    // anything handles them, so clicks land where they look.
    sf::View uiView_;
    bool fullscreen_ = false;
    void fitView();
    void toggleFullscreen();
    sf::FloatRect letterbox() const; // the game's area of the window, as a viewport fraction
    friend Element talentElement(const Talent& t);
    // Which music fits the current screen and floor (called from run()
    // only, so the UI tests stay silent).
    void updateMusic();
    sf::Clock musicClock_;
    // The town square (ApplicationTown.cpp): buildings drawn from the
    // dungeon art, lit at night, each one a click target.
    void renderTownSquare();
    void renderMerchant();
    std::optional<sf::RenderTexture> townLight_;
    void renderTravel();
    void handleTravelKey(sf::Keyboard::Key key);
    void handleTravelMouse(const sf::Event& event);

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
    void advanceEnemyIntents();
    float enemyStealthDetectionChance(const Actor& enemy) const;

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
    void executeAIDecision(Actor& actor, const AIDecision& decision, int chillMagnitude = 0);

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

    // The forked trees' delayed effects. Afterimage: tiles you blinked from,
    // bursting once the enemies have answered. Echo: a beam that fires again
    // down the same line when your next turn begins. Neither is saved.
    std::vector<Position> afterimages_;
    struct EchoBeam { Talent talent; Position from, cursor; TalentTarget target; };
    std::optional<EchoBeam> echo_;
    void burstAfterimages();
    // Blizzard: areas that keep striking as your turns begin.
    struct Storm { Talent talent; std::vector<Position> area; int turns = 0; };
    std::vector<Storm> storms_;
    void tickStorms();
    void shatterHoarfrost(const Actor& dead);
    void tremorAt(Position at);
    void echoHexes(const Actor& dead);
    // Soul Harvest: where cursed foes fell, raised once the dead are cleared away.
    std::vector<Position> risingSouls_;
    void raiseHarvestedSouls();
    void fireEcho();
    void spreadWildfire(const Actor& dead);

    // The death recap: the last blows the player took, oldest first, shown
    // on the death screen as plain facts. harmSource_ names whatever is
    // acting right now; noteHarm() turns any life lost since the last note
    // into an entry.
    struct Harm { int clock=0; std::string source; int amount=0, hp=0, maxHp=0; std::string state; };
    static constexpr std::size_t kRecapLength=8;
    std::deque<Harm> harms_;
    std::string harmSource_;
    int harmHp_=0, harmClock_=0;
    void noteHarm();
    void resetHarms() { harms_.clear(); harmSource_.clear(); harmHp_=player_.stats().hp; }
    std::string harmfulStates() const;

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
    ui::Kit ui_; // fonts, stone/bronze panels, icons, tooltips -- see UiKit.hpp
    SoundManager soundManager_; // Prompt 25 -- see SoundManager.hpp for the "sound is a
                                // presentation detail, never a hard requirement" design
    SpriteAtlas sprites_; // same policy: a missing sheet falls back to flat-colored squares
    sf::Clock animationClock_; // drives purely cosmetic animation (torch flicker)
    // Lighting (see renderLighting): a radial light texture and the map-sized
    // light map it is accumulated into, both created on first use.
    std::optional<sf::Texture> lightBlob_;
    std::optional<sf::RenderTexture> lightMap_;
    void renderLighting(const std::vector<std::pair<sf::Vector2f, sf::Color>>& lights);
    bool ensureLightBlob();
    void drawActorShadow(sf::Vector2f tileTopLeft);

    // Character animation (ApplicationAnimation.cpp): purely cosmetic and
    // time-based. Actors glide between tiles, face where they move or
    // strike, play idle/walk/attack rows, and dead monsters play their death
    // row where they fell. The camera slides instead of jumping.
    struct ActorAnim {
        bool initialised = false;
        Position lastTile{}, fromTile{};
        float moveStart = -10.f, attackStart = -10.f, phase = 0.f;
        bool faceLeft = false;
    };
    struct ActorPose { sf::Vector2f screen; int row = 0, frame = 0; bool flip = false; };
    struct Corpse { SpriteFrame base; sf::Color tint; Position tile; float start; bool faceLeft; int deathRow = 4, deathFrames = 10; float scale = 1.f; };
    std::unordered_map<const Actor*, ActorAnim> actorAnims_;
    std::vector<Corpse> corpses_;
    sf::Vector2f cameraShiftStart_{};
    float cameraShiftTime_ = -10.f;
    int previousCameraX_ = INT_MIN, previousCameraY_ = INT_MIN;
    float animNow() const;
    float animTimeOffset_ = 0; // lets tests freeze animation at a chosen moment
    sf::Vector2f cameraShift() const;
    void updateCameraShift();
    void notifyAttack(const Actor& actor, Position target);
    ActorPose actorPose(const Actor& actor);
    static SpriteFrame animatedFrame(const SpriteFrame& base, int row, int frame);
    void recordCorpse(const Monster& monster, const SpriteFrame& base, sf::Color tint, int deathRow = 4, int deathFrames = 10, float scale = 1.f);
    void forgetActor(const Actor& actor);
    void renderCorpses();

    // Combat effects (ApplicationEffects.cpp): cosmetic, time-based, in
    // tile space so they follow the camera. `from`/`to` are tile centres;
    // `radius` is in tiles (or a scale, for lightning).
    struct Vfx {
        enum class Kind { Bolt, Arrow, Lightning, Slash, Burst, Ring, Puff, Smoke, Pillar, Sparkle } kind;
        sf::Vector2f from, to;
        sf::Color color;
        float start = 0, duration = .3f, radius = 1.f;
        unsigned seed = 0;
    };
    std::vector<Vfx> vfx_;
    float vfxSlot_ = 0;          // when the next attack's effects may start, so a burst of turns plays in order
    bool suppressAttackVfx_ = false;
    std::unordered_map<const Actor*, float> hitFlash_;
    sf::Vector2f tileToScreen(sf::Vector2f tile) const;
    void spawnVfx(Vfx v, float delay = 0);
    void nextVfxSlot(float gap);
    void flashActor(const Actor& actor);
    void spawnTalentVfx(const Talent& talent, Position from, Position cursor, const TalentTarget& target);
    void spawnAttackVfx(const Actor& attacker, const Actor& target, bool magic, bool dodged);
    void spawnReleaseVfx(const EnemyIntent& intent);
    void addVfxLights(std::vector<std::pair<sf::Vector2f, sf::Color>>& lights);
    void renderVfx();
    void renderStatusVfx();
    void drawHitFlash(const Actor& actor, const SpriteFrame& frame, sf::Vector2f topLeft, float size, bool flip);
    void renderTelegraphs(int viewStartX, int viewStartY, int viewEndX, int viewEndY);

    // Darkness (Application.cpp, computeLight): which tiles are lit, by
    // wall torches, the stairs, landmarks, burning creatures and the
    // player's own light. The player sees lit tiles in sight and anything
    // adjacent; monsters without darkvision see the same way.
    bool darknessEnabled_ = true; // tests of other systems switch it off
    std::vector<std::uint8_t> litTiles_;
    // What each light can see, so the drawn light stops at walls instead of
    // bleeding into the next room: tile index and distance, per source.
    struct LightSource { sf::Color color; float radius; bool flicker; std::vector<std::pair<int, float>> tiles; };
    std::vector<LightSource> lightSources_;
    std::vector<Position> wallTorches_; // floor tiles lit by a wall torch on the wall above them
    void computeLight();
    bool tileLit(Position p) const;
    int playerLightRadius() const;
    bool canSee(const Actor& viewer, Position target) const; // light-wise only; line of sight is checked separately
    void toggleLight();

    // Surfaces and fixtures (ApplicationSurfaces.cpp, world/Surfaces.hpp).
    std::vector<SurfaceTile> surfaces_;
    struct LightOrb { Position at; int turns; };
    std::vector<LightOrb> lightOrbs_; // Conjure Light's wisps
    int regenTicks_ = 0;
    const Actor* momentumTarget_ = nullptr; // Duelist's Momentum
    int momentumStreak_ = 0;
    bool ascendancyChoice_ = false;          // choosing which ascendancy, after the first trial
    std::size_t ascendancyChoiceSelection_ = 0;
    void chooseAscendancy(const std::string& id);
    void renderAscendancyChoice();
    void renderLootBeams();
    std::set<std::pair<int, int>> torchToggles_; // wall torches whose lit state differs from the floor's default
    bool torchLit(int x, int y) const;
    void setTorchLit(int x, int y, bool lit);
    SurfaceType surfaceAt(Position p) const;
    void setSurface(Position p, SurfaceType type, int turns);
    void clearSurfaces();
    bool visibleTile(Position p) const;
    void applyElement(Element element, const std::vector<Position>& tiles);
    void electrify(const std::vector<Position>& seeds);
    void shockStanding(const std::vector<Position>& pool);
    void explodeOilBarrel(std::size_t index);
    bool knockOver(Position tile, Position direction, const Actor* kicker = nullptr);
    void tickSurfaces();
    bool hazardousSurface(Position p) const;
    void seedSurfaces(unsigned seed);
    bool bossSurfaceAction(Monster& boss);
    // Pushes (ApplicationSurfaces.cpp): into hazards, fixtures, walls, other
    // creatures and chasms. `direction` is a unit step.
    void pushActor(Actor& target, Position direction, int distance, const Actor& pusher);
    bool immovable(const Actor& actor) const;    // bosses, uniques and champions: can't be thrown, held or dropped
    void enterSurface(Actor& actor, Position tile); // the ground acts at once on a creature moved onto it
    // Brawling: throw an adjacent enemy over the player's shoulder; drag a
    // grappled enemy into the tile the player just left.
    void hurlActor(Actor& target, int distance, bool domino);
    void dragGrappled(Position vacated);
    // Shadow and Radiance: put out / relight every light around a spot.
    void snuffLights(Position centre, int radius);
    void explodeGas(Position tile);                        // poison gas meets fire
    // Traps (Traps, Saboteur): set on the ground, sprung by the first foe to
    // step there. Not saved: a reload clears them, like raised pillars.
    struct Trap { Position at; int kind; int turns; int radius; };
    std::vector<Trap> traps_;
    void triggerTrap(std::size_t index, Monster& victim, Position heading);
    void renderTraps();
    void addLightOrb(Position at, int turns);              // never more than the save allows
    void skirmishStrikes(Position from, Position to);       // Lunge and Pass Strike, as you step
    bool raisePillarAt(Position tile);                     // Earth: a stone pillar, for a while
    std::map<std::pair<int, int>, int> pillarTurns_;       // raised pillars' remaining turns (not saved: they crumble on reload)
    void relightLights(Position centre, int radius);
    int situationalBonus(const Talent& talent, const Actor& target) const; // Flay, Brews, Inner Light, Umbral
    bool dominoPush_ = false;
    // Enemy knockbacks. lastHitDodged_: whether the latest monster attack was
    // dodged (a dodged blow pushes nothing). pendingFall_: the player was
    // knocked into a chasm and drops to the next floor once the round ends.
    bool lastHitDodged_ = false, pendingFall_ = false;
    bool canFall() const;
    void fallToNextFloor();
    void carveChasms(std::mt19937& rng);
    void placeBraziers(Position centre, const std::vector<Position>& offsets);
    void renderSurfaces(std::vector<std::pair<sf::Vector2f, sf::Color>>& lights, int x0, int y0, int x1, int y1);
    void renderSurfaceGlow(int x0, int y0, int x1, int y1);
    GameMode mode_ = GameMode::ClassSelection;
    PlayerClass playerClass_ = PlayerClass::Spellblade; // meaningless until selectClass() runs
    bool wonGame_ = false; // meaningless unless mode_ == GameOver -- see checkAndHandleDeath

    // Which floor of the multi-floor dungeon progression the character
    // is currently on -- 1 through kFinalFloor (see Application.cpp).
    // Reset to 1 on a fresh selectClass(), restored from the save on
    // loadGame(). Every floor except the ones that should have a boss
    // fight generates with DungeonGenerationParams::includeBossRoom ==
    // false -- see regenerateLevel().
    int currentFloor_ = 1;

    // Set when the final-floor boss dies, instead of transitioning to
    // GameOver immediately -- that kill's own XP can trigger a level-up
    // with its own attribute-allocation (or hybrid-choice) pause, and
    // jumping straight to GameOver would silently skip it, discarding
    // earned points the player never got to spend. resumeLevelUpSequence()
    // checks this once every pending choice from the kill has actually
    // resolved, and only then makes the real mode_ transition.
    bool pendingFinalVictory_ = false;

    // Which boss was actually defeated on the final floor -- set
    // alongside pendingFinalVictory_, read by renderGameOver() once the
    // GameOver transition actually happens. Needed because the boss
    // Actor itself is already gone (boss_ = nullptr, and the underlying
    // object erased by removeDeadMonsters()) by the time the victory
    // screen renders; the name has to be captured at the moment of
    // death, not read back from the (by then nonexistent) boss.
    std::string defeatedBossName_;


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
    std::size_t logTotal_ = 0; // every message ever logged (the deque keeps only the latest)
    static constexpr std::size_t kMaxLogMessages = 60; // scrollback for the log panel

    // Direction of the player's last successful move -- Blink teleports
    // in this direction.
    Position lastMoveDirection_{0, -1};

    // Whose turn it currently is, per the scheduler.
    Actor* currentActor_ = nullptr;
};

} // namespace engine
