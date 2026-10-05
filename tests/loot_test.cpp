// Standalone sanity check for Prompts 32-33's equipment and loot --
// Inventory's slot/bag ownership, Player's effective stats, Item bonus
// totals, and LootGenerator's determinism, tier formula and affix rules.
// Hand-computed expected results where the numbers are fixed; statistical
// bounds where they're rolled. No SFML, no window, no Application.

#include <algorithm>
#include <iostream>
#include <set>
#include <string>

#include "entities/LootGenerator.hpp"
#include "entities/Player.hpp"

using namespace engine;

namespace {
bool g_allOk = true;

void check(bool condition, const std::string& description) {
    g_allOk &= condition;
    std::cout << (condition ? "[ok] " : "[FAIL] ") << description << '\n';
}

std::unique_ptr<Item> makeItem(const char* id, std::uint64_t instance, std::vector<RolledAffix> affixes = {}) {
    return std::make_unique<Item>(*findItemDefinition(id), instance, Position{}, std::move(affixes));
}

bool sameItem(const Item& a, const Item& b) {
    if (a.definition() != b.definition() || a.rollTier() != b.rollTier() || a.affixes().size() != b.affixes().size())
        return false;
    for (std::size_t i = 0; i < a.affixes().size(); ++i)
        if (a.affixes()[i].id != b.affixes()[i].id || a.affixes()[i].value != b.affixes()[i].value) return false;
    return true;
}

// Every property the save loader also enforces: known affix, allowed in the
// slot, no two affixes on one stat, value inside the tier-shifted range.
bool affixesValid(const Item& item) {
    const unsigned slotBit = 1u << static_cast<unsigned>(item.definition()->slot);
    std::set<BonusStat> stats;
    for (const auto& rolled : item.affixes()) {
        const auto* affix = findAffix(rolled.id);
        if (!affix || !(affix->slots & slotBit) || !stats.insert(affix->stat).second) return false;
        const int shift = item.rollTier() * affix->perTier;
        if (rolled.value < affix->minimum + shift || rolled.value > affix->maximum + shift) return false;
    }
    return true;
}
} // namespace

