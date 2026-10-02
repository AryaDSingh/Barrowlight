#pragma once

#include <SFML/Graphics/Rect.hpp>

namespace engine::screen {

// Hit areas for the full-screen menus, shared by their drawing code, their
// mouse handlers and the UI tests, so a layout change can't leave a click
// target behind. (The play screen's layout lives in PlayLayout.hpp.)

inline sf::Vector2i center(const sf::FloatRect& r) {
    return {static_cast<int>(r.position.x + r.size.x / 2), static_cast<int>(r.position.y + r.size.y / 2)};
}

// --- Class selection: three tall cards, Warrior / Mage / Thief ---------------
inline sf::FloatRect classCard(int i) { return {{70.f + 390.f * i, 122}, {360, 420}}; }
inline const sf::FloatRect kStartLoad{{70, 600}, {260, 44}};
inline const sf::FloatRect kModeToggle{{850, 600}, {360, 44}};

// --- Game over: one centred dialog ------------------------------------------------
inline const sf::FloatRect kGameOverDialog{{340, 150}, {600, 360}};
inline const sf::FloatRect kRestart{{390, 350}, {500, 48}};
inline const sf::FloatRect kRevive{{390, 408}, {500, 48}};

// --- Level up: attribute choices in a centred dialog -------------------------------
inline const sf::FloatRect kAttributeDialog{{290, 100}, {700, 470}};
inline sf::FloatRect attributeChoice(int i) { return {{330, 230.f + 108.f * i}, {620, 96}}; }
inline const sf::FloatRect kAttributeClose{{850, 118}, {124, 32}};

// --- Town -------------------------------------------------------------------------
inline const sf::FloatRect kTownInn{{560, 22}, {150, 36}}, kTownEquipment{{718, 22}, {150, 36}},
    kTownDungeons{{876, 22}, {180, 36}}, kTownReturn{{1064, 22}, {180, 36}};
inline const sf::FloatRect kTownBuy{{40, 96}, {150, 34}}, kTownSell{{196, 96}, {150, 34}};
inline constexpr int kTownRowsPerPage = 9;
inline sf::FloatRect townRow(int i) { return {{40, 140.f + 42.f * i}, {620, 38}}; }
inline const sf::FloatRect kTownPrevious{{40, 530}, {140, 34}}, kTownNext{{520, 530}, {140, 34}};
inline const sf::FloatRect kTownPreview{{690, 96}, {554, 480}};
inline const sf::FloatRect kTownTrade{{714, 512}, {300, 44}};
inline const sf::FloatRect kTownLeaveShop{{1084, 512}, {140, 44}};

// The town square: a 31x13 grid of 40px cells under the top bar. Each
// building is a click target; the merchant's list opens over the square.
inline constexpr float kTownCell = 40.f;
inline const sf::FloatRect kTownScene{{20, 76}, {31 * kTownCell, 13 * kTownCell}};
inline sf::FloatRect townCells(float c, float r, float w, float h) {
    return {{kTownScene.position.x + c * kTownCell, kTownScene.position.y + r * kTownCell}, {w * kTownCell, h * kTownCell}};
}
inline const sf::FloatRect kTownInnSpot = townCells(0, 1, 9, 7);
inline const sf::FloatRect kTownStashSpot = townCells(9, 8, 4, 3);
inline const sf::FloatRect kTownFountainSpot = townCells(12, 1, 6, 6);
inline const sf::FloatRect kTownMerchantSpot = townCells(18, 1, 7, 7);
inline const sf::FloatRect kTownGateSpot = townCells(25, 0, 6, 7);
inline const sf::FloatRect kTownObeliskSpot = townCells(21, 8, 4, 4);

// --- Ascendancy: six node cards in two rows -----------------------------------------
inline const sf::FloatRect kAscendDialog{{110, 40}, {1060, 640}};
inline sf::FloatRect ascendNode(int i) { return {{140.f + 336.f * (i % 3), 170.f + 222.f * (i / 3)}, {320, 208}}; }
inline const sf::FloatRect kAscendLearn{{760, 618}, {240, 42}}, kAscendClose{{1012, 618}, {136, 42}};

// --- The trial obelisk ---------------------------------------------------------------
inline const sf::FloatRect kTrialDialog{{220, 90}, {840, 540}};
inline sf::FloatRect trialCard(int i) { return {{250.f + 400.f * i, 190}, {380, 340}}; }
inline sf::FloatRect trialEnter(int i) { return {{270.f + 400.f * i, 476}, {340, 40}}; }
inline const sf::FloatRect kTrialClose{{560, 566}, {160, 40}};

// --- Dungeon selection ------------------------------------------------------------
inline sf::FloatRect dungeonCard(int i) { return {{40.f + 610.f * i, 100}, {590, 150}}; }
inline sf::FloatRect depthCard(int depth) { return {{40.f + 121.f * (depth - 1), 306}, {112, 80}}; }
inline const sf::FloatRect kDungeonEnter{{40, 604}, {280, 46}}, kDungeonBack{{336, 604}, {240, 46}};

// --- Stairs-down dialog (over the map) ---------------------------------------------
inline const sf::FloatRect kTravelDialog{{340, 190}, {600, 300}};
inline const sf::FloatRect kTravelDown{{370, 290}, {540, 44}}, kTravelTown{{370, 342}, {540, 44}},
    kTravelStay{{370, 394}, {540, 44}};

// --- Vault --------------------------------------------------------------------------
inline const sf::FloatRect kVaultWarning{{290, 110}, {700, 480}};
inline sf::FloatRect vaultRewardCard(int i) { return {{60.f + 400.f * i, 110}, {360, 350}}; }
inline sf::FloatRect vaultCommit(int menu) { return menu == 1 ? sf::FloatRect{{330, 514}, {300, 44}} : sf::FloatRect{{60, 490}, {320, 46}}; }
inline sf::FloatRect vaultCancel(int menu) { return menu == 1 ? sf::FloatRect{{650, 514}, {300, 44}} : sf::FloatRect{{396, 490}, {300, 46}}; }

// --- Shrine (landmark event) --------------------------------------------------------
inline const sf::FloatRect kShrineDialog{{230, 110}, {820, 470}};
inline sf::FloatRect shrineChoice(int i) { return {{262.f + 258.f * i, 210}, {240, 290}}; }
inline const sf::FloatRect kShrineLeave{{540, 518}, {200, 40}};

} // namespace engine::screen
