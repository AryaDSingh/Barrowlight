// Monster AI pieces not covered by chaser_test: StatusEffects/tickStatusEffects
// (poison damage, stun detection, expiration), and the Kiter/Support/
// AoEBomber decision logic. No SFML, no window, no Application.

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "ai/AoEBomber.hpp"
#include "ai/Kiter.hpp"
#include "ai/NullAIBehavior.hpp"
#include "ai/Support.hpp"
#include "entities/Monster.hpp"
#include "entities/Player.hpp"
#include "entities/StatusEffectLogic.hpp"
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
} // namespace

int main() {
    // --- StatusEffects / tickStatusEffects ---
    {
        Stats stats;
        stats.hp = 20;
        stats.maxHp = 20;
        Player dummy(Position{0, 0}, stats, TalentSet{});

        dummy.statusEffects().apply(StatusEffectInstance{StatusEffectType::Poison, 2, 3});

        bool stunned = tickStatusEffects(dummy);
        check(!stunned, "poison alone does not report stunned");
        check(dummy.stats().hp == 17, "poison deals its magnitude as damage on tick 1 (20 -> 17)");
        check(dummy.statusEffects().has(StatusEffectType::Poison),
              "poison still active after 1 of 2 ticks");

        tickStatusEffects(dummy);
        check(dummy.stats().hp == 14, "poison deals damage again on tick 2 (17 -> 14)");
        check(!dummy.statusEffects().has(StatusEffectType::Poison),
              "poison expires after exactly 2 ticks");

        tickStatusEffects(dummy);
        check(dummy.stats().hp == 14, "no further damage once poison has expired");
    }
    {
        Stats stats;
        stats.hp = 20;
        stats.maxHp = 20;
        Player dummy(Position{0, 0}, stats, TalentSet{});
        dummy.statusEffects().apply(StatusEffectInstance{StatusEffectType::Stun, 2, 0});

        check(tickStatusEffects(dummy), "stunned on tick 1 of 2");
        check(tickStatusEffects(dummy), "still stunned on tick 2 of 2");
        check(!tickStatusEffects(dummy), "no longer stunned after 2 ticks");
    }

    // --- Kiter ---
    {
        Map map = makeOpenRoom();
        MonsterAttackProfile profile;
        profile.power = 4;
        Kiter kiter(profile, /*attackRange=*/6, /*tooCloseRange=*/2);

        Monster self(MonsterType::Archer, "Archer", 'a', Position{2, 2}, Stats{}, std::make_unique<NullAIBehavior>());

        Player farTarget(Position{9, 2}, Stats{}, TalentSet{}); // distance 7 -- beyond attackRange(6), within sight(8)
        AIDecision decision = kiter.decideAction(self, map, farTarget, {});
        check(decision.type == AIActionType::Move && decision.movePosition.x == 3 &&
                  decision.movePosition.y == 2,
              "Kiter approaches when target is beyond attack range");

        Player midTarget(Position{6, 2}, Stats{}, TalentSet{}); // distance 4 -- within (2,6]
        decision = kiter.decideAction(self, map, midTarget, {});
        check(decision.type == AIActionType::Attack && decision.attackPower == 4,
              "Kiter attacks when target is within its band");

        Player closeTarget(Position{3, 2}, Stats{}, TalentSet{}); // distance 1 -- too close
        decision = kiter.decideAction(self, map, closeTarget, {});
        check(decision.type == AIActionType::Move && decision.movePosition.x == 1 &&
                  decision.movePosition.y == 2,
              "Kiter retreats away from the target when too close");
        decision = kiter.decideAction(self, map, closeTarget, {});
        check(decision.type == AIActionType::Move, "...and gives ground a second time");
        decision = kiter.decideAction(self, map, closeTarget, {});
        check(decision.type == AIActionType::Attack, "After two steps back, it holds its ground and shoots");
        decision = kiter.decideAction(self, map, midTarget, {});
        decision = kiter.decideAction(self, map, closeTarget, {});
        check(decision.type == AIActionType::Move, "Once you leave its personal space, it will give ground again");
    }

    // --- Support ---
    {
        std::vector<Talent> abilities;
        Talent empower;
        empower.name = "Empower";
        empower.cooldownTurns = 4;
        abilities.push_back(empower);

        Support support(/*buffMagnitude=*/4, /*buffDuration=*/4, /*buffRadius=*/4);
        Map map = makeOpenRoom();
        Player irrelevantPlayer(Position{0, 0}, Stats{}, TalentSet{});

        Monster self(MonsterType::Shaman, "Shaman", 'h', Position{5, 2}, Stats{}, std::make_unique<NullAIBehavior>(),
                     TalentSet(abilities));
        Monster unbuffedAlly(MonsterType::Goblin, "Goblin", 'g', Position{6, 2}, Stats{},
                              std::make_unique<NullAIBehavior>());

        std::vector<Actor*> allies{&unbuffedAlly};
        AIDecision decision = support.decideAction(self, map, irrelevantPlayer, allies);
        check(decision.type == AIActionType::UseAbility && decision.target == &unbuffedAlly &&
                  decision.effectToApply.has_value() &&
                  decision.effectToApply->type == StatusEffectType::Empowered,
              "Support buffs a nearby unbuffed ally");

        unbuffedAlly.statusEffects().apply(
            StatusEffectInstance{StatusEffectType::Empowered, 4, 4});
        decision = support.decideAction(self, map, irrelevantPlayer, allies);
        check(decision.type == AIActionType::Wait,
              "Support has nothing to do once its only ally is already buffed");
    }

    // --- AoEBomber ---
    {
        Map map = makeOpenRoom();
        AoEBomber bomber(/*blastPower=*/10, /*blastRange=*/4, /*tooCloseRange=*/2);
        Monster self(MonsterType::Bomber, "Bomber", 'b', Position{2, 2}, Stats{}, std::make_unique<NullAIBehavior>(),
                     TalentSet(std::vector<Talent>{Talent{"Blast", "", TalentTree::Blade,
                                                            TargetingMode::Self,
                                                            EffectShape::SingleTarget, 0, 0, 5}}));

        Player farTarget(Position{8, 2}, Stats{}, TalentSet{}); // distance 6 -- beyond blastRange(4), within sight(8)
        AIDecision decision = bomber.decideAction(self, map, farTarget, {});
        check(decision.type == AIActionType::Move, "AoEBomber approaches when target is too far");

        Player inRangeTarget(Position{5, 2}, Stats{}, TalentSet{}); // distance 3 -- within blastRange, off cooldown
        decision = bomber.decideAction(self, map, inRangeTarget, {});
        check(decision.type == AIActionType::UseAbility && decision.attackPower == 10,
              "AoEBomber blasts when in range and off cooldown");

        Player closeTarget(Position{3, 2}, Stats{}, TalentSet{}); // distance 1 -- too close
        decision = bomber.decideAction(self, map, closeTarget, {});
        check(decision.type == AIActionType::Move && decision.movePosition.x == 1,
              "AoEBomber retreats when the target is too close");
    }

    std::cout << "\n" << (g_allOk ? "All monster AI checks passed." : "Some checks FAILED.")
               << '\n';
    return g_allOk ? 0 : 1;
}
