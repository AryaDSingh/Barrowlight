#include "entities/MonsterInspection.hpp"

#include "entities/Monster.hpp"
#include "world/ExploredMap.hpp"

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
    if(monster.tactics.retreat>0) lines.push_back("Will retreat: seeking nearby cover.");
    else if(monster.tactics.alert>0) lines.push_back("Alert: fighting or investigating the last sighting.");
    else if(!monster.vaultGuard) lines.push_back("Patrolling a short local route.");
    if(enemyHealer(monster.type())) lines.push_back("Healing casts left: "+std::to_string(monster.tactics.heals));
    if(enemyTank(monster.type())) lines.push_back("Frontline: advances while nearby ranged allies stay behind.");
    if (monster.tier()==MonsterTier::Elite)
        lines.push_back("Elite: 1.5x base HP, 1.4x damage, 2x XP; guaranteed magic-or-better gear.");
    else if (monster.tier()==MonsterTier::Nightmare)
        lines.push_back("Rare: 2.2x base HP, 1.8x damage, 4x XP; guaranteed rare gear.");
    if (isUniqueMonster(monster.type())) lines.push_back("Unique encounter: guaranteed rare gear; does not seal the stairs.");
    if (monster.recoveryActions>0) lines.push_back("Recovering: cannot act until you complete one action.");
    if (monster.type()==MonsterType::Lich) {
        lines.push_back("Summon attempts left: "+std::to_string(3-monster.summonsCommitted)+" (no replacements).");
        lines.push_back("Every few turns it breathes out the light: nearby torches, braziers and wisps die, and you are Smothered: pitch black, your light can't burn for 4 turns. Fire spells can still light braziers and oil.");
        lines.push_back(monster.flooded ? "It has flooded its sanctum; its bolts freeze the water you stand in." :
            "Below 60% life it floods its sanctum, and its bolts freeze water.");
    }
    if (monster.type()==MonsterType::GoblinWarlord)
        lines.push_back("Kicks lit braziers at you when one is beside it; its Fury scorches the ground it strikes.");
    if (monster.intent()) {
        const auto& intent=*monster.intent();
        lines.push_back("Committed target: ("+std::to_string(intent.target.x)+","+std::to_string(intent.target.y)+
            ") | "+std::to_string(intent.playerActionsRemaining)+" player actions before release.");
        lines.push_back(intent.kind==IntentKind::Summon ? "Ritual: occupy the purple tile or interrupt the Lich." : "Strike: leave the marked tiles, stun or push to interrupt. Hits other enemies too.");
    }
    for (const auto& effect : monster.statusEffects().active()) {
        const char* name=statusName(effect.type);
        lines.push_back(std::string(name) + ": " + std::to_string(effect.turnsRemaining) + " enemy turns" +
            (effect.type==StatusEffectType::Marked ? " | 1 hit, +25% damage" : ""));
    }
    lines.push_back("Stun: max "+std::to_string(monster.statusEffects().maxStunDuration())+" turn(s); then "+
        std::to_string(monster.statusEffects().stunRecoveryTurns())+" action(s) immune. Cannot refresh an active stun.");
    switch (monster.type()) {
        case MonsterType::Goblin:
            lines.push_back("Chases you. Melee strike."); break;
        case MonsterType::Spider:
            lines.push_back("Chases you. Venomous bite applies Poison."); break;
        case MonsterType::Ogre:
            lines.push_back("Melee; 35% of attacks wind up a Stun Slam. Red tile, 1 action to escape."); break;
        case MonsterType::Archer:
            lines.push_back("Keeps distance. Ranged arrow attack."); break;
        case MonsterType::Shaman:
            lines.push_back("Supports nearby allies; does not attack."); break;
        case MonsterType::Bomber:
            lines.push_back("Blast: radius-2 diamond, 3 actions to escape. Does not track you."); break;
        case MonsterType::GoblinWarlord:
            lines.push_back("Melee, ranged blast, then enraged melee.");
            lines.push_back("Below 60%: radius-2 Blast (3 actions), advances between casts. Below 30%: radius-1 cleave (2 actions).");
            lines.push_back("After heavy attacks: one player action of recovery."); break;
        case MonsterType::Lich:
            lines.push_back("Bolt: 1 action to dodge. Ritual: 2 actions to block or interrupt.");
            lines.push_back("Hex (range 6): Mana Drain while healthy; Doom below 60% HP. C: Cleanse. Breaking sight prevents new curses.");
            lines.push_back("Three rituals: Guard, Archer, Guard. Interruptions spend attempts. Summons give no XP/loot."); break;
        case MonsterType::GoblinRaider:
            lines.push_back("Melee applies Marked: the next direct hit deals +25% damage."); break;
        case MonsterType::GoblinCaptain:
            lines.push_back("Grik marks prey in melee so his pack can exploit the next hit."); break;
        case MonsterType::SkeletonArcher:
            lines.push_back("Keeps distance. Arrows Chill for 2 turns (20%); avoid being pinned beside guards."); break;
        case MonsterType::SkeletonGuard:
            lines.push_back("Heavy cleave: radius-1 diamond, 2 actions to escape; then 1 action recovery."); break;
        case MonsterType::Bonecaller:
            lines.push_back("Grave Rally empowers nearby allies. Does not attack or summon replacements."); break;
        case MonsterType::OssuaryWarden:
            lines.push_back("Veyra casts Ashfall: radius-2 diamond, 3 actions to escape; hits other enemies."); break;
        case MonsterType::GoblinBulwark: case MonsterType::CryptSentinel:
            lines.push_back("Durable frontline fighter. Screens support units; retreats once below 30% HP."); break;
        case MonsterType::GoblinMedic: case MonsterType::GraveMender:
            lines.push_back("Heals wounded allies in sight. Kill or separate it from the group."); break;
        case MonsterType::GoblinStalker: case MonsterType::CryptShade:
            lines.push_back("Hidden approach. Reveals within 3 tiles and spends an action before attacking."); break;
        case MonsterType::GoblinSlinger:
            lines.push_back("Kites at range; stones have a 50% chance to Mark for 2 turns."); break;
        case MonsterType::FrostAcolyte:
            lines.push_back("Kites at range. Frost bolts Chill for 2 turns (20%)."); break;
        case MonsterType::Skeleton:
            lines.push_back("Undead fighter. Chases and strikes."); break;
        case MonsterType::Torchbearer:
            lines.push_back("Carries a torch that lights the ground around it. Rekindles torches and braziers, ignites oil, and its blows may set you burning."); break;
        case MonsterType::Gloomstalker:
            lines.push_back("Strikes far harder from darkness. In light it is weakened and seared each turn: bring a torch."); break;
        case MonsterType::OrcFirebrand:
            lines.push_back("Kites at range, hurling flasks that burst into burning oil where they land."); break;
        case MonsterType::DrownedOne:
            lines.push_back("Leaves water where it walks and heals while standing in it. Lightning runs through the puddles it leaves."); break;
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
