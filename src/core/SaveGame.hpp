#pragma once

#include <tuple>

#include <optional>
#include <cstdint>
#include <string>
#include <array>
#include <vector>

#include "core/Position.hpp"
#include "entities/MonsterType.hpp"
#include "entities/EnemyIntent.hpp"
#include "entities/EnemyTactics.hpp"
#include "entities/PlayerClass.hpp"
#include "entities/Stats.hpp"
#include "entities/Item.hpp"
#include "entities/DungeonProgression.hpp"
#include "entities/Player.hpp"
#include "entities/MonsterTier.hpp"
#include "entities/StatusEffects.hpp"
#include "world/ExploredMap.hpp"
#include "world/Map.hpp"
#include "world/Props.hpp"

namespace engine {

// Everything needed to faithfully resume a game in progress.
//
// Deliberately does NOT include TurnScheduler's exact energy levels or
// any AIBehavior-internal state (e.g. the boss's one-time enrage
// trigger, or phase-announcement tracking) -- both are designed to be
// self-correcting/re-derivable: turn order re-settles within a few turns
// regardless of exact energy values, and BossBehavior's phase is
// re-derived from hp fraction on every single decideAction() call, not
// tracked as state that could go stale. Serializing either would add
// real complexity for a difference nobody would notice in play (at
// worst, a loaded boss already past a phase threshold re-announces that
// phase and refreshes its one-time buff). Talent cooldowns DO persist by ID,
// as do summon reward eligibility and exact monster tier. Other AI-internal
// counters remain a limitation of the current save format.
struct SaveGameState {
    bool adventureMode=false;
    int extraLives=0;
    DungeonLevels dungeonLevels{}; // Legacy version-19 entry levels; ignored by gameplay, cleared on migration.
    // Inactive floor snapshots contain no further snapshots. Their character
    // fields are archival only; travel restores world fields, never character loot.
    std::vector<SaveGameState> savedFloors;
    Position floorEntrance{}, floorExit{};
    bool inTown=false;
    int gold=0, quietTurns=0;
    int floorTurns=0; // turns spent on this floor (the hunt); format 37
    int breachTurns=0, breachKills=0; Position breachAt{}; // an open breach; format 38
    bool bloodRelic=false, animationRelic=false;
    std::vector<int> deathlessSpentFloors;
    Map map;
    ExploredMap exploredMap;

    Position playerPosition;
    PlayerClass playerClass = PlayerClass::Spellblade; // starting class; learned talents persist by ID
    int playerLevel = 1; // Prompt 20
    int playerXp = 0;    // progress toward the *next* level, not a cumulative lifetime total
    int currentFloor = 1; // which floor of the multi-floor dungeon progression -- the
                           // dungeon layout itself is never saved (map/monsters below
                           // already capture whatever floor the player was actually on when
                           // they saved), but the floor *number* is needed on load so
                           // subsequent door transitions and boss-floor gating pick up
                           // correctly rather than silently resetting to floor 1
                          // -- see PlayerLeveling.hpp
    // Base maxima/attributes, but CURRENT hp/mana (which may exceed base maxima
    // while equipped). Rebuild equipment before restoring these current pools.
    Stats playerStats;
    int unspentAttributePoints = 0;
    struct ItemSaveData {
        std::string definitionId;
        std::uint64_t instanceId = 0;
        int location = -1; // -2 ground, -1 bag; nonnegative values are persistent EquipmentSlot IDs
        Position position;
        int rollTier = 0;
        std::vector<RolledAffix> affixes;
    };
    std::vector<ItemSaveData> items;
    std::uint64_t nextItemId = 1;
    std::uint64_t lootRngState = 1;
    Position chestPosition;
    bool chestExists = false, chestClaimed = false;
    int ordinaryDrops = 0;
    bool vaultExists=false, vaultOpened=false, vaultClaimed=false;
    Position vaultCenter{}, vaultEntrance{};
    // The floor's landmark set piece (LandmarkKind as an int), its solid
    // altar tile and whether its event has been used. Version 23+.
    int landmark=0;
    Position landmarkAltar{};
    bool landmarkUsed=false;
    // Blocking props (version 24+). Their tiles are saved as walls; loading
    // makes them see-through again.
    std::vector<Prop> props;
    // Ascendancy and trials (format 26): see entities/Ascendancy.hpp.
    std::string ascendancy;
    int ascendancyPoints=0, trialKeys=0, trialsCleared=0;
    int trial=0, trialReturnFloor=0;
    int lightSource=1; bool lightLit=true; // format 28
    bool bloodMagicUnlocked=false;         // format 33
    int patron=0, favor=0;                 // format 34
    std::vector<std::array<int, 4>> extraLandmarks; // format 36: kind, altar x, altar y, used
    // Format 29: ground surfaces (x, y, type, turns) and wall torches lit or put out.
    std::vector<std::tuple<int,int,int,int>> surfaces;
    std::vector<Position> torchToggles;
    std::vector<std::tuple<int,int,int>> lightOrbs; // format 30: Conjure Light's wisps (x, y, turns) // inside a trial arena: which, and the dungeon floor to return to
    struct TalentSaveData {
        std::string id;
        int cooldown = 0;
        int rank = 1;
        bool operator==(const TalentSaveData& other) const { return id == other.id && cooldown == other.cooldown && rank == other.rank; }
    };
    std::vector<TalentSaveData> playerTalents; // learned order, identity and running cooldown
    int treePoints=1, abilityPoints=4;
    std::vector<Player::TreeAccess> trees;
    std::vector<std::string> hotbar;
    bool progressionReviewPending=false, pendingFinalVictory=false;
    std::string defeatedBossName;
    std::vector<StatusEffectInstance> playerStatusEffects;
    Position lastMoveDirection;

    struct MonsterSaveData {
        EnemyTactics tactics;
        MonsterType type; // reconstructed via MonsterFactory::createMonster()
        Position position;
        int hp = 0;
        int maxHp = 0;
        bool isBoss = false;
        MonsterTier tier = MonsterTier::Base;
        bool rewardsEligible = true;
        bool allied=false;
        int summonRank=1, summonIntelligence=0, remainingLife=0;
        bool vaultGuard = false;
        int eventChampion = 0;
        int roam = 0; // Roam; format 37
        int essence = 0, rift = 0; bool corrupted = false; // format 38
        int recoveryActions=0, summonsCommitted=0, announcedPhase=1;
        bool enraged=false;
        std::optional<EnemyIntent> intent;
        std::vector<StatusEffectInstance> statusEffects;
        std::vector<TalentSaveData> talents;
    };
    std::vector<MonsterSaveData> monsters;
};

// Writes `state` to `path` in a simple, hand-rolled text format --
// deliberately not JSON or a binary format: this project has exactly one
// external dependency (SFML) by deliberate choice throughout, and this
// code is the only reader or writer of its own format, so a
// general-purpose serialization library would be new build complexity
// for no real benefit. Returns false on any I/O failure.
bool saveGame(const SaveGameState& state, const std::string& path);
// Writes an older format's layout (for migration tests); fields newer than
// `version` are left out exactly as that version's files lacked them.
bool saveGameAsVersion(const SaveGameState& state, const std::string& path, int version);

// Reads a save file written by saveGame(). Returns std::nullopt (not an
// exception) if the file doesn't exist or is malformed -- "no valid
// save" is an expected, ordinary outcome (first run, deleted save file),
// not an exceptional one.
std::optional<SaveGameState> loadGame(const std::string& path);

} // namespace engine
