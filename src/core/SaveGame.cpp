#include "core/SaveGame.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <array>
#include <iomanip>
#include <unordered_set>
#include "entities/Item.hpp"
#include "entities/RunProgression.hpp"
#include "world/Landmark.hpp"
#include "world/Surfaces.hpp"
#include "entities/Ascendancy.hpp"
#include <bitset>
#include "entities/HiddenTrees.hpp"

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
// Version 19 adds persistent first-entry levels for the two dungeon bands.
// Version 18 expands equipment to eleven slots; original slot IDs remain stable.
// Version 17 adds curses, the Lich hex cooldown, and a wider Warlord cleave.
// Version 16 appended enemy types and larger blast areas.
// Version 21 adds death mode and remaining extra lives. Older runs remain Roguelike.
// Version 20 replaces entry-level scaling with fixed global-depth scaling.
constexpr int kSaveFormatVersion = 31;

void writeTalentStates(std::ostream& out, const std::vector<SaveGameState::TalentSaveData>& talents) {
    out << talents.size() << '\n';
    for (const auto& talent : talents) out << talent.id << ' ' << talent.cooldown << ' ' << talent.rank << '\n';
}
bool readTalentStates(std::istream& in, std::vector<SaveGameState::TalentSaveData>& talents) {
    std::size_t count = 0;
    if (!(in >> count) || count > 128) return false;
    std::unordered_set<std::string> ids;
    for (std::size_t i = 0; i < count; ++i) {
        SaveGameState::TalentSaveData talent;
        if (!(in >> talent.id >> talent.cooldown >> talent.rank) || talent.id.size() > 100 ||
            talent.rank<1 || talent.rank>kMaxTalentRank || talent.cooldown < 0 || talent.cooldown > 10000 || !ids.insert(talent.id).second) return false;
        talents.push_back(std::move(talent));
    }
    return true;
}

bool validItems(const SaveGameState& state) {
    std::unordered_set<std::uint64_t> ids;
    std::array<bool, kEquipmentSlotCount> occupied{};
    std::array<bool, 3> vaultRewards{};
    if (state.nextItemId == 0 || state.items.size() > 100000 || state.unspentAttributePoints < 0)
        return false;
    if (!state.lootRngState || state.ordinaryDrops < 0 || state.ordinaryDrops > 2 ||
        (state.chestExists && !state.map.isWalkable(state.chestPosition.x, state.chestPosition.y))) return false;
    for (const auto& item : state.items) {
        const auto* definition = findItemDefinition(item.definitionId);
        if (!definition || item.instanceId == 0 || item.instanceId >= state.nextItemId ||
            !ids.insert(item.instanceId).second || item.location < -5 || item.location >= kEquipmentSlotCount)
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
        if (item.location<=-3) {
            const auto slot=static_cast<std::size_t>(-3-item.location);
            if (vaultRewards[slot] || !state.vaultExists || state.vaultClaimed) return false;
            vaultRewards[slot]=true;
        }
        if (item.location >= 0) {
            if (occupied[item.location] || !slotAccepts(static_cast<EquipmentSlot>(item.location),definition->slot)) return false;
            occupied[item.location] = true;
        } else if (item.location == -2 && !state.map.isWalkable(item.position.x, item.position.y)) {
            return false;
        }
    }
    const int guardCount=static_cast<int>(std::count_if(state.monsters.begin(),state.monsters.end(),[](const auto& m){return m.vaultGuard;}));
    if (state.vaultExists) {
        if (state.currentFloor<3 || state.currentFloor==5 || state.currentFloor==10 || state.currentFloor==kRunFinalFloor ||
            !state.map.isWalkable(state.vaultCenter.x,state.vaultCenter.y) ||
            !state.map.inBounds(state.vaultEntrance.x,state.vaultEntrance.y) ||
            state.map.isWalkable(state.vaultEntrance.x,state.vaultEntrance.y)!=state.vaultOpened ||
            std::abs(state.vaultCenter.x-state.vaultEntrance.x)+std::abs(state.vaultCenter.y-state.vaultEntrance.y)!=3 ||
            guardCount>2 || (!state.vaultOpened && guardCount!=2) ||
            (state.vaultClaimed && (!state.vaultOpened || guardCount!=0))) return false;
        if (!state.vaultClaimed && !std::all_of(vaultRewards.begin(),vaultRewards.end(),[](bool v){return v;})) return false;
    } else if (state.vaultOpened || state.vaultClaimed || guardCount) return false;
    bool shield=false,twoHanded=false;
    for (const auto& item:state.items) {
        if (item.location==3) shield=true;
        if (item.location==0) { const auto kind=findItemDefinition(item.definitionId)->weaponKind; twoHanded=kind==WeaponKind::TwoHanded || kind==WeaponKind::Bow; }
    }
    if (shield && twoHanded) return false;
    return true;
}

