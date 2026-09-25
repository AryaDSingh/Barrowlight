#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/Position.hpp"
#include "entities/MonsterType.hpp"
#include "entities/PlayerClass.hpp"
#include "entities/Stats.hpp"
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
// phase and re-applies its one-time buff once more -- harmless, since
// StatusEffects::apply() refreshes rather than stacks).
struct SaveGameState {
    Map map;
    ExploredMap exploredMap;

    Position playerPosition;
    PlayerClass playerClass = PlayerClass::Spellblade; // which kit playerCooldowns belongs to
    int playerLevel = 1; // Prompt 20
    int playerXp = 0;    // progress toward the *next* level, not a cumulative lifetime total
    int currentFloor = 1; // which floor of the multi-floor dungeon progression -- the
                           // dungeon layout itself is never saved (map/monsters below
                           // already capture whatever floor the player was actually on when
                           // they saved), but the floor *number* is needed on load so
                           // subsequent door transitions and boss-floor gating pick up
                           // correctly rather than silently resetting to floor 1
                          // -- see PlayerLeveling.hpp
    Stats playerStats;
    std::vector<int> playerCooldowns; // parallel to player_.talents().knownTalents() at save
                                       // time -- NOT just playerClass's default starting kit;
                                       // see playerHybridPickNames below for why that list can
                                       // be longer than the default
    std::vector<StatusEffectInstance> playerStatusEffects;
    Position lastMoveDirection; // needed for Blink to resume correctly

    // Prompt 24 (fixing a real gap found while building it): which
    // hybrid-pool talents (by name) have been learned. Deliberately NOT
    // storing the base class's own level-4/level-7 unlocks here too --
    // those are fully deterministic from playerLevel (loadGame() just
    // re-checks talentUnlockedAtLevel() against the saved level), so
    // saving them again would be redundant state that could drift out
    // of sync. Hybrid picks are different: which specific abilities
    // were chosen is a real, non-derivable player decision, so it's the
    // one piece of "extra known talents" that actually has to be saved
    // explicitly. Names, not full Talent structs -- loadGame() looks
    // each name up against HybridSpec::fullKitForClass() to find the
    // matching talent, the same "save the minimum that's a genuine
    // choice, re-derive everything else" approach monster tier
    // reconstruction already uses. Spaces in a name (e.g. "Arcane
    // Bolt") are written as underscores on disk -- see SaveGame.cpp --
    // since no talent name in this project ever contains one naturally,
    // so the round-trip is unambiguous.
    std::vector<std::string> playerHybridPickNames;
    bool playerHybridSpecced = false;

    struct MonsterSaveData {
        MonsterType type; // reconstructed via MonsterFactory::createMonster()
        Position position;
        int hp = 0;
        int maxHp = 0;
        bool isBoss = false;
        std::vector<StatusEffectInstance> statusEffects;
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
