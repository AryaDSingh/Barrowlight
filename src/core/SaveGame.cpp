#include "core/SaveGame.hpp"

#include <algorithm>
#include <fstream>
#include <array>
#include <unordered_set>
#include "entities/Item.hpp"

namespace engine {

namespace {

// Version 2: added Stats::intelligence (missed when it was introduced
// at Prompt 14 -- a real gap, caught while touching this file again for
// playerClass below) and playerClass (Prompt 15's multi-class system --
// needed so a loaded save's playerCooldowns are applied to the right
// class's talent list, not whatever player_ happened to be configured
// as at the moment F9 was pressed). A version-1 save is simply
// rejected, not migrated -- see ARCHITECTURE_DECISIONS.md, "Save/load,"
// for why that's an acceptable simplification at this scale.
// Version 3: added playerLevel/playerXp (Prompt 20's leveling system).
// Same "reject outright, don't migrate" policy as the version-1 -> 2
// jump above.
// Version 4: added playerHybridPickNames/playerHybridSpecced (Prompt
// 24's hybrid path). Same "reject outright, don't migrate" policy as
// every prior version bump.
// Version 5: added currentFloor (the multi-floor dungeon progression).
// Same "reject outright, don't migrate" policy as every prior version
// bump.
// Version 6: base stats, current pools, owned/ground items and next instance ID.
constexpr int kSaveFormatVersion = 8; // stable talent IDs, support rune ownership and attachments

void writeTalentStates(std::ofstream& out, const std::vector<SaveGameState::TalentSaveData>& talents) {
    out << talents.size() << '\n';
    for (const auto& talent : talents) out << talent.id << ' ' << talent.cooldown << '\n';
}
bool readTalentStates(std::ifstream& in, std::vector<SaveGameState::TalentSaveData>& talents) {
    std::size_t count = 0;
    if (!(in >> count) || count > 64) return false;
    std::unordered_set<std::string> ids;
    for (std::size_t i = 0; i < count; ++i) {
        SaveGameState::TalentSaveData talent;
        if (!(in >> talent.id >> talent.cooldown) || talent.id.size() > 100 ||
            talent.cooldown < 0 || talent.cooldown > 10000 || !ids.insert(talent.id).second) return false;
        talents.push_back(std::move(talent));
    }
    return true;
}

bool validItems(const SaveGameState& state) {
    std::unordered_set<std::uint64_t> ids;
    std::array<bool, 3> occupied{};
    if (state.nextItemId == 0 || state.items.size() > 100000 || state.runes.size() > 100000 || state.unspentAttributePoints < 0)
        return false;
    if (!state.lootRngState || state.ordinaryDrops < 0 || state.ordinaryDrops > 2 ||
        (state.chestExists && !state.map.isWalkable(state.chestPosition.x, state.chestPosition.y))) return false;
    for (const auto& item : state.items) {
        const auto* definition = findItemDefinition(item.definitionId);
        if (!definition || item.instanceId == 0 || item.instanceId >= state.nextItemId ||
            !ids.insert(item.instanceId).second || item.location < -2 || item.location > 2)
            return false;
        if (item.affixes.size() > 2 || item.rollTier < 0 || item.rollTier > 5) return false;
        unsigned int usedStats = 0;
        for (const auto& rolled : item.affixes) {
            const auto* affix = findAffix(rolled.id);
            if (!affix || !(affix->slots & (1u << static_cast<unsigned>(definition->slot)))) return false;
            const unsigned int mask = 1u << static_cast<unsigned>(affix->stat);
            if ((usedStats & mask) || rolled.value < affix->minimum + item.rollTier * affix->perTier ||
                rolled.value > affix->maximum + item.rollTier * affix->perTier) return false;
            usedStats |= mask;
        }
        if (item.location >= 0) {
            if (occupied[item.location] || static_cast<int>(definition->slot) != item.location) return false;
            occupied[item.location] = true;
        } else if (item.location == -2 && !state.map.isWalkable(item.position.x, item.position.y)) {
            return false;
        }
    }
    std::unordered_set<std::string> attached;
    for (const auto& rune : state.runes) {
        if (!findRune(rune.definitionId) || !rune.instanceId || rune.instanceId >= state.nextItemId ||
            !ids.insert(rune.instanceId).second) return false;
        if (!rune.talentId.empty()) {
            if (!attached.insert(rune.talentId).second || std::none_of(state.playerTalents.begin(), state.playerTalents.end(),
                [&](const auto& talent) { return talent.id == rune.talentId; })) return false;
        }
    }
    return true;
}

char tileChar(const Tile& t) {
    if (t.type == TileType::Wall) {
        return '#';
    }
    if (t.type == TileType::Door) {
        return 'D';
    }
    return '.';
}

Tile charToTile(char c) {
    if (c == '#') {
        return Tile{TileType::Wall, false, false};
    }
    if (c == 'D') {
        return Tile{TileType::Door, true, true}; // walkable/transparent exactly like Floor
    }
    return Tile{TileType::Floor, true, true};
}

char visibilityChar(Visibility v) {
    switch (v) {
        case Visibility::Visible:
            return 'V';
        case Visibility::Remembered:
            return 'R';
        case Visibility::Hidden:
        default:
            return 'H';
    }
}

Visibility charToVisibility(char c) {
    if (c == 'V') {
        return Visibility::Visible;
    }
    if (c == 'R') {
        return Visibility::Remembered;
    }
    return Visibility::Hidden;
}

void writeStatusEffects(std::ofstream& out, const std::vector<StatusEffectInstance>& effects) {
    out << effects.size() << '\n';
    for (const StatusEffectInstance& e : effects) {
        out << static_cast<int>(e.type) << ' ' << e.turnsRemaining << ' ' << e.magnitude << '\n';
    }
}

bool readStatusEffects(std::ifstream& in, std::vector<StatusEffectInstance>& effects) {
    std::size_t count = 0;
    if (!(in >> count) || count > 1024) {
        return false;
    }
    effects.clear();
    effects.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        int type = 0;
        int turnsRemaining = 0;
        int magnitude = 0;
        if (!(in >> type >> turnsRemaining >> magnitude)) {
            return false;
        }
        effects.push_back(
            StatusEffectInstance{static_cast<StatusEffectType>(type), turnsRemaining, magnitude});
    }
    return true;
}

} // namespace

