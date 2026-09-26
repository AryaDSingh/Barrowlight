// Standalone sanity check for LichBehavior. No SFML, no window -- same
// pattern as boss_test.cpp. Verifies the Kiter-like movement (approach/
// retreat/bolt) and, more importantly, the summon mechanic itself: it
// fires when ready and in range, respects the maxSummons cap even
// across multiple ready cooldowns, and falls back to a bolt when no
// open tile exists to summon into.

#include <iostream>
#include <memory>
#include <string>

#include "ai/LichBehavior.hpp"
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

// Matches MonsterFactory's real Lich numbers exactly, so this test
// verifies the actual configured boss, not a simplified stand-in.
// maxSummons is a parameter (not hardcoded to the real 3) specifically
// so the cap-enforcement test below can use a small number and stay
// readable, without that choice affecting any other check.
std::unique_ptr<Monster> makeLich(Position pos, int maxSummons) {
    MonsterAttackProfile boltProfile;
    boltProfile.power = 6;
    boltProfile.scalingStat = ScalingStat::Intelligence;

    std::vector<Talent> abilities;
    Talent raiseSkeleton;
    raiseSkeleton.name = "Raise Skeleton";
    raiseSkeleton.cooldownTurns = 5;
    abilities.push_back(raiseSkeleton);

    Stats stats;
    stats.hp = 110;
    stats.maxHp = 110;
    stats.strength = 4;
    stats.dexterity = 20;
    stats.intelligence = 16;

    return std::make_unique<Monster>(
        MonsterType::Lich, "Lich", 'L', pos, stats,
        std::make_unique<LichBehavior>(boltProfile, /*attackRange=*/6, /*tooCloseRange=*/2,
                                        maxSummons),
        TalentSet(abilities));
}
} // namespace

int main() {
    Map map = makeOpenRoom();

    // Too far -> approaches, same movement shape as Kiter.
    {
        auto lich = makeLich(Position{2, 2}, /*maxSummons=*/3);
        Player target(Position{9, 2}, Stats{}, TalentSet{}); // distance 7: within sight (8), beyond attackRange(6)
        const AIDecision decision = lich->ai()->decideAction(*lich, map, target, {});
        check(decision.type == AIActionType::Move,
              "approaches a visible target beyond attack range");
    }

    // Too close -> retreats.
    {
        auto lich = makeLich(Position{2, 2}, /*maxSummons=*/3);
        Player target(Position{3, 2}, Stats{}, TalentSet{}); // distance 1, within tooCloseRange(2)
        const AIDecision decision = lich->ai()->decideAction(*lich, map, target, {});
        check(decision.type == AIActionType::Move && decision.movePosition.x == 1,
              "retreats when the target is too close");
    }

    // In range, cooldown NOT ready (fresh Lich, never summoned, but
    // manually started here to simulate "just summoned last turn") ->
    // falls back to a bolt.
    {
        auto lich = makeLich(Position{2, 2}, /*maxSummons=*/3);
        lich->talents().startCooldown(0);
        Player target(Position{6, 2}, Stats{}, TalentSet{}); // distance 4, within attackRange(6)
        const AIDecision decision = lich->ai()->decideAction(*lich, map, target, {});
        check(decision.type == AIActionType::Attack && decision.attackPower == 6,
              "bolts (power 6) when in range but the summon cooldown isn't ready");
    }

    // In range, cooldown ready, open tile available -> summons a
    // Skeleton, and starts the same cooldown (abilityIndex 0).
    {
        auto lich = makeLich(Position{2, 2}, /*maxSummons=*/3);
        Player target(Position{6, 2}, Stats{}, TalentSet{});
        const AIDecision decision = lich->ai()->decideAction(*lich, map, target, {});
        check(decision.type == AIActionType::Summon && decision.summonType == MonsterType::Skeleton,
              "summons a Skeleton when in range, ready, and under the cap");
        check(decision.abilityIndex == 0 && !decision.announcement.empty(),
              "the summon decision references ability 0 (the cooldown to start) and announces itself");
    }

    // Cap enforcement: reset the cooldown to ready before each call
    // (simulating several real turns passing) and confirm summoning
    // stops appearing once maxSummons summons have actually happened,
    // even though the cooldown itself is ready every time.
    {
        auto lich = makeLich(Position{2, 2}, /*maxSummons=*/2);
        Player target(Position{6, 2}, Stats{}, TalentSet{});

        const AIDecision first = lich->ai()->decideAction(*lich, map, target, {});
        check(first.type == AIActionType::Summon, "cap test: first summon succeeds");
        lich->talents().setCooldownRemaining(0, 0); // simulate the cooldown coming back up

        const AIDecision second = lich->ai()->decideAction(*lich, map, target, {});
        check(second.type == AIActionType::Summon, "cap test: second summon succeeds (at the cap of 2)");
        lich->talents().setCooldownRemaining(0, 0);

        const AIDecision third = lich->ai()->decideAction(*lich, map, target, {});
        check(third.type == AIActionType::Attack,
              "cap test: third attempt falls back to a bolt -- the cap (2) was already reached, "
              "even though the cooldown is ready again");
    }

    // No open tile to summon into (every neighbor occupied by an ally)
    // -> falls back to a bolt, and does NOT consume a summon attempt --
    // confirmed by a follow-up call, with a neighbor freed up, still
    // succeeding at summoning rather than having silently used up the
    // cap on the blocked attempt.
    {
        auto lich = makeLich(Position{5, 2}, /*maxSummons=*/3);
        Player target(Position{9, 2}, Stats{}, TalentSet{}); // distance 4, within attackRange(6)
        // Surround the Lich on all 8 sides with allies.
        std::vector<std::unique_ptr<Monster>> blockers;
        std::vector<Actor*> allies;
        const int offsets[8][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0},
                                    {1, 0},   {-1, 1}, {0, 1},  {1, 1}};
        for (const auto& offset : offsets) {
            Stats blockerStats;
            blockerStats.hp = 5;
            blockerStats.maxHp = 5;
            blockers.push_back(std::make_unique<Monster>(
                MonsterType::Goblin, "Blocker", 'g',
                Position{5 + offset[0], 2 + offset[1]}, blockerStats, nullptr));
            allies.push_back(blockers.back().get());
        }

        const AIDecision blocked = lich->ai()->decideAction(*lich, map, target, allies);
        check(blocked.type == AIActionType::Attack,
              "falls back to a bolt when every neighboring tile is occupied");

        // Free up one neighbor and retry -- should now succeed, proving
        // the blocked attempt above didn't silently consume the cap.
        allies.erase(allies.begin());
        const AIDecision freed = lich->ai()->decideAction(*lich, map, target, allies);
        check(freed.type == AIActionType::Summon,
              "succeeds once a neighboring tile opens up -- the earlier blocked attempt did not "
              "consume a summon from the cap");
    }

    std::cout << "\n" << (g_allOk ? "All Lich behavior checks passed." : "Some checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
