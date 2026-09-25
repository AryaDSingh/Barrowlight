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
    original.playerHybridPickNames = {"Arcane Bolt", "Mind Shatter"}; // deliberately non-empty --
                                                                        // names with spaces,
                                                                        // specifically exercising
                                                                        // the underscore-escaping
                                                                        // round-trip
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
    original.playerCooldowns = {0, 3, 1, 0, 0, 4, 0, 2};
    original.playerStatusEffects = {
        StatusEffectInstance{StatusEffectType::Poison, 2, 3},
    };
    original.lastMoveDirection = Position{1, 0};

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
    check(loaded.playerHybridPickNames.size() == 2 &&
              loaded.playerHybridPickNames[0] == "Arcane Bolt" &&
              loaded.playerHybridPickNames[1] == "Mind Shatter",
          "player hybrid pick names round-trip exactly, including the space in each name "
          "(the underscore-escaping survives the round-trip correctly)");
    check(loaded.playerStats.hp == 17 && loaded.playerStats.maxHp == 30 &&
              loaded.playerStats.mana == 9 && loaded.playerStats.maxMana == 20 &&
              loaded.playerStats.strength == 12 && loaded.playerStats.dexterity == 14 &&
              loaded.playerStats.intelligence == 18 && loaded.playerStats.speed == 100,
          "every player stat field matches exactly");
    check(loaded.playerCooldowns == original.playerCooldowns,
          "all 8 talent cooldowns match exactly, in order");
    check(loaded.lastMoveDirection.x == 1 && loaded.lastMoveDirection.y == 0,
          "lastMoveDirection matches (needed for Blink to resume correctly)");

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

    std::remove(path.c_str());
    std::remove("savegame_test_garbage.txt");

    std::cout << "\n" << (g_allOk ? "All save/load checks passed." : "Some checks FAILED.")
              << '\n';
    return g_allOk ? 0 : 1;
}