bool saveGame(const SaveGameState& state, const std::string& path) {
    if (!validItems(state)) return false;
    std::ofstream out(path);
    if (!out.is_open()) {
        return false;
    }

    out << "ROGUELIKE_SAVE " << kSaveFormatVersion << '\n';

    const int width = state.map.width();
    const int height = state.map.height();
    out << width << ' ' << height << '\n';

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            out << tileChar(state.map.tileAt(x, y));
        }
        out << '\n';
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            out << visibilityChar(state.exploredMap.at(x, y));
        }
        out << '\n';
    }

    out << state.playerPosition.x << ' ' << state.playerPosition.y << '\n';
    out << static_cast<int>(state.playerClass) << '\n';
    out << state.playerLevel << ' ' << state.playerXp << '\n';
    out << state.currentFloor << '\n';
    out << state.playerStats.hp << ' ' << state.playerStats.maxHp << ' '
        << state.playerStats.mana << ' ' << state.playerStats.maxMana << ' '
        << state.playerStats.strength << ' ' << state.playerStats.dexterity << ' '
        << state.playerStats.intelligence << ' ' << state.playerStats.speed << '\n';
    out << state.lastMoveDirection.x << ' ' << state.lastMoveDirection.y << '\n';

    writeTalentStates(out, state.playerTalents);
    out << state.playerHybridSpecced << '\n';

    writeStatusEffects(out, state.playerStatusEffects);

    out << state.monsters.size() << '\n';
    for (const SaveGameState::MonsterSaveData& m : state.monsters) {
        out << static_cast<int>(m.type) << ' ' << m.position.x << ' ' << m.position.y << ' '
            << m.hp << ' ' << m.maxHp << ' ' << (m.isBoss ? 1 : 0) << ' '
            << static_cast<int>(m.tier) << ' ' << m.rewardsEligible << '\n';
        writeStatusEffects(out, m.statusEffects);
        writeTalentStates(out, m.talents);
    }

    out << state.unspentAttributePoints << ' ' << state.nextItemId << ' ' << state.items.size() << '\n';
    for (const auto& item : state.items) {
        out << item.definitionId << ' ' << item.instanceId << ' ' << item.location << ' '
            << item.position.x << ' ' << item.position.y << ' ' << item.rollTier << ' ' << item.affixes.size();
        for (const auto& affix : item.affixes) out << ' ' << affix.id << ' ' << affix.value;
        out << '\n';
    }
    out << state.lootRngState << ' ' << state.ordinaryDrops << ' ' << state.chestExists << ' '
        << state.chestClaimed << ' ' << state.chestPosition.x << ' ' << state.chestPosition.y << '\n';
    out << state.runeChoiceAvailable << ' ' << state.runes.size() << '\n';
    for (const auto& rune : state.runes)
        out << rune.definitionId << ' ' << rune.instanceId << ' ' << (rune.talentId.empty() ? "-" : rune.talentId) << '\n';
    return static_cast<bool>(out);
}

