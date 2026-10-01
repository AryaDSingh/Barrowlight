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

// --- Town -------------------------------------------------------------------------
inline const sf::FloatRect kTownInn{{560, 22}, {150, 36}}, kTownEquipment{{718, 22}, {150, 36}},
    kTownDungeons{{876, 22}, {180, 36}}, kTownReturn{{1064, 22}, {180, 36}};
inline const sf::FloatRect kTownBuy{{40, 96}, {150, 34}}, kTownSell{{196, 96}, {150, 34}};
inline constexpr int kTownRowsPerPage = 9;
inline sf::FloatRect townRow(int i) { return {{40, 140.f + 42.f * i}, {620, 38}}; }
inline const sf::FloatRect kTownPrevious{{40, 530}, {140, 34}}, kTownNext{{520, 530}, {140, 34}};
inline const sf::FloatRect kTownPreview{{690, 96}, {554, 480}};
inline const sf::FloatRect kTownTrade{{714, 512}, {300, 44}};

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