// Validate progression against earned budgets before committing a loaded run.
bool validProgression(const SaveGameState& s) {
    if (s.playerLevel<1 || s.playerLevel>kRunMaxLevel || s.treePoints<0 || s.abilityPoints<0 || s.trees.size()>kTalentTrees.size() || s.hotbar.size()>18) return false;
    if (s.playerClass==PlayerClass::Spellblade) return false;
    int treeSpent=0,abilitySpent=0,ascendancyNodes=0;
    const int fullTrials=(1<<kTrialCount)-1;
    if (s.trialKeys<0 || s.trialKeys>fullTrials || s.trialsCleared<0 || (s.trialsCleared & ~s.trialKeys) ||
        ((s.trialsCleared & 2) && !(s.trialsCleared & 1)) || s.ascendancyPoints<0) return false;
    if (!s.ascendancy.empty() && !s.trialsCleared) return false; // an ascendancy needs a trial won (choosing may still be pending)
    if (!s.ascendancy.empty()) {
        const auto* a=findAscendancy(s.ascendancy);
        if (!a || !ascendancyAllowed(*a,s.playerClass)) return false;
    }
    std::unordered_set<std::string> trees,known,bound;
    for (std::size_t i=0;i<s.trees.size();++i) {
        const auto& access=s.trees[i]; const auto* d=findTree(access.id);
        if (!d || !trees.insert(access.id).second || (i==0 && !startingTreeAllowed(s.playerClass,d->tree))) return false;
        if ((i>0 || access.specialized) && s.playerLevel<5) return false;
        treeSpent+=access.specialized?2:1;
    }
    if (treeSpent+s.treePoints != earnedTreePoints(s.playerLevel)) return false;
    // Hidden trees are locked to new purchases, but saves that already own
    // one keep it.
    for (const auto& t:s.playerTalents) {
        if (!known.insert(t.id).second || t.rank<1 || t.rank>kMaxTalentRank || t.cooldown<0 || t.cooldown>10000) return false;
        if (t.id=="basic.attack" || t.id=="basic.cleanse" || t.id=="basic.light") { if (t.rank!=1) return false; continue; }
        const auto* d=findTalentDefinition(t.id);
        if (d && isAscendancyTree(d->treeId)) {
            if (d->treeId!=s.ascendancy || t.rank!=1 || (d->ranks[0].passive && t.cooldown)) return false;
            ++ascendancyNodes;
            continue;
        }
        if (!d || !trees.count(d->treeId)) return false;
        if (isImbueVariant(t.id)) {
            const auto base=std::find_if(s.playerTalents.begin(),s.playerTalents.end(),[](const auto& other){return other.id=="spellblade.imbue";});
            if (base==s.playerTalents.end() || base->rank!=t.rank || base->cooldown!=t.cooldown) return false;
            const char* parents[]{"fire","ice","lightning","arcane"};
            if (!trees.count(parents[d->ranks[0].imbueElement-1])) return false;
            continue;
        }
        constexpr int levels[]{1,1,4,5};
        if (s.playerLevel<levels[d->tier]) return false;
        if (d->ranks[0].passive && t.cooldown) return false;
        int investment=0;
        for (const auto& other:s.playerTalents) {
            const auto* od=findTalentDefinition(other.id);
            // Only earlier nodes can establish the prerequisites of a learned node.
            if (od && !isImbueVariant(od->id) && od->treeId==d->treeId && od->tier<d->tier) investment+=other.rank;
        }
        constexpr int required[]{0,1,3,4};
        if (investment<required[d->tier]) return false;
        if (d->tier==3) {
            const auto access=std::find_if(s.trees.begin(),s.trees.end(),[&](const auto& a){return a.id==d->treeId;});
            if (!access->specialized) return false;
        }
        abilitySpent+=t.rank;
    }
    if (ascendancyNodes+s.ascendancyPoints!=static_cast<int>(std::bitset<8>(static_cast<unsigned>(s.trialsCleared)).count())) return false;
    if (s.trees.empty() && !s.progressionReviewPending) return false;
    if (!known.count("basic.attack") || !known.count("basic.cleanse") || abilitySpent+s.abilityPoints!=earnedAbilityPoints(s.playerLevel)) return false;
    for (const auto& tree:s.trees) if (tree.specialized) {
        int investment=0;
        for (const auto& t:s.playerTalents) { const auto* d=findTalentDefinition(t.id); if (d && !isImbueVariant(d->id) && d->treeId==tree.id && d->tier<3) investment+=t.rank; }
        if (investment<4) return false;
    }
    for (const auto& id:s.hotbar) if (!id.empty()) {
        const auto* d=findTalentDefinition(id);
        if (!known.count(id) || !bound.insert(id).second || (d && d->ranks[0].passive)) return false;
    }
    std::unordered_set<int> spent;
    for (int floor:s.deathlessSpentFloors) if (floor<1 || floor>kRunFinalFloor || !spent.insert(floor).second) return false;
    if (s.unspentAttributePoints<0 || s.unspentAttributePoints>2*(s.playerLevel-1)) return false;
    if (s.pendingFinalVictory && (s.currentFloor!=kRunFinalFloor || s.defeatedBossName.empty())) return false;
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

void writeStatusEffects(std::ostream& out, const std::vector<StatusEffectInstance>& effects) {
    out << effects.size() << '\n';
    for (const StatusEffectInstance& e : effects) {
        out << static_cast<int>(e.type) << ' ' << e.turnsRemaining << ' ' << e.magnitude << '\n';
    }
}

bool readStatusEffects(std::istream& in, std::vector<StatusEffectInstance>& effects) {
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
        if (type<0 || type>static_cast<int>(StatusEffectType::Doom) || turnsRemaining<1 || turnsRemaining>10000 || magnitude<0 || magnitude>10000) return false;
        if (type==static_cast<int>(StatusEffectType::Marked) && magnitude!=1) return false;
        effects.push_back(
            StatusEffectInstance{static_cast<StatusEffectType>(type), turnsRemaining, magnitude});
    }
    return true;
}

} // namespace

