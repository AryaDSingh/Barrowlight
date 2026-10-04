#include "entities/MonsterInspection.hpp"

#include "entities/Monster.hpp"
#include "world/ExploredMap.hpp"
#include "world/Landmark.hpp"

namespace engine {
std::vector<std::string> inspectMonster(const Monster& monster,
    const ExploredMap& vision, InspectionAccess access) {
    const auto p = monster.position();
    if (monster.tactics.concealed || monster.stats().hp <= 0 || vision.at(p.x, p.y) != Visibility::Visible) return {};
    const auto& s = monster.stats();
    std::vector<std::string> lines{monster.name(),
        "HP " + std::to_string(s.hp) + "/" + std::to_string(s.maxHp) +
        "  Speed " + std::to_string(s.speed),
        "STR " + std::to_string(s.strength) + "  DEX " + std::to_string(s.dexterity) +
        "  INT " + std::to_string(s.intelligence)};
    // Only what you can see: no behaviour notes, multipliers or timings.
    // How it fights, you learn by fighting it.
    for (const auto& effect : monster.statusEffects().active())
        lines.push_back(std::string(statusName(effect.type)) + ": " + std::to_string(effect.turnsRemaining) + " turns");
    const auto& abilities = monster.talents().knownTalents();
    for (std::size_t i = 0; i < abilities.size(); ++i) {
        lines.push_back(abilities[i].name);
        if (access.revealCooldowns) {
            const int remaining = monster.talents().cooldownRemaining(i);
            lines.push_back(remaining == 0 ? "Cooldown: ready" :
                "Cooldown: " + std::to_string(remaining) + " enemy turns");
        }
    }
    return lines;
}
} // namespace engine
