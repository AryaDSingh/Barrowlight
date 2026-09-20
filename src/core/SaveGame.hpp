#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/Position.hpp"
#include "entities/MonsterType.hpp"
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
    Stats playerStats;
    std::vector<int> playerCooldowns; // parallel to the Spellblade's talent list
    std::vector<StatusEffectInstance> playerStatusEffects;
    Position lastMoveDirection; // needed for Blink to resume correctly

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