// `version` lets tests write genuine older layouts (each field is written
// under the same version threshold the reader uses); the game always writes
// kSaveFormatVersion.
static bool writeSaveState(std::ostream& out, const SaveGameState& state, int depth=0, int version=kSaveFormatVersion) {
    if (state.extraLives<0 || state.extraLives>2 || (!state.adventureMode && state.extraLives!=0)) return false;
    if (!validDungeonLevels(state.dungeonLevels) || !validItems(state) || !validProgression(state) || state.savedFloors.size()>=kRunFinalFloor ||
        (depth>0 && !state.savedFloors.empty())) return false;

    out << "ROGUELIKE_SAVE " << version << '\n';

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
    // Inside a trial, currentFloor only sets the arena's difficulty; the
    // dungeon floor waiting for the player's return is among savedFloors.
    if (version>=26) out << state.trial << ' ' << state.trialReturnFloor << '\n';
    out << state.playerStats.hp << ' ' << state.playerStats.maxHp << ' '
        << state.playerStats.mana << ' ' << state.playerStats.maxMana << ' '
        << state.playerStats.strength << ' ' << state.playerStats.dexterity << ' '
        << state.playerStats.intelligence << ' ' << state.playerStats.speed << '\n';
    out << state.lastMoveDirection.x << ' ' << state.lastMoveDirection.y << '\n';

    writeTalentStates(out, state.playerTalents);


    writeStatusEffects(out, state.playerStatusEffects);

    out << state.monsters.size() << '\n';
    for (const SaveGameState::MonsterSaveData& m : state.monsters) {
        out << static_cast<int>(m.type) << ' ' << m.position.x << ' ' << m.position.y << ' '
            << m.hp << ' ' << m.maxHp << ' ' << (m.isBoss ? 1 : 0) << ' '
            << static_cast<int>(m.tier) << ' ' << m.rewardsEligible << '\n';
        writeStatusEffects(out, m.statusEffects);
        writeTalentStates(out, m.talents);
        if (version>=12) out << m.vaultGuard << '\n';
        if (version>=25) out << m.eventChampion << '\n';
        if (version>=13) out << m.recoveryActions << ' ' << m.summonsCommitted << ' ' << m.announcedPhase << ' ' << m.enraged << '\n';
        if (version>=11) {
            out << m.intent.has_value();
            if (m.intent) {
                const auto& intent=*m.intent;
                out << ' ' << intent.origin.x << ' ' << intent.origin.y << ' ' << intent.target.x << ' ' << intent.target.y
                    << ' ' << intent.radius << ' ' << intent.playerActionsRemaining << ' ' << intent.attackPower;
                if (version>=13) out << ' ' << static_cast<int>(intent.kind);
            }
            out << '\n';
        }
        if (version>=15) out << m.allied << ' ' << m.summonRank << ' ' << m.summonIntelligence << ' ' << m.remainingLife << '\n';
        const auto& t=m.tactics;
        if (version>=22)
            out << t.home.x << ' ' << t.home.y << ' ' << t.lastKnown.x << ' ' << t.lastKnown.y << ' '
                << t.alert << ' ' << t.patrol << ' ' << t.retreat << ' ' << t.heals << ' ' << t.retreated << ' ' << t.concealed << '\n';
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
    // Older layouts predate format 27's larger ability point budget.
    const int abilityPoints=version<27 ? state.abilityPoints-(earnedAbilityPoints(state.playerLevel)-(state.playerLevel+2)) : state.abilityPoints;
    out << state.treePoints << ' ' << abilityPoints << ' ' << state.trees.size() << '\n';
    for (const auto& tree:state.trees) out << tree.id << ' ' << tree.specialized << '\n';
    out << state.hotbar.size() << '\n';
    for (const auto& id:state.hotbar) out << (id.empty()?"-":id) << '\n';
    out << state.progressionReviewPending << ' ' << state.pendingFinalVictory << ' ' << std::quoted(state.defeatedBossName) << '\n';
    if (version>=12)
        out << state.vaultExists << ' ' << state.vaultOpened << ' ' << state.vaultClaimed << ' '
            << state.vaultCenter.x << ' ' << state.vaultCenter.y << ' ' << state.vaultEntrance.x << ' ' << state.vaultEntrance.y << '\n';
    if (version>=14) {
        out << state.floorEntrance.x << ' ' << state.floorEntrance.y << ' ' << state.floorExit.x << ' ' << state.floorExit.y
            << ' ' << state.inTown << ' ' << state.gold << ' ' << state.quietTurns << ' ' << state.savedFloors.size() << '\n';
        for (const auto& floor:state.savedFloors) if (!writeSaveState(out,floor,depth+1,version)) return false;
    }
    if (version>=15) {
        out << state.bloodRelic << ' ' << state.animationRelic << ' ' << state.deathlessSpentFloors.size();
        for (int floor:state.deathlessSpentFloors) out << ' ' << floor;
        out << '\n';
    }
    if (version>=19) out << state.dungeonLevels[0] << ' ' << state.dungeonLevels[1] << '\n';
    if (version>=21) out << state.adventureMode << ' ' << state.extraLives << '\n';
    if (version>=23) out << state.landmark << ' ' << state.landmarkAltar.x << ' ' << state.landmarkAltar.y << ' ' << state.landmarkUsed << '\n';
    if (version>=24) {
        out << state.props.size();
        for (const auto& prop : state.props) out << ' ' << static_cast<int>(prop.kind) << ' ' << prop.pos.x << ' ' << prop.pos.y;
        out << '\n';
    }
    if (version>=26)
        out << (state.ascendancy.empty() ? std::string("-") : state.ascendancy) << ' ' << state.ascendancyPoints << ' '
            << state.trialKeys << ' ' << state.trialsCleared << '\n';
    if (version>=28) out << state.lightSource << ' ' << state.lightLit << '\n';
    if (version>=29) {
        out << state.surfaces.size();
        for (const auto& [x,y,type,turns] : state.surfaces) out << ' ' << x << ' ' << y << ' ' << type << ' ' << turns;
        out << ' ' << state.torchToggles.size();
        for (const auto& t : state.torchToggles) out << ' ' << t.x << ' ' << t.y;
        out << '\n';
    }
    if (version>=30) {
        out << state.lightOrbs.size();
        for (const auto& [x,y,turns] : state.lightOrbs) out << ' ' << x << ' ' << y << ' ' << turns;
        out << '\n';
    }
    return static_cast<bool>(out);
}

static std::optional<SaveGameState> readSaveState(std::istream& in, int depth=0) {

    std::string tag;
    int version = 0;
    if (!(in >> tag >> version) || tag != "ROGUELIKE_SAVE" || (version != kSaveFormatVersion && version != 30 && version != 29 && version != 28 && version != 27 && version != 26 && version != 25 && version != 24 && version != 23 && version != 22 && version != 21 && version != 20 && version != 19 && version != 18 && version != 17 && version != 16 && version != 15 && version != 14 && version != 13 && version != 12 && version != 11 && version != 10 && version != 9)) {
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
    if (!(in >> state.playerLevel >> state.playerXp) || state.playerLevel < 1 || state.playerLevel > kRunMaxLevel || state.playerXp < 0) {
        return std::nullopt;
    }
    if (!(in >> state.currentFloor) || state.currentFloor < 1 || state.currentFloor > kRunFinalFloor ||
        !state.map.isWalkable(state.playerPosition.x, state.playerPosition.y)) {
        return std::nullopt;
    }
    if (version>=26 && (!(in>>state.trial>>state.trialReturnFloor) || state.trial<0 || state.trial>kTrialCount ||
        (state.trial && (state.trialReturnFloor<1 || state.trialReturnFloor>kRunFinalFloor)) || (!state.trial && state.trialReturnFloor)))
        return std::nullopt;
    if (!(in >> state.playerStats.hp >> state.playerStats.maxHp >> state.playerStats.mana >>
          state.playerStats.maxMana >> state.playerStats.strength >>
          state.playerStats.dexterity >> state.playerStats.intelligence >>
          state.playerStats.speed)) {
        return std::nullopt;
    }
    if (!(in >> state.lastMoveDirection.x >> state.lastMoveDirection.y)) {
        return std::nullopt;
    }

    if (!readTalentStates(in, state.playerTalents))
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
            tier < 0 || tier > 2 || type < 0 || type > static_cast<int>(version>=31?MonsterType::DrownedOne:version>=22?MonsterType::FrostAcolyte:version>=16?MonsterType::OssuaryWarden:MonsterType::Skeleton) ||
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
        if (version<17 && m.type==MonsterType::Lich &&
            std::none_of(m.talents.begin(),m.talents.end(),[](const auto& t){return t.id=="lich.hex";}))
            m.talents.push_back({"lich.hex",10,1}); // grace period for an already-running fight
        for (const auto& t:m.talents) if (t.rank!=1) return std::nullopt;
        if (version>=12 && !(in>>m.vaultGuard)) return std::nullopt;
        if (version>=25 && (!(in>>m.eventChampion) || m.eventChampion<0 || m.eventChampion>kEventChampionKinds)) return std::nullopt;
        if (version>=13) {
            if (!(in>>m.recoveryActions>>m.summonsCommitted>>m.announcedPhase>>m.enraged) ||
                m.recoveryActions<0 || m.recoveryActions>1 || m.summonsCommitted<0 || m.summonsCommitted>3 ||
                m.announcedPhase<1 || m.announcedPhase>3 ||
                (m.type!=MonsterType::Lich && m.summonsCommitted!=0) ||
                (m.type!=MonsterType::GoblinWarlord && (m.enraged || m.announcedPhase!=1))) return std::nullopt;
        } else {
            // Legacy files cannot reveal already-killed summons. Never replenish
            // a loaded Lich's lifetime budget; new fights use the persisted count.
            if (m.type==MonsterType::Lich) m.summonsCommitted=3;
            if (m.type==MonsterType::GoblinWarlord) {
                m.announcedPhase=m.hp*10<=m.maxHp*3?3:m.hp*10<=m.maxHp*6?2:1;
                m.enraged=m.announcedPhase==3 && std::any_of(m.statusEffects.begin(),m.statusEffects.end(),
                    [](const auto& e){return e.type==StatusEffectType::Empowered;});
            }
        }
        if (version>=11) {
            int hasIntent=0;
            if (!(in >> hasIntent) || hasIntent<0 || hasIntent>1) return std::nullopt;
            if (hasIntent) {
                EnemyIntent intent;
                if (!(in >> intent.origin.x >> intent.origin.y >> intent.target.x >> intent.target.y
                    >> intent.radius >> intent.playerActionsRemaining >> intent.attackPower)) return std::nullopt;
                int kind=m.type==MonsterType::Bomber?1:0;
                if (version>=13 && !(in>>kind)) return std::nullopt;
                if (kind<0 || kind>3) return std::nullopt;
                intent.kind=static_cast<IntentKind>(kind);
                const bool bomber=m.type==MonsterType::Bomber || m.type==MonsterType::OssuaryWarden;
                const bool guard=m.type==MonsterType::SkeletonGuard;
                const bool warlord=m.type==MonsterType::GoblinWarlord;
                const bool lich=m.type==MonsterType::Lich;
                const bool summon=intent.kind==IntentKind::Summon;
                const bool blast=intent.kind==IntentKind::MagicStrike && (bomber || warlord);
                const bool compatible=(intent.kind==IntentKind::StunStrike && m.type==MonsterType::Ogre) ||
                    (intent.kind==IntentKind::MagicStrike && (bomber || warlord || lich)) ||
                    (intent.kind==IntentKind::HeavyStrike && (warlord || guard)) || (summon && lich && m.summonsCommitted>0);
                // Radius-1 legacy blasts remain valid after a version-16 resave.
                const bool heavyWarlord=warlord && intent.kind==IntentKind::HeavyStrike;
                const bool radiusValid=blast ? (intent.radius==1 || (version>=16 && intent.radius==2)) :
                    heavyWarlord ? (intent.radius==0 || (version>=17 && intent.radius==1)) : intent.radius==(guard?1:0);
                const int maxWarning=(blast || heavyWarlord)?intent.radius+1:(summon || guard)?2:1;
                if (!compatible || !radiusValid ||
                    intent.playerActionsRemaining<0 || intent.playerActionsRemaining>maxWarning ||
                    intent.attackPower<(summon?0:1) || intent.attackPower>100000 ||
                    !state.map.isWalkable(intent.origin.x,intent.origin.y) ||
                    !state.map.isWalkable(intent.target.x,intent.target.y)) return std::nullopt;
                m.intent=intent;
            }
        }
        if (version>=15) {
            if (!(in>>m.allied>>m.summonRank>>m.summonIntelligence>>m.remainingLife) || m.summonRank<1 || m.summonRank>3 ||
                m.summonIntelligence<0 || m.summonIntelligence>100000 || m.remainingLife<0 || m.remainingLife>8 ||
                (m.allied && (m.type!=MonsterType::Skeleton || m.isBoss || m.rewardsEligible || m.vaultGuard || m.intent)) ||
                (!m.allied && (m.summonRank!=1 || m.summonIntelligence || m.remainingLife))) return std::nullopt;
        }
        if(version>=22) {
            auto& t=m.tactics;
            if(!(in>>t.home.x>>t.home.y>>t.lastKnown.x>>t.lastKnown.y>>t.alert>>t.patrol>>t.retreat>>t.heals>>t.retreated>>t.concealed) ||
                t.home.x<0 || t.home.y<0 || t.home.x>=state.map.width() || t.home.y>=state.map.height() ||
                t.lastKnown.x<0 || t.lastKnown.y<0 || t.lastKnown.x>=state.map.width() || t.lastKnown.y>=state.map.height() ||
                t.alert<0 || t.alert>8 || t.patrol<0 || t.patrol>=24 || t.retreat<0 || t.retreat>3 || t.heals<0 || t.heals>3 ||
                (t.concealed && !enemyAmbusher(m.type))) return std::nullopt;
        } else { m.tactics.home=m.position; m.tactics.lastKnown=m.position; }
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
    std::size_t treeCount=0,barCount=0;
    if (!(in >> state.treePoints >> state.abilityPoints >> treeCount) || treeCount>kTalentTrees.size()) return std::nullopt;
    // Format 27 raised the ability point budget; older characters get the difference.
    if (version<27) state.abilityPoints+=earnedAbilityPoints(state.playerLevel)-(state.playerLevel+2);
    for (std::size_t i=0;i<treeCount;++i) {
        Player::TreeAccess tree;
        if (!(in>>tree.id>>tree.specialized)) return std::nullopt;
        state.trees.push_back(tree);
    }
    if (!(in>>barCount) || barCount>18) return std::nullopt;
    for (std::size_t i=0;i<barCount;++i) {
        std::string id; if (!(in>>id)) return std::nullopt;
        state.hotbar.push_back(id=="-"?"":id);
    }
    if (!(in>>state.progressionReviewPending>>state.pendingFinalVictory>>std::quoted(state.defeatedBossName)) || state.defeatedBossName.size()>100) return std::nullopt;
    if (version>=12 && !(in>>state.vaultExists>>state.vaultOpened>>state.vaultClaimed
        >>state.vaultCenter.x>>state.vaultCenter.y>>state.vaultEntrance.x>>state.vaultEntrance.y)) return std::nullopt;
    if (version>=14) {
        std::size_t count=0;
        if (!(in>>state.floorEntrance.x>>state.floorEntrance.y>>state.floorExit.x>>state.floorExit.y
            >>state.inTown>>state.gold>>state.quietTurns>>count) || count>=kRunFinalFloor || (depth>0 && count) ||
            state.gold<0 || state.gold>100000000 || state.quietTurns<0 || state.quietTurns>10 ||
            !state.map.isWalkable(state.floorEntrance.x,state.floorEntrance.y) ||
            (!state.trial && state.currentFloor<10 && !state.map.isWalkable(state.floorExit.x,state.floorExit.y))) return std::nullopt;
        std::unordered_set<int> floors;
        if (!state.trial) floors.insert(state.currentFloor);
        for (std::size_t i=0;i<count;++i) {
            auto floor=readSaveState(in,depth+1);
            if (!floor || !floors.insert(floor->currentFloor).second) return std::nullopt;
            state.savedFloors.push_back(std::move(*floor));
        }
        std::unordered_set<std::uint64_t> worldIds;
        for (const auto& item:state.items) worldIds.insert(item.instanceId);
        for (const auto& floor:state.savedFloors) for (const auto& item:floor.items) if (item.location<=-2) {
            if (item.instanceId>=state.nextItemId || !worldIds.insert(item.instanceId).second) return std::nullopt;
        }
    } else {
        // Earlier floors were never saved. Do not fabricate/reset them.
        state.floorEntrance=state.playerPosition; state.floorExit={-1,-1};
        for (int y=0;y<state.map.height();++y) for (int x=0;x<state.map.width();++x)
            if (state.map.tileAt(x,y).type==TileType::Door) state.floorExit={x,y};
    }
    if (version>=15) {
        std::size_t spent=0;
        if (!(in>>state.bloodRelic>>state.animationRelic>>spent) || spent>kRunFinalFloor) return std::nullopt;
        for (std::size_t i=0;i<spent;++i) { int floor; if (!(in>>floor)) return std::nullopt; state.deathlessSpentFloors.push_back(floor); }
    } else {
        state.bloodRelic=state.currentFloor>5 || (state.currentFloor==5 && std::none_of(state.monsters.begin(),state.monsters.end(),[](const auto& m){return m.type==MonsterType::GoblinWarlord && m.isBoss;}));
        if (state.currentFloor==10) {
            const auto boss=std::find_if(state.monsters.begin(),state.monsters.end(),[](const auto& m){return m.isBoss && m.type==MonsterType::Lich;});
            state.animationRelic=boss==state.monsters.end();
            state.floorExit=boss!=state.monsters.end()?boss->position:state.playerPosition;
            state.map.setTile(state.floorExit.x,state.floorExit.y,Tile{TileType::Door,true,true});
            state.pendingFinalVictory=false;
        }
    }
    // Version 9 uses the same layout; add the universal action without changing point budgets.
    if (version==9 && std::none_of(state.playerTalents.begin(),state.playerTalents.end(),[](const auto& t){return t.id=="basic.cleanse";})) {
        state.playerTalents.push_back({"basic.cleanse",0,1});
        auto free=std::find(state.hotbar.begin(),state.hotbar.end(),std::string{});
        if (free!=state.hotbar.end()) *free="basic.cleanse";
        else if (state.hotbar.size()<18) state.hotbar.push_back("basic.cleanse");
    }
    if(version>=19) {
        if(!(in>>state.dungeonLevels[0]>>state.dungeonLevels[1]) || !validDungeonLevels(state.dungeonLevels)) return std::nullopt;
    }
    if(version<20) {
        // Undo the old integer HP multiplier, then apply depth scaling once.
        // Nested floor snapshots pass through this reader independently.
        const int index=dungeonIndex(state.currentFloor);
        const int oldBonus=version==19?std::max(0,state.dungeonLevels[index]-dungeonMinimum(index)):0;
        const int oldFactor=100+8*oldBonus;
        const int newFactor=100+8*dungeonDepthBonus(state.currentFloor);
        for(auto& monster:state.monsters) {
            if(monster.allied) continue;
            const auto base=(static_cast<std::int64_t>(monster.maxHp)*100+oldFactor-1)/oldFactor;
            const int newMax=static_cast<int>(base*newFactor/100);
            monster.hp=std::max(1,static_cast<int>(static_cast<std::int64_t>(monster.hp)*newMax/monster.maxHp));
            monster.maxHp=newMax;
        }
    }
    state.dungeonLevels={};
    if(version>=21 && (!(in>>state.adventureMode>>state.extraLives) || state.extraLives<0 || state.extraLives>2 || (!state.adventureMode && state.extraLives!=0))) return std::nullopt;
    if (version>=23 && !(in>>state.landmark>>state.landmarkAltar.x>>state.landmarkAltar.y>>state.landmarkUsed)) return std::nullopt;
    if (state.landmark<0 || state.landmark>=kLandmarkKindCount) return std::nullopt;
    if (version>=24) {
        std::size_t count=0;
        if (!(in>>count) || count>500) return std::nullopt;
        for (std::size_t i=0;i<count;++i) {
            int kind=0; Position pos;
            if (!(in>>kind>>pos.x>>pos.y) || kind<kPropKindMin || kind>kPropKindMax) return std::nullopt;
            const Prop prop{static_cast<PropKind>(kind),pos};
            for (int t=0;t<propWidth(prop.kind);++t) {
                if (!state.map.inBounds(pos.x+t,pos.y) || state.map.isWalkable(pos.x+t,pos.y)) return std::nullopt;
                state.map.setTile(pos.x+t,pos.y,Tile{TileType::Wall,false,true}); // props don't block sight
            }
            state.props.push_back(prop);
        }
    }
    if (version>=26) {
        if (!(in>>state.ascendancy>>state.ascendancyPoints>>state.trialKeys>>state.trialsCleared)) return std::nullopt;
        if (state.ascendancy=="-") state.ascendancy.clear();
    }
    if (version>=28 && (!(in>>state.lightSource>>state.lightLit) || state.lightSource<0 || state.lightSource>2)) return std::nullopt;
    if (version>=29) {
        std::size_t count=0;
        if (!(in>>count) || count>static_cast<std::size_t>(state.map.width()*state.map.height())) return std::nullopt;
        for (std::size_t i=0;i<count;++i) {
            int x=0,y=0,type=0,turns=0;
            if (!(in>>x>>y>>type>>turns) || !state.map.inBounds(x,y) || !state.map.isWalkable(x,y) ||
                type<1 || type>(version>=30 ? kSurfaceTypeMax : 5) || turns<0 || turns>50) return std::nullopt;
            state.surfaces.push_back({x,y,type,turns});
        }
        if (!(in>>count) || count>1000) return std::nullopt;
        for (std::size_t i=0;i<count;++i) {
            Position p;
            if (!(in>>p.x>>p.y) || !state.map.inBounds(p.x,p.y)) return std::nullopt;
            state.torchToggles.push_back(p);
        }
    }
    if (version>=30) {
        std::size_t count=0;
        if (!(in>>count) || count>4) return std::nullopt;
        for (std::size_t i=0;i<count;++i) {
            int x=0,y=0,turns=0;
            if (!(in>>x>>y>>turns) || !state.map.inBounds(x,y) || turns<1 || turns>100) return std::nullopt;
            state.lightOrbs.push_back({x,y,turns});
        }
    }
    if (state.landmark && (!state.map.inBounds(state.landmarkAltar.x,state.landmarkAltar.y) ||
        state.map.isWalkable(state.landmarkAltar.x,state.landmarkAltar.y))) return std::nullopt;
    if (version>=15 && !state.trial && state.currentFloor<kRunFinalFloor && !state.map.isWalkable(state.floorExit.x,state.floorExit.y)) return std::nullopt;
    if (!validProgression(state)) return std::nullopt;
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
    // Ascendancy passives that raise maximum life or mana by a percentage
    // (Iron Skin, Devotion, Wellspring), applied the way the player does.
    int lifePercent = 0, manaPercent = 0;
    for (const auto& t : state.playerTalents)
        if (const auto* d = findTalentDefinition(t.id)) {
            const auto kind = d->ranks[0].passiveKind;
            if (kind == PassiveKind::IronSkin || kind == PassiveKind::Wellspring) lifePercent += d->ranks[0].passiveMagnitude;
            if (kind == PassiveKind::Devotion || kind == PassiveKind::Wellspring) manaPercent += d->ranks[0].passiveMagnitude;
        }
    maxHp += maxHp * lifePercent / 100;
    maxMana += maxMana * manaPercent / 100;
    if (stats.hp < 0 || stats.hp > maxHp || stats.mana < 0 || stats.mana > maxMana)
        return std::nullopt;
    return state;
}

bool saveGame(const SaveGameState& state, const std::string& path) {
    return saveGameAsVersion(state, path, kSaveFormatVersion);
}

bool saveGameAsVersion(const SaveGameState& state, const std::string& path, int version) {
    std::ostringstream payload;
    if (!writeSaveState(payload,state,0,version)) return false;
    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << payload.str();
    return static_cast<bool>(out);
}

std::optional<SaveGameState> loadGame(const std::string& path) {
    std::ifstream in(path);
    return in.is_open()?readSaveState(in):std::nullopt;
}

} // namespace engine
