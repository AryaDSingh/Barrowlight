#include "entities/PlayerLeveling.hpp"

namespace engine {

namespace {
constexpr int kMaxLevel = 10;
constexpr int kXpPerLevelStep = 20;
constexpr int kMaxHpGrowthPerLevel = 1;
constexpr int kAttributePointsPerLevel = 2;
} // namespace

int xpForNextLevel(int currentLevel) {
    return kXpPerLevelStep * currentLevel;
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