std::optional<SaveGameState> loadGame(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) {
        return std::nullopt;
    }

    std::string tag;
    int version = 0;
    if (!(in >> tag >> version) || tag != "ROGUELIKE_SAVE" || version != kSaveFormatVersion) {
        return std::nullopt;
    }

    int width = 0;
    int height = 0;
    if (!(in >> width >> height) || width <= 0 || height <= 0 || width > 512 || height > 512) {
        return std::nullopt;
    }

    SaveGameState state;
    state.map = Map(width, height);

    for (int y = 0; y < height; ++y) {
        std::string row;
        if (!(in >> row) || static_cast<int>(row.size()) != width) {
            return std::nullopt;
        }
        for (int x = 0; x < width; ++x) {
            state.map.setTile(x, y, charToTile(row[static_cast<std::size_t>(x)]));
        }
    }

    std::vector<Visibility> visibility;
    visibility.reserve(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        std::string row;
        if (!(in >> row) || static_cast<int>(row.size()) != width) {
            return std::nullopt;
        }
        for (int x = 0; x < width; ++x) {
            visibility.push_back(charToVisibility(row[static_cast<std::size_t>(x)]));
        }
    }
    state.exploredMap = ExploredMap(state.map);
    state.exploredMap.restoreAll(std::move(visibility));

    if (!(in >> state.playerPosition.x >> state.playerPosition.y)) {
        return std::nullopt;
    }
    int playerClassValue = 0;
    if (!(in >> playerClassValue) || playerClassValue < 0 || playerClassValue > static_cast<int>(PlayerClass::Mage)) {
        return std::nullopt;
    }
    state.playerClass = static_cast<PlayerClass>(playerClassValue);
    if (!(in >> state.playerLevel >> state.playerXp) || state.playerLevel < 1 || state.playerLevel > 10 || state.playerXp < 0) {
        return std::nullopt;
    }
    if (!(in >> state.currentFloor) || state.currentFloor < 1 || state.currentFloor > 10 ||
        !state.map.isWalkable(state.playerPosition.x, state.playerPosition.y)) {
        return std::nullopt;
    }
    if (!(in >> state.playerStats.hp >> state.playerStats.maxHp >> state.playerStats.mana >>
          state.playerStats.maxMana >> state.playerStats.strength >>
          state.playerStats.dexterity >> state.playerStats.intelligence >>
          state.playerStats.speed)) {
        return std::nullopt;
    }
    if (!(in >> state.lastMoveDirection.x >> state.lastMoveDirection.y)) {
        return std::nullopt;
    }

    if (!readTalentStates(in, state.playerTalents) || !(in >> state.playerHybridSpecced))
        return std::nullopt;

    if (!readStatusEffects(in, state.playerStatusEffects)) {
        return std::nullopt;
    }

    std::size_t numMonsters = 0;
    if (!(in >> numMonsters) || numMonsters > 100000) {
        return std::nullopt;
    }
    state.monsters.reserve(numMonsters);
    for (std::size_t i = 0; i < numMonsters; ++i) {
        SaveGameState::MonsterSaveData m;
        int type = 0;
        int isBoss = 0;
        int tier = 0;
        if (!(in >> type >> m.position.x >> m.position.y >> m.hp >> m.maxHp >> isBoss >> tier >> m.rewardsEligible) ||
            tier < 0 || tier > 2 || type < 0 || type > static_cast<int>(MonsterType::Skeleton) ||
            !state.map.isWalkable(m.position.x, m.position.y) || m.maxHp <= 0 || m.hp <= 0 || m.hp > m.maxHp) {
            return std::nullopt;
        }
        m.type = static_cast<MonsterType>(type);
        m.isBoss = (isBoss != 0);
        m.tier = static_cast<MonsterTier>(tier);
        if (!readStatusEffects(in, m.statusEffects)) {
            return std::nullopt;
        }
        if (!readTalentStates(in, m.talents)) return std::nullopt;
        state.monsters.push_back(std::move(m));
    }

    std::size_t itemCount = 0;
    if (!(in >> state.unspentAttributePoints >> state.nextItemId >> itemCount) || itemCount > 100000)
        return std::nullopt;
    state.items.reserve(itemCount);
    for (std::size_t i = 0; i < itemCount; ++i) {
        SaveGameState::ItemSaveData item;
        std::size_t affixCount = 0;
        if (!(in >> item.definitionId >> item.instanceId >> item.location >> item.position.x >> item.position.y >> item.rollTier >> affixCount) || affixCount > 2)
            return std::nullopt;
        for (std::size_t j = 0; j < affixCount; ++j) {
            RolledAffix affix;
            if (!(in >> affix.id >> affix.value)) return std::nullopt;
            item.affixes.push_back(std::move(affix));
        }
        state.items.push_back(std::move(item));
    }
    if (!(in >> state.lootRngState >> state.ordinaryDrops >> state.chestExists >> state.chestClaimed >> state.chestPosition.x >> state.chestPosition.y))
        return std::nullopt;
    std::size_t runeCount = 0;
    if (!(in >> state.runeChoiceAvailable >> runeCount) || runeCount > 100000) return std::nullopt;
    for (std::size_t i = 0; i < runeCount; ++i) {
        RuneInstance rune;
        if (!(in >> rune.definitionId >> rune.instanceId >> rune.talentId)) return std::nullopt;
        if (rune.talentId == "-") rune.talentId.clear();
        state.runes.push_back(std::move(rune));
    }
    if (!validItems(state)) return std::nullopt;
    const auto& stats = state.playerStats;
    if (stats.maxHp <= 0 || stats.maxHp > 100000 || stats.maxMana < 0 || stats.maxMana > 100000 ||
        stats.strength < 0 || stats.strength > 100000 || stats.dexterity < 0 || stats.dexterity > 100000 ||
        stats.intelligence < 0 || stats.intelligence > 100000 || stats.speed <= 0 || stats.speed > 100000)
        return std::nullopt;
    int maxHp = stats.maxHp, maxMana = stats.maxMana;
    for (const auto& item : state.items) {
        if (item.location < 0) continue;
        const auto bonus = Item(*findItemDefinition(item.definitionId), item.instanceId, {}, item.affixes, item.rollTier).bonuses();
        maxHp += bonus.maxHp; maxMana += bonus.maxMana;
    }
    if (stats.hp < 0 || stats.hp > maxHp || stats.mana < 0 || stats.mana > maxMana)
        return std::nullopt;
    return state;
}

} // namespace engine
