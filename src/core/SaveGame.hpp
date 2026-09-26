#pragma once

#include <optional>
#include <cstdint>
#include <string>
#include <vector>

#include "core/Position.hpp"
#include "entities/MonsterType.hpp"
#include "entities/PlayerClass.hpp"
#include "entities/Stats.hpp"
#include "entities/Item.hpp"
#include "entities/Rune.hpp"
#include "entities/MonsterTier.hpp"
#include "entities/StatusEffects.hpp"
#include "world/ExploredMap.hpp"
#include "world/Map.hpp"

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
        int location = -1; // -2 ground, -1 bag, 0 weapon, 1 armour, 2 charm
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
    struct TalentSaveData {
        std::string id;
        int cooldown = 0;
        bool operator==(const TalentSaveData& other) const { return id == other.id && cooldown == other.cooldown; }
    };
    std::vector<TalentSaveData> playerTalents; // learned order, identity and running cooldown
    std::vector<RuneInstance> runes;
    bool runeChoiceAvailable = false;
    std::vector<StatusEffectInstance> playerStatusEffects;
    Position lastMoveDirection;
    bool playerHybridSpecced = false;

    struct MonsterSaveData {
        MonsterType type; // reconstructed via MonsterFactory::createMonster()
        Position position;
        int hp = 0;
        int maxHp = 0;
        bool isBoss = false;
        MonsterTier tier = MonsterTier::Base;
        bool rewardsEligible = true;
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

// Reads a save file written by saveGame(). Returns std::nullopt (not an
// exception) if the file doesn't exist or is malformed -- "no valid
// save" is an expected, ordinary outcome (first run, deleted save file),
// not an exceptional one.
std::optional<SaveGameState> loadGame(const std::string& path);

} // namespace engine
