// Standalone sanity check for PlayerLeveling -- hand-computed expected
// results, no SFML, no window, no Application.

#include <iostream>
#include <string>

#include "entities/PlayerLeveling.hpp"

using namespace engine;

namespace {
bool g_allOk = true;

void check(bool condition, const std::string& description) {
    g_allOk &= condition;
    std::cout << (condition ? "[ok] " : "[FAIL] ") << description << '\n';
}

Player makePlayer() {
    Stats stats;
    stats.hp = 30;
    stats.maxHp = 30;
    stats.mana = 20;
    stats.maxMana = 20;
    return Player(Position{0, 0}, stats, TalentSet{});
}
} // namespace

int main() {
    // --- xpForNextLevel: 20 * currentLevel.
    check(xpForNextLevel(1) == 20, "xpForNextLevel(1) == 20");
    check(xpForNextLevel(2) == 40, "xpForNextLevel(2) == 40");
    check(xpForNextLevel(9) == 180, "xpForNextLevel(9) == 180 (the step from 9 to 10)");

    // --- grantXp: a small grant that doesn't cross a threshold.
    {
        Player player = makePlayer();
        grantXp(player, 5);
        check(player.level() == 1, "5 XP (below the 20 needed) doesn't level up");
        check(player.xp() == 5, "5 XP is tracked as progress toward level 2");
        check(player.stats().maxHp == 30, "maxHp unchanged without a level-up");
        check(player.unspentAttributePoints() == 0, "no attribute points without a level-up");
    }

    // --- grantXp: exactly enough for one level-up. Rewritten for the
    // new leveling rules: +1 maxHp automatically (not +3), +2 unspent
    // attribute points (a player choice, not applied automatically),
    // and maxMana does NOT grow automatically at all anymore -- it only
    // grows from Intelligence points the player actually chooses to
    // spend via the AttributeAllocation screen, which this test never
    // does.
    {
        Player player = makePlayer();
        grantXp(player, 20);
        check(player.level() == 2, "exactly 20 XP triggers the level 1 -> 2 level-up");
        check(player.xp() == 0, "no leftover XP after an exact-threshold grant");
        check(player.stats().maxHp == 31, "maxHp grew by 1 on level-up (30 -> 31)");
        check(player.stats().maxMana == 20,
              "maxMana does NOT grow automatically -- only allocated Intelligence points do that");
        check(player.unspentAttributePoints() == 2,
              "one level-up grants 2 unspent attribute points to allocate");
        check(player.stats().hp == 31 && player.stats().mana == 20,
              "a level-up fully heals hp and mana to their (possibly unchanged) max");
    }

    // --- grantXp: a grant with leftover XP carrying into the new level.
    {
        Player player = makePlayer();
        grantXp(player, 25);
        check(player.level() == 2, "25 XP still only reaches level 2 (needs 20, not 40 more)");
        check(player.xp() == 5, "the leftover 5 XP (25 - 20) carries into progress toward level 3");
    }

    // --- grantXp: a single large grant crossing multiple levels at once
    // (the exact scenario the boss's 200 XP reward triggers against a
    // fresh level-1 character -- see MonsterFactory::xpRewardForType).
    // Cumulative cost 1->2->3->4->5 == 20+40+60+80 == 200 exactly.
    {
        Player player = makePlayer();
        grantXp(player, 200);
        check(player.level() == 5,
              "200 XP from level 1 crosses 4 thresholds at once, landing exactly on level 5 "
              "(20+40+60+80 == 200) -- confirms grantXp() loops rather than checking once");
        check(player.xp() == 0, "no leftover XP after landing exactly on a threshold");
        check(player.stats().maxHp == 30 + 4 * 1, "maxHp grew by 1 for each of the 4 level-ups");
        check(player.stats().maxMana == 20, "maxMana is still untouched -- no allocation happened");
        check(player.unspentAttributePoints() == 4 * 2,
              "4 level-ups grant 2 points each == 8 unspent attribute points");
    }

    // --- grantXp: capped at level 20, doesn't overshoot.
    {
        Player player = makePlayer();
        grantXp(player, 100000); // an absurdly large grant -- should still stop at 20
        check(player.level() == 20, "an enormous XP grant still caps at level 20, not beyond");
        check(player.xp() == 0,
              "XP is zeroed once capped, not left holding a huge leftover that reads as a bug");
        check(player.stats().maxHp == 30 + 19 * 1,
              "19 level-ups (1 through 20) grew maxHp by 1 each, capped correctly at the 19th");
        check(player.unspentAttributePoints() == 19 * 2,
              "19 level-ups granted 2 points each == 38 unspent attribute points, none lost to the cap");
    }

    // --- grantXp: already at level 20, further XP does nothing.
    {
        Player player = makePlayer();
        grantXp(player, 100000);
        check(player.level() == 20, "sanity check -- player is capped at 20 before the next grant");
        const int hpBefore = player.stats().maxHp;
        const int pointsBefore = player.unspentAttributePoints();
        grantXp(player, 500);
        check(player.level() == 20, "granting more XP at the cap doesn't do anything further");
        check(player.xp() == 0, "XP stays at 0 once capped, doesn't start accumulating again");
        check(player.stats().maxHp == hpBefore, "maxHp doesn't grow further once capped");
        check(player.unspentAttributePoints() == pointsBefore,
              "no further attribute points are granted once capped");
    }

    std::cout << "\n"
              << (g_allOk ? "All PlayerLeveling checks passed." : "Some PlayerLeveling checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
