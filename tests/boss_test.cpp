// Standalone sanity check for BossBehavior. No SFML, no window -- same
// pattern as the other tests. Verifies phase transitions (including that
// announcements fire exactly once per transition, not every turn) and
// each phase's decision logic against hand-computed expected values.

#include <iostream>
#include <memory>
#include <string>

#include "ai/BossBehavior.hpp"
#include "entities/Monster.hpp"
#include "entities/Player.hpp"
#include "entities/TalentSet.hpp"
#include "world/Map.hpp"

using namespace engine;

namespace {
bool g_allOk = true;

void check(bool condition, const std::string& description) {
    g_allOk &= condition;
    std::cout << (condition ? "[ok] " : "[FAIL] ") << description << '\n';
}

Map makeOpenRoom() {
    const std::vector<std::string> rows = {
        "###############",
        "#@............#",
        "#.............#",
        "#.............#",
        "###############",
    };
    Position unused;
    return parseAsciiMap(rows, unused);
}

// Matches MonsterFactory's real GoblinWarlord numbers exactly, so this
// test verifies the actual configured boss, not a simplified stand-in.
std::unique_ptr<Monster> makeBoss(Position pos, int hp) {
    MonsterAttackProfile meleeProfile;
    meleeProfile.power = 9;

    std::vector<Talent> abilities;
    Talent fury;
    fury.name = "Warlord's Fury";
    fury.cooldownTurns = 4;
    abilities.push_back(fury);

    Stats stats;
    stats.hp = hp;
    stats.maxHp = 90;

    return std::make_unique<Monster>(
        MonsterType::GoblinWarlord, "Goblin Warlord", 'W', pos, stats,
        std::make_unique<BossBehavior>(meleeProfile, /*blastPower=*/14, /*blastRange=*/5,
                                        /*tooCloseRange=*/2, /*enrageBonus=*/6),
        TalentSet(abilities));
}
} // namespace

int main() {
    Map map = makeOpenRoom();

    // Phase 1 (full hp): distant visible target -> approach, no announcement.
    {
        auto boss = makeBoss(Position{2, 2}, 90);
        Player target(Position{9, 2}, Stats{}); // distance 7, within default sight 10
        const AIDecision decision = boss->ai()->decideAction(*boss, map, target, {});
        check(decision.type == AIActionType::Move && decision.announcement.empty(),
              "Phase 1: approaches a distant visible target, no announcement");
    }

    // Phase 1: adjacent -> attacks with the melee profile's power.
    {
        auto boss = makeBoss(Position{2, 2}, 90);
        Player target(Position{3, 2}, Stats{});
        const AIDecision decision = boss->ai()->decideAction(*boss, map, target, {});
        check(decision.type == AIActionType::Attack && decision.attackPower == 9,
              "Phase 1: attacks for the melee profile's power (9) when adjacent");
    }

    // hp = 54 is exactly 60% of 90 -- the phase 2 threshold. First call
    // should announce the transition; the second call at the same hp
    // should not repeat it.
    {
        auto boss = makeBoss(Position{2, 2}, 54);
        Player target(Position{9, 2}, Stats{}); // distance 7, beyond blastRange(5)
        const AIDecision first = boss->ai()->decideAction(*boss, map, target, {});
        check(!first.announcement.empty(), "Phase 2: transition announced on first call at threshold");
        check(first.type == AIActionType::Move, "Phase 2: approaches when beyond blast range");

        const AIDecision second = boss->ai()->decideAction(*boss, map, target, {});
        check(second.announcement.empty(), "Phase 2: no repeated announcement on later calls");
    }

    // Phase 2: in blast range, ability ready -> UseAbility.
    {
        auto boss = makeBoss(Position{2, 2}, 50);
        Player target(Position{6, 2}, Stats{}); // distance 4, within blastRange(5)
        const AIDecision decision = boss->ai()->decideAction(*boss, map, target, {});
        check(decision.type == AIActionType::UseAbility && decision.attackPower == 14,
              "Phase 2: uses Warlord's Fury (power 14) when in range and ready");
    }

    // Phase 2: too close -> retreats.
    {
        auto boss = makeBoss(Position{2, 2}, 50);
        Player target(Position{3, 2}, Stats{}); // distance 1, within tooCloseRange(2)
        const AIDecision decision = boss->ai()->decideAction(*boss, map, target, {});
        check(decision.type == AIActionType::Move && decision.movePosition.x == 1,
              "Phase 2: retreats when the target is too close");
    }

    // hp = 27 is exactly 30% of 90 -- the phase 3 threshold. First call
    // should self-empower (not act) and announce; second call should
    // fall back to melee-approach behavior.
    {
        auto boss = makeBoss(Position{2, 2}, 27);
        Player target(Position{9, 2}, Stats{});
        const AIDecision first = boss->ai()->decideAction(*boss, map, target, {});
        check(first.type == AIActionType::SelfBuff && first.effectToApply.has_value() &&
                  first.effectToApply->type == StatusEffectType::Empowered &&
                  first.effectToApply->magnitude == 6,
              "Phase 3: first call self-empowers (magnitude 6) instead of acting");
        check(!first.announcement.empty(), "Phase 3: transition announced");

        const AIDecision second = boss->ai()->decideAction(*boss, map, target, {});
        check(second.type == AIActionType::Move,
              "Phase 3: after enraging, approaches like phase 1");
    }

    std::cout << "\n" << (g_allOk ? "All boss behavior checks passed." : "Some checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
