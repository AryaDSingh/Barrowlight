#pragma once

#include <string>
#include <vector>

namespace engine {
class Monster;
class ExploredMap;

// Future perception talents may grant access. Ordinary inspection never does.
struct InspectionAccess { bool revealCooldowns = false; };
std::vector<std::string> inspectMonster(const Monster& monster,
    const ExploredMap& vision, InspectionAccess access = {});
} // namespace engine
