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
    // --- xpForNextLevel: 20 * level + 1.5 * level squared (rounded down).
    check(xpForNextLevel(1) == 21, "xpForNextLevel(1) == 21");
    check(xpForNextLevel(2) == 46, "xpForNextLevel(2) == 46");
    check(xpForNextLevel(9) == 301, "xpForNextLevel(9) == 301 (the step from 9 to 10)");
    {
        int total = 0;
        for (int level = 1; level < 20; ++level) total += xpForNextLevel(level);
        check(total > 7000 && total < 8000, "about 7,500 XP from level 1 to 20 (was 3,800)");
    }

    // --- grantXp: a small grant that doesn't cross a threshold.
    {
        Player player = makePlayer();
        grantXp(player, 5);
        check(player.level() == 1, "5 XP (below the 21 needed) doesn't level up");
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
        grantXp(player, 21);
        check(player.level() == 2, "exactly 21 XP triggers the level 1 -> 2 level-up");
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
        grantXp(player, 26);
        check(player.level() == 2, "26 XP still only reaches level 2 (needs 21, then 46 more)");
        check(player.xp() == 5, "the leftover 5 XP (26 - 21) carries into progress toward level 3");
    }

    // --- grantXp: a single large grant crossing multiple levels at once
    // (the exact scenario the boss's 200 XP reward triggers against a
    // fresh level-1 character -- see MonsterFactory::xpRewardForType).
    // Cumulative cost 1->2->3->4->5 == 21+46+73+104 == 244 exactly.
    {
        Player player = makePlayer();
        grantXp(player, 244);
        check(player.level() == 5,
              "244 XP from level 1 crosses 4 thresholds at once, landing exactly on level 5 "
              "(21+46+73+104 == 244) -- confirms grantXp() loops rather than checking once");
        check(player.xp() == 0, "no leftover XP after landing exactly on a threshold");
        check(player.stats().maxHp == 30 + 4 * 1, "maxHp grew by 1 for each of the 4 level-ups");
        check(player.stats().maxMana == 20, "maxMana is still untouched -- no allocation happened");
        check(player.unspentAttributePoints() == 4 * 2,
              "4 level-ups grant 2 points each == 8 unspent attribute points");
    }

    // --- grantXp: capped at level 30 on the paths beyond the Lich (the winter road), doesn't overshoot.
    {
        Player player = makePlayer();
        player.lore.push_back("winter_road");
        grantXp(player, 100000); // an absurdly large grant -- should still stop at 30
        check(player.level() == 30, "an enormous XP grant still caps at level 30, not beyond");
        check(player.xp() == 0,
              "XP is zeroed once capped, not left holding a huge leftover that reads as a bug");
        check(player.stats().maxHp == 30 + 29 * 1,
              "29 level-ups (1 through 30) grew maxHp by 1 each, capped correctly at the 29th");
        check(player.unspentAttributePoints() == 29 * 2,
              "29 level-ups granted 2 points each == 58 unspent attribute points, none lost to the cap");
    }

    // --- grantXp: before the Lich falls, the cap is 20.
    {
        Player player = makePlayer();
        grantXp(player, 100000);
        check(player.level() == 20, "until you go on past the Lich, the cap is 20");
        player.lore.push_back("winter_road");
        grantXp(player, 2000);
        check(player.level() > 20, "past the Lich, levels go on");
    }

    // --- grantXp: already at level 30, further XP does nothing.
    {
        Player player = makePlayer();
        player.lore.push_back("winter_road");
        grantXp(player, 100000);
        check(player.level() == 30, "sanity check -- player is capped at 30 before the next grant");
        const int hpBefore = player.stats().maxHp;
        const int pointsBefore = player.unspentAttributePoints();
        grantXp(player, 500);
        check(player.level() == 30, "granting more XP at the cap doesn't do anything further");
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
