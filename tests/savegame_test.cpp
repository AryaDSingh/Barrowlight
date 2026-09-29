// Standalone sanity check for saveGame/loadGame. No SFML, no window, no
// Application -- SaveGame is entirely SFML-independent by design, so this
// tests the real serialization logic directly: build a SaveGameState by
// hand, save it, load it back into a fresh state, and verify every field
// matches exactly, not just "loading didn't crash."

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

#include "core/SaveGame.hpp"

using namespace engine;

namespace {
bool g_allOk = true;

void check(bool condition, const std::string& description) {
    g_allOk &= condition;
    std::cout << (condition ? "[ok] " : "[FAIL] ") << description << '\n';
}

Map makeTestMap() {
    Map map(5, 4);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 5; ++x) {
            const bool wall = (x == 0 || y == 0 || x == 4 || y == 3);
            map.setTile(x, y, wall ? Tile{TileType::Wall, false, false}
                                    : Tile{TileType::Floor, true, true});
        }
    }
    return map;
}
} // namespace

int main() {
    const std::string path = "savegame_test_output.txt";

    // --- Build a SaveGameState by hand, with deliberately varied data
    // (non-trivial stats, mixed fog-of-war, a poisoned player, a boss
    // among the monsters) so a round-trip bug has somewhere to hide.
    SaveGameState original;
    original.map = makeTestMap();

    original.exploredMap = ExploredMap(original.map);
    std::vector<Visibility> visibility(20, Visibility::Hidden);
    visibility[6] = Visibility::Visible;    // (1,1)
    visibility[7] = Visibility::Remembered; // (2,1)
    original.exploredMap.restoreAll(visibility);

    original.playerPosition = Position{2, 1};
    original.playerClass = PlayerClass::Warrior; // deliberately not the default (Spellblade) --
                                                    // a round-trip bug that always left this at
                                                    // its default would go uncaught otherwise
    original.playerLevel = 4; // deliberately not the default (1) -- same reasoning as playerClass
    original.playerXp = 37;   // deliberately not the default (0)
    original.currentFloor = 6; // deliberately not the default (1)
    original.playerHybridSpecced = true; // deliberately not the default (false)
    original.playerStats.hp = 17;
    original.playerStats.maxHp = 30;
    original.playerStats.mana = 9;
    original.playerStats.maxMana = 20;
    original.playerStats.strength = 12;
    original.playerStats.dexterity = 14;
    original.playerStats.intelligence = 18; // deliberately non-default -- this exact field was
                                              // missing from the save format until Prompt 15
                                              // caught it, and the gap slipped through
                                              // unnoticed specifically because the original
                                              // version of this test never set a non-default
                                              // value here either
    original.playerStats.speed = 100;
    original.playerTalents = {{"warrior.slam",0}, {"warrior.cleave",3},
        {"warrior.rallying_cry",1}, {"warrior.berserkers_fury",0},
        {"warrior.whirlwind",0}, {"warrior.undying_rage",4},
        {"mage.arcane_bolt",0}, {"mage.mind_shatter",2}};
    original.playerStatusEffects = {
        StatusEffectInstance{StatusEffectType::Poison, 2, 3},
    };
    original.lastMoveDirection = Position{1, 0};

    // Prompts 32-34: equipment in every location, loot stream, chest, runes.
    original.nextItemId = 20;
    original.items = {
        {"iron_sword", 3, 0, Position{}, 1, {{"might", 4}, {"vigor", 5}}}, // equipped weapon, tier 1
        {"chain_coat", 5, -1, Position{}, 0, {{"reservoir", 4}}},            // in the bag
        {"focus_charm", 7, -2, Position{3, 2}, 0, {}},                       // on the ground
    };
    original.lootRngState = 0x9E3779B97F4A7C15ull;
    original.ordinaryDrops = 2;
    original.chestExists = true;
    original.chestClaimed = true;
    original.chestPosition = Position{1, 2};
    original.runeChoiceAvailable = true;
    original.runes = {{"chain", 11, "mage.arcane_bolt"}, {"widen", 12, ""}};
    original.unspentAttributePoints = 3;

    SaveGameState::MonsterSaveData goblin;
    goblin.type = MonsterType::Goblin;
    goblin.position = Position{3, 1};
    goblin.hp = 12;
    goblin.maxHp = 20;
    goblin.isBoss = false;
    original.monsters.push_back(goblin);

    SaveGameState::MonsterSaveData boss;
    boss.type = MonsterType::GoblinWarlord;
    boss.position = Position{2, 2};
    boss.hp = 40;
    boss.maxHp = 90;
    boss.isBoss = true;
    boss.statusEffects = {
        StatusEffectInstance{StatusEffectType::Empowered, 999, 6},
    };
    original.monsters.push_back(boss);

    // --- Round-trip.
    check(saveGame(original, path), "saveGame() succeeds");

    const std::optional<SaveGameState> loadedOpt = loadGame(path);
    check(loadedOpt.has_value(), "loadGame() succeeds and returns a value");
    if (!loadedOpt.has_value()) {
        std::cout << "\nCannot continue without a loaded state.\n";
        return 1;
    }
    const SaveGameState& loaded = *loadedOpt;

    // --- Field-by-field verification.
    check(loaded.map.width() == original.map.width() &&
              loaded.map.height() == original.map.height(),
          "map dimensions match");

    bool mapTilesMatch = true;
    for (int y = 0; y < original.map.height() && mapTilesMatch; ++y) {
        for (int x = 0; x < original.map.width() && mapTilesMatch; ++x) {
            if (loaded.map.tileAt(x, y).type != original.map.tileAt(x, y).type ||
                loaded.map.tileAt(x, y).walkable != original.map.tileAt(x, y).walkable) {
                mapTilesMatch = false;
            }
        }
    }
    check(mapTilesMatch, "every map tile's type/walkable matches exactly");

    check(loaded.exploredMap.at(1, 1) == Visibility::Visible &&
              loaded.exploredMap.at(2, 1) == Visibility::Remembered &&
              loaded.exploredMap.at(0, 0) == Visibility::Hidden,
          "fog-of-war visibility matches exactly, including the Hidden default");

    check(loaded.playerPosition.x == 2 && loaded.playerPosition.y == 1,
          "player position matches");
    check(loaded.playerClass == PlayerClass::Warrior,
          "player class matches (not left at the Spellblade default)");
    check(loaded.playerLevel == 4 && loaded.playerXp == 37,
          "player level and XP match (not left at the level-1/0-XP defaults)");
    check(loaded.currentFloor == 6, "current floor matches (not left at the default of 1)");
    check(loaded.playerHybridSpecced == true,
          "player hybrid-specced flag matches (not left at the false default)");
    check(loaded.playerTalents.size() == 8 &&
              loaded.playerTalents[6].id == "mage.arcane_bolt" &&
              loaded.playerTalents[7].id == "mage.mind_shatter",
          "borrowed talents preserve stable IDs");
    check(loaded.playerStats.hp == 17 && loaded.playerStats.maxHp == 30 &&
              loaded.playerStats.mana == 9 && loaded.playerStats.maxMana == 20 &&
              loaded.playerStats.strength == 12 && loaded.playerStats.dexterity == 14 &&
              loaded.playerStats.intelligence == 18 && loaded.playerStats.speed == 100,
          "every player stat field matches exactly");
    check(loaded.playerTalents == original.playerTalents,
          "all 8 talent identities and cooldowns match exactly");
    check(loaded.lastMoveDirection.x == 1 && loaded.lastMoveDirection.y == 0,
          "lastMoveDirection matches (needed for Blink to resume correctly)");

    check(loaded.nextItemId == 20 && loaded.unspentAttributePoints == 3,
          "next item ID and unspent attribute points match");
    bool itemsMatch = loaded.items.size() == original.items.size();
    for (std::size_t i = 0; itemsMatch && i < original.items.size(); ++i) {
        const auto& a = original.items[i];
        const auto& b = loaded.items[i];
        itemsMatch = a.definitionId == b.definitionId && a.instanceId == b.instanceId &&
                     a.location == b.location && a.position.x == b.position.x &&
                     a.position.y == b.position.y && a.rollTier == b.rollTier &&
                     a.affixes.size() == b.affixes.size();
        for (std::size_t j = 0; itemsMatch && j < a.affixes.size(); ++j)
            itemsMatch = a.affixes[j].id == b.affixes[j].id && a.affixes[j].value == b.affixes[j].value;
    }
    check(itemsMatch, "equipped, bagged and ground items match exactly, affixes never rerolled");
    check(loaded.lootRngState == original.lootRngState && loaded.ordinaryDrops == 2,
          "loot RNG state and the per-floor drop count match");
    check(loaded.chestExists && loaded.chestClaimed && loaded.chestPosition.x == 1 &&
              loaded.chestPosition.y == 2,
          "chest position and claimed flag match");
    check(loaded.runeChoiceAvailable && loaded.runes.size() == 2 &&
              loaded.runes[0].definitionId == "chain" && loaded.runes[0].instanceId == 11 &&
              loaded.runes[0].talentId == "mage.arcane_bolt" &&
              loaded.runes[1].definitionId == "widen" && loaded.runes[1].talentId.empty(),
          "rune ownership and attachments match, including an unattached rune");

    check(loaded.playerStatusEffects.size() == 1 &&
              loaded.playerStatusEffects[0].type == StatusEffectType::Poison &&
              loaded.playerStatusEffects[0].turnsRemaining == 2 &&
              loaded.playerStatusEffects[0].magnitude == 3,
          "player's poison status effect matches exactly");

    check(loaded.monsters.size() == 2, "both monsters were restored");
    if (loaded.monsters.size() == 2) {
        const auto& g = loaded.monsters[0];
        check(g.type == MonsterType::Goblin && g.position.x == 3 && g.position.y == 1 &&
                  g.hp == 12 && g.maxHp == 20 && !g.isBoss && g.statusEffects.empty(),
              "goblin monster's every field matches exactly");

        const auto& b = loaded.monsters[1];
        check(b.type == MonsterType::GoblinWarlord && b.position.x == 2 && b.position.y == 2 &&
                  b.hp == 40 && b.maxHp == 90 && b.isBoss,
              "boss monster's every field matches exactly, including isBoss");
        check(b.statusEffects.size() == 1 &&
                  b.statusEffects[0].type == StatusEffectType::Empowered &&
                  b.statusEffects[0].turnsRemaining == 999 && b.statusEffects[0].magnitude == 6,
              "boss's Empowered status effect matches exactly");
    }

    // --- Missing/corrupt file handling.
    const std::optional<SaveGameState> missing = loadGame("this_file_does_not_exist.txt");
    check(!missing.has_value(), "loading a missing file returns nullopt, not a crash");

    {
        std::ofstream garbage("savegame_test_garbage.txt");
        garbage << "not a valid save file at all\n";
    }
    const std::optional<SaveGameState> corrupt = loadGame("savegame_test_garbage.txt");
    check(!corrupt.has_value(), "loading a malformed file returns nullopt, not a crash");

    // --- Item/rune validation: both saveGame() and loadGame() refuse states
    // that could never arise in play, rather than restoring them.
    {
        SaveGameState bad = original;
        bad.items[0].affixes[0].value = 99; // tier-1 "might" allows 3-5
        check(!saveGame(bad, path), "an affix value outside its tier range is refused");

        bad = original;
        bad.items[1].location = 0; // chain coat into the weapon slot
        check(!saveGame(bad, path), "an item equipped in the wrong slot is refused");

        bad = original;
        bad.items[1].instanceId = 3; // duplicates the sword's ID
        check(!saveGame(bad, path), "duplicate item instance IDs are refused");

        bad = original;
        bad.runes[1].talentId = "mage.meteor"; // not a known talent in this save
        check(!saveGame(bad, path), "a rune attached to a talent the player doesn't know is refused");

        bad = original;
        bad.runes[1].talentId = "mage.arcane_bolt"; // second rune on the same talent
        check(!saveGame(bad, path), "two runes attached to one talent are refused");

        bad = original;
        bad.ordinaryDrops = 3;
        check(!saveGame(bad, path), "an ordinary-drop count past the per-floor cap is refused");
    }

    std::remove(path.c_str());
    std::remove("savegame_test_garbage.txt");

    std::cout << "\n" << (g_allOk ? "All save/load checks passed." : "Some checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
