#include "core/Application.hpp"
#include "entities/MonsterFactory.hpp"
#include "entities/PlayerLeveling.hpp"
#include "entities/TalentProgression.hpp"

#include <climits>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <random>

namespace engine {

// The Encounter Lab: one fixed run of three rooms, replayable by seed, with
// the same level-6 budget spent three ways. Every attempt writes a plain
// log of turns, actions, enemy decisions, damage and resources, so two
// attempts can be read side by side.
namespace {
constexpr int kLabWidth = 52, kLabHeight = 17, kLabLevel = 6, kLabDepth = 6;

struct LabPlan { const char* main; const char* partner; const char* armour; const char* extra; const char* resonance; };
LabPlan labPlan(PlayerClass cls) {
    if (cls == PlayerClass::Mage) return {"fire", "lightning", "cloth", "acrobatics", "resonance.ionise"};
    if (cls == PlayerClass::Thief) return {"bow", "lightning", "light_armour", "stealth", "resonance.storm_bolts"};
    return {"one_handed", "fire", "heavy_armour", "acrobatics", "resonance.searing_edge"};
}
const char* className(PlayerClass cls) { return cls == PlayerClass::Mage ? "Mage" : cls == PlayerClass::Thief ? "Thief" : "Warrior"; }
const char* decisionName(AIActionType type) {
    switch (type) {
        case AIActionType::Move: return "moves";
        case AIActionType::Attack: return "attacks";
        case AIActionType::UseAbility: return "uses an ability";
        case AIActionType::SelfBuff: return "steels itself";
        case AIActionType::Summon: return "summons";
        case AIActionType::Wait: return "waits";
    }
    return "?";
}
} // namespace

const char* labBuildName(int build) { return build == 1 ? "Hybrid" : build == 2 ? "Broad" : "Specialist"; }

int Application::labRoomAt(Position p) const { return p.x < 17 ? 1 : p.x < 35 ? 2 : 3; }

void Application::labNote(const std::string& line) {
    if (labRun_) labLog_.push_back("T" + std::to_string(labTurn_) + "  " + line);
}

// Spends the level-6 budget the way the chosen build does, through the same
// purchase rules as the talent screen.
void Application::buildLabCharacter() {
    const auto plan = labPlan(playerClass_);
    const auto treeIndex = [](const std::string& id) {
        for (std::size_t i = 0; i < kTalentTrees.size(); ++i) if (id == kTalentTrees[i].id) return i;
        return std::size_t{0};
    };
    const auto open = [&](const char* id) { treeSelection_ = treeIndex(id); resonanceSelection_.reset(); handleTreeKey(sf::Keyboard::Key::Enter, false); };
    const auto buyOne = [&](const char* tree, bool newOnly) {
        for (const auto* d : treeNodes(tree))
            if ((!newOnly || !player_.talents().rankOf(d->id)) && abilityPurchaseReason(player_, *d).empty()) { purchaseAbility(player_, *d); return true; }
        return false;
    };
    const auto deep = [&](const char* tree) { while (buyOne(tree, false)) {} };

    open(plan.main);
    if (labBuild_ >= 1) open(plan.partner);
    open(plan.armour);
    if (labBuild_ == 2) open(plan.extra);
    if (labBuild_ == 0) { deep(plan.main); deep(plan.armour); }
    if (labBuild_ == 1) {
        // Both colours to the threshold, wake the resonance, then the rest into the main tree.
        const auto* r = findTalentDefinition(plan.resonance);
        const auto buyColour = [&](const char* tree, Affinity colour) {
            for (const auto* d : treeNodes(tree))
                if (d->affinity == colour && abilityPurchaseReason(player_, *d).empty()) { purchaseAbility(player_, *d); return true; }
            return false;
        };
        for (bool any = true; r && any && !resonanceAwake(player_, *r);) {
            any = false;
            for (const Affinity colour : {r->resonance[0], r->resonance[1]})
                if (affinityPoints(player_, colour) < kResonancePoints)
                    any = buyColour(plan.main, colour) || buyColour(plan.partner, colour) || any;
        }
        if (r && abilityPurchaseReason(player_, *r).empty()) purchaseAbility(player_, *r);
        deep(plan.main); deep(plan.partner); deep(plan.armour);
    }
    if (labBuild_ == 2) {
        // A little of everything: every node you can reach at rank 1 first.
        for (bool any = true; any;) {
            any = false;
            for (const char* tree : {plan.main, plan.partner, plan.armour, plan.extra}) any = buyOne(tree, true) || any;
        }
        deep(plan.main); deep(plan.partner); deep(plan.armour); deep(plan.extra);
    }
}

// Three rooms, west to east. 1: an Ogre that telegraphs its stunning slam,
// with the doorway behind you to fall back through. 2: slingers behind
// pillars, an oil slick and a brazier to kick into it. 3: a goblin squad
// holding a chokepoint, a bomber's warned blast, and a pillar to circle.
void Application::buildLabMap() {
    std::mt19937 rng(labSeed_ * 2654435761u + 17u);
    const auto roll = [&](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
    Map lab(kLabWidth, kLabHeight);
    const auto floorAt = [&](int x, int y) { lab.setTile(x, y, Tile{TileType::Floor, true, true}); };
    const auto wallAt = [&](int x, int y) { lab.setTile(x, y, Tile{TileType::Wall, false, false}); };
    for (int y = 0; y < kLabHeight; ++y) for (int x = 0; x < kLabWidth; ++x) wallAt(x, y);
    const auto room = [&](int x0, int x1) { for (int y = 2; y <= 14; ++y) for (int x = x0; x <= x1; ++x) floorAt(x, y); };
    room(2, 15); room(19, 33); room(37, 49);
    for (int x = 16; x <= 18; ++x) floorAt(x, 8);
    for (int x = 34; x <= 36; ++x) floorAt(x, 8);
    // Room 2: four pillars of cover; two hide archers.
    const Position pillars[]{{25, 5}, {25, 11}, {29, 4}, {29, 12}};
    for (const auto p : pillars) { wallAt(p.x, p.y); wallAt(p.x, p.y + 1); }
    // Room 3: a block to circle round, and a side passage from the doorway
    // into its far corners -- a way round the squad, or a way out.
    const int blockY = roll(6, 8);
    for (int y = blockY; y <= blockY + 2; ++y) for (int x = 42; x <= 44; ++x) wallAt(x, y);
    for (int y = 2; y <= 14; ++y) if (y != 3 && y != 8 && y != 13) wallAt(37, y);
    for (int y = 3; y <= 13; ++y) floorAt(36, y);
    map_ = lab;
    clearSurfaces();
    setProps({});
    // Room 2's oil slick, somewhere between the doorway and the pillars.
    const Position oil{roll(21, 23), roll(6, 10)};
    for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 2; ++dx)
        if (std::abs(dx) + std::abs(dy) <= 2 && map_.isWalkable(oil.x + dx, oil.y + dy)) setSurface({oil.x + dx, oil.y + dy}, SurfaceType::Oil, 0);
    placeBraziers({8, 8}, {{0, -4}, {0, 4}});
    placeBraziers(oil, {{0, roll(0, 1) ? -3 : 3}});
    placeBraziers({46, 8}, {{0, -4}, {0, 4}});

    const auto spawn = [&](MonsterType type, Position at) {
        auto m = createMonster(type, at);
        scaleDungeonMonster(*m, kLabDepth);
        m->lastObservedHp = m->stats().hp;
        m->tactics.home = at; m->tactics.lastKnown = at;
        monsters_.push_back(std::move(m));
    };
    monsters_.clear(); boss_ = nullptr;
    spawn(MonsterType::Ogre, {roll(11, 13), roll(6, 10)});
    spawn(MonsterType::Goblin, {roll(12, 14), roll(0, 1) ? roll(3, 5) : roll(11, 13)}); // clear of the Ogre
    const int hidden = roll(0, 1);
    spawn(MonsterType::GoblinSlinger, {pillars[hidden].x + 1, pillars[hidden].y});
    const auto far = pillars[2 + roll(0, 1)];
    spawn(MonsterType::GoblinSlinger, {far.x + 1, far.y});
    spawn(MonsterType::GoblinBulwark, {39, 8});
    spawn(MonsterType::GoblinSlinger, {47, roll(3, 5)});
    spawn(MonsterType::GoblinMedic, {48, roll(11, 13)});
    spawn(MonsterType::Bomber, {48, 8});

    map_.setTile(49, 8, Tile{TileType::Door, true, true});
    floorExit_ = {49, 8};
    floorEntrance_ = {3, 8};
    player_.setPosition(floorEntrance_);
}

void Application::startLab(PlayerClass cls) {
    selectClass(cls);
    labRun_ = true; labTurn_ = 0; labRoom_ = 1; labLog_.clear();
    extraLives_ = 0;
    // Level 6, every attribute point in the class's own stat.
    int xp = 0;
    for (int level = 1; level < kLabLevel; ++level) xp += xpForNextLevel(level);
    grantXp(player_, xp);
    for (; player_.unspentAttributePoints() > 0; --player_.unspentAttributePoints()) {
        auto& base = player_.baseStats();
        if (cls == PlayerClass::Warrior) { ++base.strength; ++base.maxHp; }
        else if (cls == PlayerClass::Thief) ++base.dexterity;
        else { ++base.intelligence; ++base.maxMana; }
    }
    player_.refreshEquipmentStats();
    buildLabCharacter();
    progressionReviewPending_ = false;

    currentFloor_ = kLabDepth;
    autoExploring_ = false; exploreSeenInterests_.clear(); cancelTargeting();
    inventoryOpen_ = false; merchantOpen_ = false; dungeonMenu_ = false; exitMenu_ = false;
    vaultExists_ = vaultOpened_ = vaultClaimed_ = false; vaultRewards_.clear();
    landmark_ = LandmarkKind::None; landmarkUsed_ = false; extraLandmarks_.clear(); decals_.clear();
    chestExists_ = chestClaimed_ = chestMimic_ = false;
    groundItems_.clear(); lightOrbs_.clear(); traps_.clear(); storms_.clear(); afterimages_.clear();
    actorAnims_.clear(); corpses_.clear(); previousCameraX_ = previousCameraY_ = INT_MIN; vfx_.clear(); hitFlash_.clear();
    floorNotice_.clear();
    buildLabMap();
    player_.stats().hp = player_.stats().maxHp; player_.stats().mana = player_.stats().maxMana;
    exploredMap_ = ExploredMap(map_);
    scheduler_ = TurnScheduler{};
    scheduler_.add(player_);
    for (auto& m : monsters_) scheduler_.add(*m);
    currentActor_ = &scheduler_.nextTurn();
    resetHarms();
    mode_ = GameMode::Playing;
    updateFieldOfView();

    // The log's header: who walked in, and with what.
    labLog_.push_back(std::string("Encounter Lab -- ") + className(cls) + ", " + labBuildName(labBuild_) + " build, seed " + std::to_string(labSeed_));
    const auto& s = player_.stats();
    labLog_.push_back("Level " + std::to_string(player_.level()) + "  Life " + std::to_string(s.maxHp) + "  Mana " + std::to_string(s.maxMana) +
                      "  Str " + std::to_string(s.strength) + "  Dex " + std::to_string(s.dexterity) + "  Int " + std::to_string(s.intelligence));
    std::string trees = "Trees:";
    for (const auto& t : player_.trees()) trees += std::string(" ") + t.id;
    labLog_.push_back(trees);
    for (std::size_t i = 0; i < player_.talents().knownTalents().size(); ++i) {
        const auto& t = player_.talents().knownTalents()[i];
        if (t.id.rfind("basic.", 0) == 0) continue;
        labLog_.push_back("  " + t.name + " rank " + std::to_string(player_.talents().rank(i)));
    }
    labLog_.push_back("");
    log("Encounter Lab: ", labBuildName(labBuild_), " build, seed ", labSeed_, ". Three rooms; the stairs at the far end finish it.");
}

// Each player turn: where you stand and what you have left.
void Application::labTurnBegins() {
    if (!labRun_) return;
    ++labTurn_;
    const auto p = player_.position();
    labRoom_ = std::max(labRoom_, labRoomAt(p));
    labNote("you  life " + std::to_string(player_.stats().hp) + "/" + std::to_string(player_.stats().maxHp) +
            "  mana " + std::to_string(player_.stats().mana) + "/" + std::to_string(player_.stats().maxMana) +
            "  at " + std::to_string(p.x) + "," + std::to_string(p.y) + "  room " + std::to_string(labRoomAt(p)));
}

void Application::labEnemyDecision(const Monster& monster, const AIDecision& decision) {
    if (!labRun_) return;
    std::string line = monster.name() + " " + decisionName(decision.type);
    if (decision.type == AIActionType::Move) line += " to " + std::to_string(decision.movePosition.x) + "," + std::to_string(decision.movePosition.y);
    if (decision.target) line += decision.target == &player_ ? " you" : " " + decision.target->name();
    if (monster.intent()) line += " (winding up)";
    labNote(line);
}

void Application::finishLab(const char* outcome) {
    if (!labRun_) return;
    labRun_ = false;
    labLog_.push_back("");
    labLog_.push_back(std::string("Outcome: ") + outcome + " after " + std::to_string(labTurn_) + " turns, reaching room " + std::to_string(labRoom_) +
                      ", life " + std::to_string(std::max(0, player_.stats().hp)) + "/" + std::to_string(player_.stats().maxHp));
    if (!harms_.empty()) {
        labLog_.push_back("The blows that mattered:");
        for (const auto& h : harms_)
            labLog_.push_back("  " + h.source + " -" + std::to_string(h.amount) + "  (life " + std::to_string(h.hp) + "/" + std::to_string(h.maxHp) + ")" +
                              (h.state.empty() ? "" : "  " + h.state));
    }
    std::error_code error;
    std::filesystem::create_directories("encounter-lab", error);
    const std::string path = "encounter-lab/" + std::string(className(playerClass_)) + "-" + labBuildName(labBuild_) + "-seed" +
                             std::to_string(labSeed_) + "-" + std::to_string(static_cast<long long>(std::time(nullptr))) + ".txt";
    std::ofstream out(path);
    for (const auto& line : labLog_) out << line << '\n';
    log(out ? "The attempt is written to " + path + "." : std::string("Couldn't write the lab log."));
}

} // namespace engine
