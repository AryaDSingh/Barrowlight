#include "entities/MonsterInspection.hpp"

#include "entities/Monster.hpp"
#include "world/ExploredMap.hpp"

namespace engine {
std::vector<std::string> inspectMonster(const Monster& monster,
    const ExploredMap& vision, InspectionAccess access) {
    const auto p = monster.position();
    if (monster.stats().hp <= 0 || vision.at(p.x, p.y) != Visibility::Visible) return {};
    const auto& s = monster.stats();
    std::vector<std::string> lines{monster.name(),
        "HP " + std::to_string(s.hp) + "/" + std::to_string(s.maxHp) +
        "  Speed " + std::to_string(s.speed),
        "STR " + std::to_string(s.strength) + "  DEX " + std::to_string(s.dexterity) +
        "  INT " + std::to_string(s.intelligence)};
    for (const auto& effect : monster.statusEffects().active()) {
        const char* name = effect.type == StatusEffectType::Poison ? "Poison" :
            effect.type == StatusEffectType::Stun ? "Stun" : "Empowered";
        lines.push_back(std::string(name) + ": " + std::to_string(effect.turnsRemaining) +
            " enemy turns");
    }
    switch (monster.type()) {
        case MonsterType::Goblin:
            lines.push_back("Chases you. Melee strike."); break;
        case MonsterType::Spider:
            lines.push_back("Chases you. Venomous bite applies Poison."); break;
        case MonsterType::Ogre:
            lines.push_back("Chases you. Heavy melee can Stun."); break;
        case MonsterType::Archer:
            lines.push_back("Keeps distance. Ranged arrow attack."); break;
        case MonsterType::Shaman:
            lines.push_back("Supports nearby allies; does not attack."); break;
        case MonsterType::Bomber:
            lines.push_back("Keeps distance. Blasts a target area."); break;
        case MonsterType::GoblinWarlord:
            lines.push_back("Melee, ranged blast, then enraged melee.");
            lines.push_back("Enrage: empowers itself when badly hurt."); break;
        case MonsterType::Lich:
            lines.push_back("Keeps distance. Dark bolt and summons."); break;
        case MonsterType::Skeleton:
            lines.push_back("Summoned fighter. Chases and strikes."); break;
    }
    const auto& abilities = monster.talents().knownTalents();
    for (std::size_t i = 0; i < abilities.size(); ++i) {
        lines.push_back(abilities[i].name + ": " + abilities[i].description);
        if (access.revealCooldowns) {
            const int remaining = monster.talents().cooldownRemaining(i);
            lines.push_back(remaining == 0 ? "Cooldown: ready" :
                "Cooldown: " + std::to_string(remaining) + " enemy turns");
        }
    }
    if (!abilities.empty() && !access.revealCooldowns)
        lines.push_back("Remaining cooldowns: unknown");
    return lines;
}
} // namespace engine
