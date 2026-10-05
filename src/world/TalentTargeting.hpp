#pragma once

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "core/Position.hpp"

namespace engine {
class Actor;
class Map;
class ExploredMap;
struct Talent;

// Ephemeral result: pointers are used only during the current input/render call.
// Resolving never spends resources, advances time, rolls RNG, or changes actors.
struct TalentTarget {
    bool valid = false;
    std::string message;
    std::vector<Position> path;
    std::vector<Position> area;
    std::vector<Position> movementPath;
    std::vector<Position> chainPath;
    Actor* chainedTarget = nullptr;
    std::vector<Actor*> chainedMore; // further jumps (rank 3 Chain Lightning)
    bool chained(const Actor* a) const { return a && (a == chainedTarget || std::find(chainedMore.begin(), chainedMore.end(), a) != chainedMore.end()); }
    std::vector<Actor*> affected;
    Position destination;
    std::optional<Position> blockedAt;
};

TalentTarget resolveTalentTarget(const Map& map, const ExploredMap& vision,
    Actor& caster, const std::vector<Actor*>& enemies, const Talent& talent,
    Position cursor);

// Shared preflight for UI and casting. An empty string means affordable/ready.
std::string talentUnavailableReason(const Actor& caster, std::size_t index);
} // namespace engine
