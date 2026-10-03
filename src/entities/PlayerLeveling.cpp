#include "entities/PlayerLeveling.hpp"
#include "entities/RunProgression.hpp"

namespace engine {

namespace {
constexpr int kMaxLevel = kRunMaxLevel;
constexpr int kXpPerLevelStep = 20;
constexpr int kMaxHpGrowthPerLevel = 1;
constexpr int kAttributePointsPerLevel = 2;
} // namespace

// Each level costs a little more than the last, and the step itself grows:
// 20 per level plus 1.5 x level squared. About 7,500 XP to reach level 20,
// which a full run reaches around its last few floors.
int xpForNextLevel(int currentLevel) {
    return kXpPerLevelStep * currentLevel + 3 * currentLevel * currentLevel / 2;
}

void grantXp(Player& player, int amount) {
    if (player.level() >= kMaxLevel) {
        return; // already capped -- don't let XP pile up toward a level that won't come
    }

    player.xp() += amount;

    while (player.level() < kMaxLevel && player.xp() >= xpForNextLevel(player.level())) {
        player.xp() -= xpForNextLevel(player.level());
        player.level() += 1;

        player.baseStats().maxHp += kMaxHpGrowthPerLevel;
        player.refreshEquipmentStats();
        player.unspentAttributePoints() += kAttributePointsPerLevel;
        ++player.abilityPoints();
        if (grantsExtraAbilityPoint(player.level())) ++player.abilityPoints();
        if (grantsTreePoint(player.level())) ++player.treePoints();
        // Full heal on level-up, same as before -- max mana isn't
        // touched here at all anymore (it only grows from Intelligence
        // points the player actually chooses to spend), but a fresh
        // level should still restore whatever pool currently exists.
        player.stats().hp = player.stats().maxHp;
        player.stats().mana = player.stats().maxMana;
    }

    if (player.level() >= kMaxLevel) {
        player.xp() = 0; // nothing left to progress toward -- avoid a confusing "X/0" display
    }
}

} // namespace engine
