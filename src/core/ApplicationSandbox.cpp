#include "core/Application.hpp"
#include "entities/MonsterFactory.hpp"
#include "entities/PlayerLeveling.hpp"
#include "entities/TalentProgression.hpp"
#include "entities/DungeonProgression.hpp"
#include "entities/Ascendancy.hpp"
#include "entities/Lore.hpp"

#include <algorithm>
#include <bitset>

namespace engine {

// The sandbox: a run for trying things. F1 opens a panel that spawns any
// foe or item, grants levels and points, and respecs, so a build can be
// changed on the fly. Nothing in it is saved.
namespace {
const sf::FloatRect kPanel{{790, 30}, {470, 660}};
sf::FloatRect sandboxTab(int i) { return {{806.f + 90.f * i, 46}, {85, 30}}; }
constexpr int kSandboxTabs = 5;
sf::FloatRect sandboxToggle(int i, int count, float y = 86) {
    const float w = (kPanel.size.x - 32 - 6.f * (count - 1)) / count;
    return {{806.f + (w + 6) * i, y}, {w, 28}};
}
sf::FloatRect sandboxRarity(int i) { return sandboxToggle(i, 3, 118); }
sf::FloatRect sandboxItemCell(int i) { return {{806.f + 222.f * (i % 2), 154.f + 26.f * (i / 2)}, {216, 24}}; }
// Every kind of foe, in 3 columns.
sf::FloatRect sandboxCell(int i) { return {{806.f + 148.f * (i % 3), 124.f + 22.f * (i / 3)}, {144, 20}}; }
sf::FloatRect sandboxButton(int i) { return {{806.f + 222.f * (i % 2), 130.f + 40.f * (i / 2)}, {216, 34}}; }
const sf::FloatRect kPagePrev{{806, 640}, {60, 30}}, kPageNext{{1184, 640}, {60, 30}};
constexpr int kItemsPerPage = 36; // 18 rows of 2, from y 154 to 622
constexpr int kMonsterTypes = static_cast<int>(MonsterType::WinterKing) + 1;

const char* itemGroupName(int g) { return g == 0 ? "Weapons" : g == 1 ? "Armour" : g == 2 ? "Jewellery" : "Uniques"; }
bool inItemGroup(const ItemDefinition& d, int group) {
    if (group == 3) return d.unique;
    if (d.unique) return false;
    switch (d.slot) {
        case EquipmentSlot::Weapon: case EquipmentSlot::OffHand: return group == 0;
        case EquipmentSlot::Charm: case EquipmentSlot::Ring1: case EquipmentSlot::Ring2: return group == 2;
        default: return group == 1;
    }
}
std::vector<const ItemDefinition*> itemGroup(int group) {
    std::vector<const ItemDefinition*> found;
    for (const auto& d : kItemDefinitions) if (inItemGroup(d, group)) found.push_back(&d);
    return found;
}
const std::string& monsterName(int type) {
    static std::vector<std::string> names;
    if (names.empty())
        for (int t = 0; t < kMonsterTypes; ++t) names.push_back(createMonster(static_cast<MonsterType>(t), {0, 0})->name());
    return names[static_cast<std::size_t>(type)];
}
const char* characterAction(int i, bool god) {
    static const char* labels[]{"+1 level", "+5 levels", "+5 ability points", "+5 utility points", "+1 tree point", "Respec everything",
                                "+5 Strength", "+5 Dexterity", "+5 Intelligence", "Full heal", nullptr, "Open talents", "Max out", "Choose ascendancy"};
    return i == 10 ? (god ? "God mode: on" : "God mode: off") : labels[i];
}
constexpr int kCharacterActions = 14;
// The Travel tab: every dungeon, a button for each of its depths.
sf::FloatRect travelDepth(int dungeon, int depth) { return {{806.f + 44.f * (depth - 1), 116.f + 74.f * dungeon}, {40, 30}}; }
const char* worldAction(int i) {
    static const char* labels[]{"Reveal the map", "Kill every foe", "Ready all cooldowns", "Spawn a chest", "Spawn a mimic", "Previous floor", "Next floor"};
    return labels[i];
}
constexpr int kWorldActions = 7;
} // namespace

void Application::startSandbox(PlayerClass cls) {
    selectClass(cls);
    sandboxRun_ = true; player_.sandbox = true; extraLives_ = 0;
    log("Sandbox: F1 opens the sandbox panel. Nothing here is saved.");
}

// A free tile among the 8 around you, then further out.
std::optional<Position> Application::sandboxSpot() {
    const auto me = player_.position();
    for (int r = 1; r <= 5; ++r)
        for (int dy = -r; dy <= r; ++dy)
            for (int dx = -r; dx <= r; ++dx) {
                const Position p{me.x + dx, me.y + dy};
                if (std::max(std::abs(dx), std::abs(dy)) == r && map_.isWalkable(p.x, p.y) && !isOccupied(p, nullptr)) return p;
            }
    return std::nullopt;
}

void Application::sandboxSpawn(int type) {
    const auto spot = sandboxSpot();
    if (!spot) { log("No room to spawn anything."); return; }
    const MonsterTier tier = sandboxTier_ == 2 ? MonsterTier::Nightmare : sandboxTier_ == 1 ? MonsterTier::Elite : MonsterTier::Base;
    auto m = createMonster(static_cast<MonsterType>(type), *spot, tier);
    scaleDungeonMonster(*m, floorDepth(currentFloor_));
    m->lastObservedHp = m->stats().hp;
    m->tactics.home = *spot;
    m->tactics.concealed = false;
    if (sandboxAwake_) { m->tactics.alert = 8; m->tactics.lastKnown = player_.position(); m->voicedAlert = true; }
    log(m->name(), " appears.");
    scheduler_.add(*m);
    monsters_.push_back(std::move(m));
    updateFieldOfView();
}

void Application::sandboxItem(const ItemDefinition& definition) {
    if (player_.inventory().full()) { log("Bag full (50). Drop or sell an item first."); return; }
    const ItemRarity rarity = sandboxRarity_ == 2 ? ItemRarity::Rare : sandboxRarity_ == 1 ? ItemRarity::Magic : ItemRarity::Normal;
    auto item = loot_.make(definition, std::max(1, floorDepth(currentFloor_)), rarity, nextItemId_++, player_.position());
    log(item->name(), " is in your bag.");
    player_.inventory().add(std::move(item));
}

// Every point back and every tree closed; your basic abilities stay.
void Application::sandboxRespec() {
    std::vector<Talent> basics;
    for (const auto& t : player_.talents().knownTalents()) if (t.id.rfind("basic.", 0) == 0) basics.push_back(t);
    player_.talents() = TalentSet(basics);
    player_.trees().clear();
    player_.abilityPoints() = earnedAbilityPoints(player_.level());
    player_.utilityPoints() = earnedUtilityPoints(player_.level());
    player_.treePoints() = earnedTreePoints(player_.level());
    player_.refreshEquipmentStats();
    log("Every point comes back. Every tree closes.");
}

void Application::sandboxCharacter(int action) {
    auto& base = player_.baseStats();
    const auto levels = [&](int n) {
        const int before = player_.level();
        for (int i = 0; i < n && player_.level() < player_.levelCap(); ++i) grantXp(player_, xpForNextLevel(player_.level()) - player_.xp());
        if (player_.level() > before) { soundManager_.play(SoundEffect::LevelUp); log("You are now level ", player_.level(), "."); }
    };
    switch (action) {
        case 0: levels(1); break;
        case 1: levels(5); break;
        case 2: player_.abilityPoints() += 5; log("+5 ability points."); break;
        case 3: player_.utilityPoints() += 5; log("+5 utility points."); break;
        case 4: ++player_.treePoints(); log("+1 tree point."); break;
        case 5: sandboxRespec(); break;
        case 6: base.strength += 5; base.maxHp += 5; player_.refreshEquipmentStats(); log("+5 Strength."); break;
        case 7: base.dexterity += 5; player_.refreshEquipmentStats(); log("+5 Dexterity."); break;
        case 8: base.intelligence += 5; base.maxMana += 5; player_.refreshEquipmentStats(); log("+5 Intelligence."); break;
        case 9: player_.stats().hp = player_.stats().maxHp; player_.stats().mana = player_.stats().maxMana; player_.statusEffects().active().clear(); log("Fully restored."); break;
        case 10: sandboxGod_ = !sandboxGod_; log(sandboxGod_ ? "Nothing can kill you now." : "You are mortal again."); break;
        case 11: sandboxMenu_ = false; openTalentTrees(); break;
        case 12: sandboxMaxOut(); break;
        case 13:
            if (!player_.trialsCleared) { log("Max out first: an ascendancy needs a trial won."); break; }
            if (!player_.ascendancy.empty()) { log("You are already a ", findAscendancy(player_.ascendancy)->name, ". Respec to choose again."); break; }
            sandboxMenu_ = false; openAscendancy(); break;
    }
}

// Max out: level 30, every lore found (every dungeon and deep tree open, the
// winter road too), every trial won with its points, gold, and full health.
// Your attribute points are left for you to spend.
void Application::sandboxMaxOut() {
    for (const auto& e : loreEntries()) if (!player_.knowsLore(e.id)) player_.lore.push_back(e.id);
    const int before = player_.level();
    while (player_.level() < player_.levelCap()) grantXp(player_, xpForNextLevel(player_.level()) - player_.xp());
    const int all = (1 << kTrialCount) - 1;
    const int newly = static_cast<int>(std::bitset<8>(static_cast<unsigned>(all & ~player_.trialsCleared)).count());
    player_.trialKeys = all; player_.trialsCleared = all; player_.ascendancyPoints += newly;
    gold_ += 5000;
    player_.stats().hp = player_.stats().maxHp; player_.stats().mana = player_.stats().maxMana;
    if (player_.level() > before) soundManager_.play(SoundEffect::LevelUp);
    log("Maxed out: level ", player_.level(), ", every lore, every dungeon and trial open, 5000 gold. Spend your points (T, and attributes).");
}

void Application::sandboxWorld(int action) {
    switch (action) {
        case 0: {
            exploredMap_.restoreAll(std::vector<Visibility>(static_cast<std::size_t>(map_.width() * map_.height()), Visibility::Remembered));
            updateFieldOfView(); log("The whole floor is revealed.");
            break;
        }
        case 1: {
            int slain = 0;
            for (auto& m : monsters_) if (!m->allied && m->stats().hp > 0) { m->stats().hp = 0; checkAndHandleDeath(*m); ++slain; }
            removeDeadMonsters();
            log(slain, " foes fall.");
            break;
        }
        case 2:
            for (std::size_t i = 0; i < player_.talents().knownTalents().size(); ++i) player_.talents().setCooldownRemaining(i, 0);
            log("Every ability is ready.");
            break;
        case 3: case 4: {
            const auto spot = sandboxSpot();
            if (!spot) { log("No room for a chest."); break; }
            if (chestExists_ && map_.inBounds(chestPosition_.x, chestPosition_.y)) map_.setTile(chestPosition_.x, chestPosition_.y, Tile{TileType::Floor, true, true});
            chestPosition_ = *spot; chestExists_ = true; chestClaimed_ = false; chestMimic_ = action == 4;
            map_.setTile(spot->x, spot->y, Tile{TileType::Floor, false, true});
            updateFieldOfView();
            log(action == 4 ? "A chest appears. It breathes." : "A chest appears.");
            break;
        }
        case 5: case 6: {
            const int floor = std::clamp(currentFloor_ + (action == 6 ? 1 : -1), 1, kRunFinalFloor);
            if (floor == currentFloor_) break;
            sandboxMenu_ = false;
            currentFloor_ = floor;
            regenerateLevel(freshSeed());
            log("Floor ", floor, ".");
            break;
        }
    }
}

// Travel: straight to any depth of any dungeon, locks and all ignored.
void Application::sandboxTravel(int dungeon, int depth) {
    sandboxMenu_ = false;
    dissolveMinions();
    trial_ = 0;
    currentFloor_ = dungeonFirstFloor(dungeon) + depth - 1;
    mode_ = GameMode::Playing; dungeonMenu_ = false;
    regenerateLevel(freshSeed());
    callPackBack();
    log("You step through to ", dungeonName(dungeon), ", depth ", depth, ".");
}

void Application::handleSandboxKey(sf::Keyboard::Key key) {
    if (key == sf::Keyboard::Key::Escape || key == sf::Keyboard::Key::F1) { sandboxMenu_ = false; return; }
    if (key == sf::Keyboard::Key::Tab) { sandboxTab_ = (sandboxTab_ + 1) % kSandboxTabs; sandboxPage_ = 0; }
}

void Application::handleSandboxMouse(const sf::Event& event) {
    if (const auto* move = event.getIf<sf::Event::MouseMoved>()) mousePixel_ = move->position;
    const auto* click = event.getIf<sf::Event::MouseButtonPressed>();
    if (!click || click->button != sf::Mouse::Button::Left) return;
    const auto p = sf::Vector2f(click->position);
    if (!kPanel.contains(p)) { sandboxMenu_ = false; return; } // a click outside closes it
    for (int i = 0; i < kSandboxTabs; ++i) if (sandboxTab(i).contains(p)) { sandboxTab_ = i; sandboxPage_ = 0; return; }
    if (sandboxTab_ == 0) {
        for (int i = 0; i < 3; ++i) if (sandboxToggle(i, 4).contains(p)) { sandboxTier_ = i; return; }
        if (sandboxToggle(3, 4).contains(p)) { sandboxAwake_ = !sandboxAwake_; return; }
        for (int t = 0; t < kMonsterTypes; ++t) if (sandboxCell(t).contains(p)) { sandboxSpawn(t); return; }
    }
    if (sandboxTab_ == 1) {
        for (int i = 0; i < 4; ++i) if (sandboxToggle(i, 4).contains(p)) { sandboxItemGroup_ = i; sandboxPage_ = 0; return; }
        for (int i = 0; i < 3; ++i) if (sandboxRarity(i).contains(p)) { sandboxRarity_ = i; return; }
        const auto items = itemGroup(sandboxItemGroup_);
        const int pages = std::max(1, (static_cast<int>(items.size()) + kItemsPerPage - 1) / kItemsPerPage);
        if (kPagePrev.contains(p)) { sandboxPage_ = (sandboxPage_ + pages - 1) % pages; return; }
        if (kPageNext.contains(p)) { sandboxPage_ = (sandboxPage_ + 1) % pages; return; }
        for (int i = 0; i < kItemsPerPage; ++i) {
            const int index = sandboxPage_ * kItemsPerPage + i;
            if (index < static_cast<int>(items.size()) && sandboxItemCell(i).contains(p)) { sandboxItem(*items[static_cast<std::size_t>(index)]); return; }
        }
    }
    if (sandboxTab_ == 2) for (int i = 0; i < kCharacterActions; ++i) if (sandboxButton(i).contains(p)) { sandboxCharacter(i); return; }
    if (sandboxTab_ == 3) for (int i = 0; i < kWorldActions; ++i) if (sandboxButton(i).contains(p)) { sandboxWorld(i); return; }
    if (sandboxTab_ == 4)
        for (int d = 0; d < kDungeonCount; ++d)
            for (int depth = 1; depth <= dungeonLength(d); ++depth)
                if (travelDepth(d, depth).contains(p)) { sandboxTravel(d, depth); return; }
}

void Application::renderSandbox() {
    if (!sandboxMenu_) return;
    const auto mouse = mousePixel_ ? std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)) : std::nullopt;
    const auto hovered = [&](const sf::FloatRect& r) { return mouse && r.contains(*mouse); };
    ui_.glass(window_, kPanel, true);
    // The chosen tab or toggle sits pressed in, its label in gold.
    const auto toggle = [&](sf::FloatRect r, const std::string& label, bool on, unsigned size = 14) {
        if (!on) { ui_.button(window_, r, label, hovered(r), true, size); return; }
        ui_.inset(window_, r, ui::kGold);
        ui_.textCentered(window_, label, r, size, ui::kGold, ui::Font::Bold);
    };
    static const char* tabs[]{"Monsters", "Items", "Character", "World", "Travel"};
    for (int i = 0; i < kSandboxTabs; ++i) toggle(sandboxTab(i), tabs[i], sandboxTab_ == i, 14);
    if (sandboxTab_ == 0) {
        static const char* tiers[]{"Normal", "Elite", "Nightmare"};
        for (int i = 0; i < 3; ++i) toggle(sandboxToggle(i, 4), tiers[i], sandboxTier_ == i);
        toggle(sandboxToggle(3, 4), sandboxAwake_ ? "Awake" : "Asleep", sandboxAwake_);
        for (int t = 0; t < kMonsterTypes; ++t) {
            const auto cell = sandboxCell(t);
            if (hovered(cell)) ui_.inset(window_, cell, ui::kBronze);
            ui_.text(window_, monsterName(t), {cell.position.x + 4, cell.position.y + 2}, 13, hovered(cell) ? ui::kGold : ui::kText);
        }
        ui_.text(window_, "Click a foe: it appears beside you.", {806, 576}, 14, ui::kMuted);
    }
    if (sandboxTab_ == 1) {
        for (int i = 0; i < 4; ++i) toggle(sandboxToggle(i, 4), itemGroupName(i), sandboxItemGroup_ == i);
        static const char* rarities[]{"Plain", "Magic", "Rare"};
        const sf::Color shades[]{ui::kText, ui::kMagic, ui::kRare};
        for (int i = 0; i < 3; ++i) toggle(sandboxRarity(i), rarities[i], sandboxRarity_ == i);
        const auto items = itemGroup(sandboxItemGroup_);
        const int pages = std::max(1, (static_cast<int>(items.size()) + kItemsPerPage - 1) / kItemsPerPage);
        for (int i = 0; i < kItemsPerPage; ++i) {
            const int index = sandboxPage_ * kItemsPerPage + i;
            if (index >= static_cast<int>(items.size())) break;
            const auto& d = *items[static_cast<std::size_t>(index)];
            const auto cell = sandboxItemCell(i);
            if (hovered(cell)) ui_.inset(window_, cell, ui::kBronze);
            ui_.text(window_, d.name, {cell.position.x + 6, cell.position.y + 3}, 14, d.unique ? ui::kUnique : shades[sandboxRarity_]);
        }
        ui_.button(window_, kPagePrev, "<", hovered(kPagePrev), pages > 1);
        ui_.button(window_, kPageNext, ">", hovered(kPageNext), pages > 1);
        ui_.textCentered(window_, "Page " + std::to_string(sandboxPage_ + 1) + " of " + std::to_string(pages), {{866, 640}, {318, 30}}, 14, ui::kMuted);
    }
    if (sandboxTab_ == 2) {
        for (int i = 0; i < kCharacterActions; ++i) ui_.button(window_, sandboxButton(i), characterAction(i, sandboxGod_), hovered(sandboxButton(i)), true, 15);
        float y = 130 + 40 * ((kCharacterActions + 1) / 2) + 10;
        const auto& s = player_.stats();
        ui_.paragraph(window_, "Level " + std::to_string(player_.level()) + "  ·  Ability points " + std::to_string(player_.abilityPoints()) +
                      "  ·  Utility " + std::to_string(player_.utilityPoints()) + "  ·  Tree " + std::to_string(player_.treePoints()),
                      806, y, 438, 15, ui::kGold, ui::Font::Bold);
        ui_.paragraph(window_, "Str " + std::to_string(s.strength) + "  ·  Dex " + std::to_string(s.dexterity) + "  ·  Int " + std::to_string(s.intelligence) +
                      "  ·  Life " + std::to_string(s.hp) + "/" + std::to_string(s.maxHp) + "  ·  Mana " + std::to_string(s.mana) + "/" + std::to_string(s.maxMana),
                      806, y, 438, 15, ui::kText);
        ui_.paragraph(window_, "In the sandbox any tree can be opened, hidden ones included; they still cost points.", 806, y, 438, 14, ui::kMuted);
    }
    if (sandboxTab_ == 3) {
        for (int i = 0; i < kWorldActions; ++i) ui_.button(window_, sandboxButton(i), worldAction(i), hovered(sandboxButton(i)), true, 15);
        float y = 130 + 40 * ((kWorldActions + 1) / 2) + 10;
        ui_.paragraph(window_, "Floor " + std::to_string(currentFloor_) + ", " + std::to_string(monsters_.size()) + " foes here.", 806, y, 438, 15, ui::kText);
    }
    if (sandboxTab_ == 4) {
        const int here = mode_ == GameMode::Playing && !trial_ ? dungeonIndex(currentFloor_) : -1;
        for (int d = 0; d < kDungeonCount; ++d) {
            const float top = travelDepth(d, 1).position.y;
            ui_.text(window_, dungeonName(d), {806, top - 24}, 16, ui::kGold, ui::Font::Title);
            for (int depth = 1; depth <= dungeonLength(d); ++depth) {
                const auto r = travelDepth(d, depth);
                const bool now = here == d && floorInDungeon(currentFloor_) == depth;
                const int f = dungeonFirstFloor(d) + depth - 1;
                const bool boss = f == 5 || f == 10 || f == kRunFinalFloor || f == kCathedralLast || f == kFoundryLast || f == kThornLast || f == kRimeLast;
                toggle(r, std::to_string(depth), now, 14);
                if (boss) ui_.icon(window_, "skull-crossed-bones", {{r.position.x + r.size.x - 13, r.position.y + 2}, {11, 11}}, ui::kBad);
            }
        }
        float hintY = 560.f;
        ui_.paragraph(window_, "Click a depth to go there at once. The skull marks a boss.", 806, hintY, 438, 14, ui::kMuted);
    }
}

} // namespace engine