int main() {
    // --- Item bonuses: definition plus rolled affixes; rarity is affix count.
    {
        const auto plain = makeItem("iron_sword", 1);
        check(plain->rarity() == ItemRarity::Normal && plain->name() == "Normal Iron Sword",
              "an item with no affixes is Normal, and named that way");
        check(plain->bonuses().strength == 5, "Iron Sword's base bonus is +5 Strength");

        const auto rare = makeItem("iron_sword", 2, {{"might", 3}, {"vigor", 2}});
        check(rare->rarity() == ItemRarity::Rare, "two affixes make an item Rare");
        check(rare->bonuses().strength == 8 && rare->bonuses().maxHp == 2,
              "affix values add onto the definition's bonuses (5+3 Str, +2 HP)");
    }

    // --- Inventory and Player effective stats.
    {
        Stats base;
        base.maxHp = 20; base.hp = 20; base.maxMana = 10; base.mana = 10; base.strength = 4;
        Player player(Position{}, base, TalentSet{});
        player.inventory().add(makeItem("iron_sword", 1));
        player.inventory().add(makeItem("chain_coat", 2));
        player.inventory().add(makeItem("iron_sword", 3, {{"might", 2}}));

        check(player.equip(0), "equipping the bag's first item succeeds");
        check(player.inventory().equipped(EquipmentSlot::Weapon)->instanceId() == 1 &&
                  player.inventory().items().size() == 2,
              "the sword moves from the bag into the weapon slot");
        check(player.stats().strength == 9 && player.baseStats().strength == 4,
              "equipping adds to effective Strength (4+5) without touching base stats");

        check(player.equip(0), "equipping the chain coat succeeds");
        check(player.stats().maxHp == 26 && player.stats().hp == 20,
              "armour raises max HP (20+6) but not current HP");

        // Bag is now just the magic sword (instance 3), at index 0.
        check(player.equip(0), "equipping a second weapon succeeds");
        check(player.inventory().equipped(EquipmentSlot::Weapon)->instanceId() == 3,
              "the new sword occupies the weapon slot");
        check(player.inventory().items().size() == 1 && player.inventory().items()[0]->instanceId() == 1,
              "the replaced sword is swapped back into the bag, not lost");
        check(player.stats().strength == 11, "effective Strength reflects the magic sword (4+5+2)");

        player.stats().hp = 26;
        check(player.unequip(EquipmentSlot::Armour), "unequipping armour succeeds");
        check(player.stats().maxHp == 20 && player.stats().hp == 20,
              "removing armour clamps current HP down to the new maximum");
        check(!player.unequip(EquipmentSlot::Armour), "unequipping an empty slot fails");
        check(!player.equip(42), "equipping an out-of-range bag index fails");
        check(!player.inventory().equipped(EquipmentSlot::Charm), "the charm slot stays empty throughout");
    }

    // --- LootGenerator: seed handling and determinism.
    {
        check(LootGenerator(0).state() == 1, "a zero seed is replaced by 1 (xorshift cannot leave 0)");
        LootGenerator zero(5);
        zero.restore(0);
        check(zero.state() == 1, "restoring a zero state is also replaced by 1");

        LootGenerator a(1234), b(1234);
        bool identical = true;
        for (int i = 0; i < 50; ++i)
            identical &= sameItem(*a.generate(4, 1, i + 1, {}), *b.generate(4, 1, i + 1, {}));
        check(identical, "two generators with the same seed produce the same 50 items");

        LootGenerator original(99);
        original.generate(1, 0, 1, {});
        LootGenerator resumed(7);
        resumed.restore(original.state());
        check(sameItem(*original.generate(7, 2, 2, {}), *resumed.generate(7, 2, 2, {})),
              "restoring a saved state continues the exact same sequence");
    }

    // --- Tier formula: clamp((floor-1)/3 + quality, 0, 5).
    {
        LootGenerator loot(42);
        check(loot.generate(1, 0, 1, {})->rollTier() == 0, "floor 1, ordinary quality -> tier 0");
        check(loot.generate(3, 0, 1, {})->rollTier() == 0, "floor 3, ordinary quality -> tier 0");
        check(loot.generate(4, 0, 1, {})->rollTier() == 1, "floor 4, ordinary quality -> tier 1");
        check(loot.generate(4, 1, 1, {})->rollTier() == 2, "floor 4, Elite quality -> tier 2");
        check(loot.generate(10, 2, 1, {})->rollTier() == 5, "floor 10, boss quality -> tier 5");
        check(loot.generate(30, 2, 1, {})->rollTier() == 5, "tier is capped at 5");
    }

    // --- Affix rules and rarity weights over many rolls.
    {
        LootGenerator loot(2026);
        const int rolls = 20000;
        int normal = 0, magic = 0, rare = 0;
        bool allValid = true;
        std::set<std::string> definitionsSeen;
        for (int i = 0; i < rolls; ++i) {
            const auto item = loot.generate(1, 0, i + 1, {});
            allValid &= affixesValid(*item);
            definitionsSeen.insert(item->definition()->id);
            if (item->rarity() == ItemRarity::Normal) ++normal;
            else if (item->rarity() == ItemRarity::Magic) ++magic;
            else ++rare;
        }
        check(allValid, "20000 tier-0 items: every affix fits its slot, stat and value range");
        check(definitionsSeen.size() == kItemDefinitions.size(), "every item definition can drop");
        // Expected 15% / 60% / 25%; a 2-point band is many standard deviations at n=20000.
        check(rare > rolls * 13 / 100 && rare < rolls * 17 / 100, "tier 0: about 15% of drops are Rare");
        check(magic > rolls * 58 / 100 && magic < rolls * 62 / 100, "tier 0: about 60% of drops are Magic");
        check(normal > rolls * 23 / 100 && normal < rolls * 27 / 100, "tier 0: about 25% of drops are Normal");

        int highRare = 0;
        bool highValid = true;
        for (int i = 0; i < rolls; ++i) {
            const auto item = loot.generate(16, 2, i + 1, {}); // tier 5: rare threshold 40%
            highValid &= affixesValid(*item);
            if (item->rarity() == ItemRarity::Rare) ++highRare;
        }
        check(highValid, "20000 tier-5 items: every affix value is shifted by tier * step");
        check(highRare > rolls * 38 / 100 && highRare < rolls * 42 / 100, "tier 5: about 40% of drops are Rare");

        bool allRare = true;
        for (int i = 0; i < 500; ++i) allRare &= loot.generate(1, 2, i + 1, {}, ItemRarity::Rare)->rarity() == ItemRarity::Rare;
        check(allRare, "a Rare minimum (boss rewards) always yields two affixes");

        bool noneNormal = true;
        for (int i = 0; i < 500; ++i) noneNormal &= loot.generate(1, 0, i + 1, {}, ItemRarity::Magic)->rarity() != ItemRarity::Normal;
        check(noneNormal, "a Magic minimum (chests) never yields a Normal item");
    }

    std::cout << "\n" << (g_allOk ? "All loot checks passed." : "Some checks FAILED.") << '\n';
    return g_allOk ? 0 : 1;
}
