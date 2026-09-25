#include "core/SaveGame.hpp"

#include <algorithm>
#include <fstream>

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
constexpr int kSaveFormatVersion = 5;

char tileChar(const Tile& t) {
    if (t.type == TileType::Wall) {
        return '#';
    }
    if (t.type == TileType::Door) {
        return 'D';
    }
    return '.';
}

// Prompt 24: every other field in this format is a single
// whitespace-delimited token (`>>`-read), but a talent name can contain
// spaces ("Arcane Bolt"). Rather than introduce a genuinely different
// line-based read for just this one field, spaces are escaped to
// underscores on write and restored on read -- safe because no talent
// name in this project ever contains an underscore naturally, so the
// round-trip is unambiguous.
std::string escapeTalentName(const std::string& name) {
    std::string escaped = name;
    std::replace(escaped.begin(), escaped.end(), ' ', '_');
    return escaped;
}

std::string unescapeTalentName(const std::string& escaped) {
    std::string name = escaped;
    std::replace(name.begin(), name.end(), '_', ' ');
    return name;
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
    if (!(in >> count)) {
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

    out << state.playerCooldowns.size() << '\n';
    for (int cd : state.playerCooldowns) {
        out << cd << ' ';
    }
    out << '\n';

    out << (state.playerHybridSpecced ? 1 : 0) << '\n';
    out << state.playerHybridPickNames.size() << '\n';
    for (const std::string& name : state.playerHybridPickNames) {
        out << escapeTalentName(name) << ' ';
    }
    out << '\n';

    writeStatusEffects(out, state.playerStatusEffects);

    out << state.monsters.size() << '\n';
    for (const SaveGameState::MonsterSaveData& m : state.monsters) {
        out << static_cast<int>(m.type) << ' ' << m.position.x << ' ' << m.position.y << ' '
            << m.hp << ' ' << m.maxHp << ' ' << (m.isBoss ? 1 : 0) << '\n';
        writeStatusEffects(out, m.statusEffects);
    }

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
    if (!(in >> width >> height) || width <= 0 || height <= 0) {
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
    if (!(in >> playerClassValue)) {
        return std::nullopt;
    }
    state.playerClass = static_cast<PlayerClass>(playerClassValue);
    if (!(in >> state.playerLevel >> state.playerXp)) {
        return std::nullopt;
    }
    if (!(in >> state.currentFloor)) {
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

    std::size_t numCooldowns = 0;
    if (!(in >> numCooldowns)) {
        return std::nullopt;
    }
    state.playerCooldowns.resize(numCooldowns);
    for (std::size_t i = 0; i < numCooldowns; ++i) {
        if (!(in >> state.playerCooldowns[i])) {
            return std::nullopt;
        }
    }

    int hybridSpeccedValue = 0;
    if (!(in >> hybridSpeccedValue)) {
        return std::nullopt;
    }
    state.playerHybridSpecced = (hybridSpeccedValue != 0);

    std::size_t numHybridPicks = 0;
    if (!(in >> numHybridPicks)) {
        return std::nullopt;
    }
    state.playerHybridPickNames.resize(numHybridPicks);
    for (std::size_t i = 0; i < numHybridPicks; ++i) {
        std::string escapedName;
        if (!(in >> escapedName)) {
            return std::nullopt;
        }
        state.playerHybridPickNames[i] = unescapeTalentName(escapedName);
    }

    if (!readStatusEffects(in, state.playerStatusEffects)) {
        return std::nullopt;
    }

    std::size_t numMonsters = 0;
    if (!(in >> numMonsters)) {
        return std::nullopt;
    }
    state.monsters.reserve(numMonsters);
    for (std::size_t i = 0; i < numMonsters; ++i) {
        SaveGameState::MonsterSaveData m;
        int type = 0;
        int isBoss = 0;
        if (!(in >> type >> m.position.x >> m.position.y >> m.hp >> m.maxHp >> isBoss)) {
            return std::nullopt;
        }
        m.type = static_cast<MonsterType>(type);
        m.isBoss = (isBoss != 0);
        if (!readStatusEffects(in, m.statusEffects)) {
            return std::nullopt;
        }
        state.monsters.push_back(std::move(m));
    }

    return state;
}

} // namespace engine
