// A bot that plays whole runs through the real game, with a hidden window.
// Not part of the test suite: run it on demand for a playtest report.
//   playtest_bot [runs per class]   (default 4)
// Writes build/playtest/report.txt and screenshots beside it.
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <sstream>

#include "core/Application.hpp"
#include "entities/RunProgression.hpp"
#include "entities/TalentProgression.hpp"
#include "world/TalentTargeting.hpp"
#include "world/FloorTheme.hpp"
#include "core/SaveGame.hpp"

namespace engine {
struct PlaytestBot {
    Application& app;
    std::filesystem::path out;
    std::ostringstream report;
    std::map<std::string, int> issues;     // problem -> times seen
    std::map<std::string, int> events;     // notable log lines -> count
    int shots = 0;

    explicit PlaytestBot(Application& a, std::filesystem::path o) : app(a), out(std::move(o)) {}

    // ------------------------------------------------------------ helpers
    static bool same(Position a, Position b) { return a.x == b.x && a.y == b.y; }
    void issue(const std::string& what) {
        if (issues[what]++ == 0) report << "  ! " << what << '\n';
    }
    void snapshot(const std::string& name) {
        if (shots >= 60) return;
        app.render(); app.render();
        sf::Texture texture(app.window_.getSize()); texture.update(app.window_);
        if (texture.copyToImage().saveToFile((out / (name + ".png")).string())) ++shots;
    }
    // Chasing: how long the bot has pursued each foe without hurting it, and
    // foes it has given up on for a while (a kiting archer, say).
    std::map<const Monster*, int> chase, ignoredUntil;
    int clock = 0;
    std::vector<Monster*> visibleEnemies() {
        std::vector<Monster*> found;
        for (auto& m : app.monsters_) {
            const auto p = m->position();
            if (ignoredUntil.count(m.get()) && ignoredUntil[m.get()] > clock) continue;
            if (!m->allied && m->stats().hp > 0 && !m->tactics.concealed && app.exploredMap_.at(p.x, p.y) == Visibility::Visible) found.push_back(m.get());
        }
        const auto me = app.player_.position();
        std::sort(found.begin(), found.end(), [&](Monster* a, Monster* b) {
            const auto da = std::abs(a->position().x - me.x) + std::abs(a->position().y - me.y);
            const auto db = std::abs(b->position().x - me.x) + std::abs(b->position().y - me.y);
            return da < db;
        });
        return found;
    }
    // First orthogonal step towards `goal` over known, walkable, unoccupied
    // ground, avoiding hazardous surfaces where possible. `adjacentOk` stops
    // beside the goal (for an occupied target).
    std::optional<Position> stepToward(Position goal, bool adjacentOk, bool throughUnknown = false) {
        const auto me = app.player_.position();
        const int w = app.map_.width(), h = app.map_.height();
        std::vector<int> seen(static_cast<std::size_t>(w * h), 0);
        struct Node { Position at, first; };
        std::queue<Node> open; open.push({me, me});
        seen[static_cast<std::size_t>(me.y * w + me.x)] = 1;
        while (!open.empty()) {
            const auto n = open.front(); open.pop();
            for (const Position d : {Position{0, -1}, Position{1, 0}, Position{0, 1}, Position{-1, 0}}) {
                const Position p{n.at.x + d.x, n.at.y + d.y};
                if (!app.map_.inBounds(p.x, p.y)) continue;
                auto& s = seen[static_cast<std::size_t>(p.y * w + p.x)];
                if (s) continue;
                s = 1;
                const Position first = same(n.at, me) ? p : n.first;
                if (same(p, goal) || (adjacentOk && std::abs(p.x - goal.x) + std::abs(p.y - goal.y) == 0)) return first;
                if (!app.map_.isWalkable(p.x, p.y)) continue;
                if (!throughUnknown && app.exploredMap_.at(p.x, p.y) == Visibility::Hidden) continue;
                if (app.hazardousSurface(p)) continue;
                if (auto* a = app.actorAt(p, &app.player_)) { auto* m = dynamic_cast<Monster*>(a); if (!m || !m->allied) continue; }
                if (adjacentOk && std::abs(p.x - goal.x) + std::abs(p.y - goal.y) == 1) return first;
                open.push({p, first});
            }
        }
        return std::nullopt;
    }
    // The nearest unexplored frontier, as auto-explore finds it.
    std::optional<Position> exploreStep() {
        const auto me = app.player_.position();
        const int w = app.map_.width(), h = app.map_.height();
        std::vector<int> seen(static_cast<std::size_t>(w * h), 0);
        struct Node { Position at, first; };
        std::queue<Node> open; open.push({me, me});
        seen[static_cast<std::size_t>(me.y * w + me.x)] = 1;
        while (!open.empty()) {
            const auto n = open.front(); open.pop();
            for (const Position d : {Position{0, -1}, Position{1, 0}, Position{0, 1}, Position{-1, 0}}) {
                const Position p{n.at.x + d.x, n.at.y + d.y};
                if (!app.map_.inBounds(p.x, p.y)) continue;
                auto& s = seen[static_cast<std::size_t>(p.y * w + p.x)];
                if (s) continue;
                s = 1;
                const Position first = same(n.at, me) ? p : n.first;
                if (app.exploredMap_.at(p.x, p.y) == Visibility::Hidden) return first;
                if (app.map_.isWalkable(p.x, p.y) && !app.hazardousSurface(p) && !app.actorAt(p, &app.player_)) open.push({p, first});
            }
        }
        return std::nullopt;
    }
    // A hostile creature that knows where you are, though you can't see it.
    const Monster* nearestAlertFoe() {
        const auto me = app.player_.position();
        const Monster* best = nullptr; int bestDist = 9;
        for (auto& m : app.monsters_) {
            if (m->allied || m->stats().hp <= 0 || m->tactics.alert <= 0) continue;
            if (ignoredUntil.count(m.get()) && ignoredUntil[m.get()] > clock) continue;
            const int d = std::max(std::abs(m->position().x - me.x), std::abs(m->position().y - me.y));
            if (d < bestDist) { bestDist = d; best = m.get(); }
        }
        return best;
    }
    bool move(Position step) {
        const auto me = app.player_.position();
        return app.tryMovePlayer(step.x - me.x, step.y - me.y);
    }
    void wait() {
        app.player_.statusEffects().apply({StatusEffectType::Opening, 2, 0});
        app.finishInventoryTurn();
    }

    // ------------------------------------------------------------ build
    std::vector<const char*> treePlan(PlayerClass cls) {
        if (cls == PlayerClass::Warrior) return {"one_handed", "brawling", "heavy_armour", "shield", "two_handed"};
        if (cls == PlayerClass::Mage) return {"fire", "lightning", "cloth", "arcane", "radiance"};
        return {"bow", "stealth", "light_armour", "acrobatics", "alchemy"};
    }
    std::size_t treeIndex(const std::string& id) {
        for (std::size_t i = 0; i < kTalentTrees.size(); ++i) if (id == kTalentTrees[i].id) return i;
        return 0;
    }
    void spendPoints(PlayerClass cls) {
        const unsigned primary = cls == PlayerClass::Warrior ? 0 : cls == PlayerClass::Thief ? 1 : 2;
        if (app.player_.unspentAttributePoints() > 0) {
            app.mode_ = GameMode::AttributeAllocation;
            while (app.player_.unspentAttributePoints() > 0) app.allocateAttribute(primary);
            app.mode_ = GameMode::Playing;
        }
        if (app.player_.treePoints() <= 0 && app.player_.abilityPoints() <= 0) return;
        app.openTalentTrees();
        const auto plan = treePlan(cls);
        // Trees: specialise the main tree when allowed, else open the next one.
        for (int guard = 0; guard < 10 && app.player_.treePoints() > 0; ++guard) {
            bool bought = false;
            for (const char* id : plan) {
                app.treeSelection_ = treeIndex(id); app.abilitySelection_ = 0;
                if (treePurchaseReason(app.player_, cls, kTalentTrees[app.treeSelection_]).empty()) {
                    app.handleTreeKey(sf::Keyboard::Key::Enter, false); bought = true; break;
                }
            }
            if (!bought) break;
        }
        // Abilities: learn what's new first, then rank up the cheapest.
        for (int guard = 0; guard < 40 && app.player_.abilityPoints() > 0; ++guard) {
            bool bought = false;
            for (int pass = 0; pass < 2 && !bought; ++pass)
                for (const char* id : plan) {
                    if (!treeAccess(app.player_, id)) continue;
                    const auto t = treeIndex(id);
                    for (std::size_t a = 0; a < 4 && !bought; ++a) {
                        const auto& d = talentCatalog()[t * 4 + a];
                        const bool known = app.player_.talents().rankOf(d.id) > 0;
                        if ((pass == 0) == known) continue;
                        if (!abilityPurchaseReason(app.player_, d).empty()) continue;
                        app.treeSelection_ = t; app.abilitySelection_ = a;
                        app.handleTreeKey(sf::Keyboard::Key::A, false); bought = true;
                    }
                    if (bought) break;
                }
            if (!bought) break;
        }
        app.closeTalentTrees();
        if (app.mode_ != GameMode::Playing && app.mode_ != GameMode::GameOver) app.mode_ = GameMode::Playing;
    }
    int itemScore(const Item& item, PlayerClass cls) {
        const auto b = item.bonuses();
        const int primary = cls == PlayerClass::Warrior ? b.strength : cls == PlayerClass::Thief ? b.dexterity : b.intelligence;
        return primary * 3 + b.strength + b.dexterity + b.intelligence + b.maxHp + b.maxMana / 2 + static_cast<int>(item.rarity()) * 2;
    }
    void equipBetter(PlayerClass cls) {
        auto& inv = app.player_.inventory();
        for (std::size_t i = 0; i < inv.items().size(); ++i) {
            const auto& item = *inv.items()[i];
            const auto* d = item.definition();
            if (!d) continue;
            const auto slot = inv.preferredSlot(*d);
            const auto* worn = inv.equipped(slot);
            if (worn && worn->definition()) {
                // Keep the weapon and armour kinds the build relies on.
                if (d->slot == EquipmentSlot::Weapon && d->weaponKind != worn->definition()->weaponKind) continue;
                if (armourSlot(d->slot) && d->armourKind != worn->definition()->armourKind) continue;
                if (d->slot == EquipmentSlot::OffHand && d->weaponKind != worn->definition()->weaponKind) continue;
                if (itemScore(item, cls) <= itemScore(*worn, cls)) continue;
            } else if (d->slot == EquipmentSlot::Weapon || d->slot == EquipmentSlot::OffHand) continue;
            if (app.player_.equip(i)) { ++events["items equipped"]; return; }
        }
    }

    // ------------------------------------------------------------ fighting
    bool fight(std::vector<Monster*> enemies) {
        auto& kit = app.player_.talents();
        const auto& known = kit.knownTalents();
        auto* target = enemies.front();
        const auto me = app.player_.position();
        const auto tp = target->position();
        const int dist = std::abs(tp.x - me.x) + std::abs(tp.y - me.y);
        const auto& stats = app.player_.stats();
        // Pray when hurt and the god will answer.
        if (stats.hp * 2 < stats.maxHp)
            for (std::size_t i = 0; i < known.size(); ++i)
                if (known[i].id == "basic.pray" && talentUnavailableReason(app.player_, i).empty()) { ++events["prayers"]; return app.tryUseTalent(i, me); }
        // Self-buffs and heals when an enemy is close.
        if (dist <= 2)
            for (std::size_t i = 0; i < known.size(); ++i) {
                const auto t = kit.effectiveTalent(i);
                if (t.passive || t.effectKind != TalentEffectKind::SelfBuff || t.cleanse || t.snuffRadius || t.id == "basic.pray" || t.conjureLight) continue;
                if ((t.restoreHpPercent && stats.hp * 2 > stats.maxHp) || (t.restoreMana && stats.mana * 2 > stats.maxMana)) continue;
                if (talentUnavailableReason(app.player_, i).empty()) return app.tryUseTalent(i, me);
            }
        // The strongest damaging ability that reaches it.
        std::optional<std::size_t> best; int bestPower = -1;
        for (std::size_t i = 0; i < known.size(); ++i) {
            const auto t = kit.effectiveTalent(i);
            if (t.passive || t.effectKind != TalentEffectKind::Damage || t.shape == EffectShape::Movement) continue;
            if (!talentUnavailableReason(app.player_, i).empty()) continue;
            const auto aim = t.shape == EffectShape::AreaAroundSelf ? me : tp;
            const auto preview = app.targetPreview(i, aim);
            if (!preview.valid || std::find(preview.affected.begin(), preview.affected.end(), target) == preview.affected.end()) continue;
            const int power = t.power * t.damagePercent + static_cast<int>(preview.affected.size()) * 50;
            if (power > bestPower) { bestPower = power; best = i; }
        }
        if (best) { chase[target] = 0; return app.tryUseTalent(*best, kit.effectiveTalent(*best).shape == EffectShape::AreaAroundSelf ? me : tp); }
        if (dist == 1) { chase[target] = 0; return move(tp); }
        if (++chase[target] > 30) { ignoredUntil[target] = clock + 60; chase[target] = 0; ++events["gave up chasing a fleeing foe"]; }
        if (const auto step = stepToward(tp, true, true)) return move(*step);
        // Nowhere to go: hold still and let it come.
        wait(); return true;
    }

    // ------------------------------------------------------------ landmarks
    int landmarkChoice() {
        const auto choices = app.landmarkChoices();
        if (choices.empty()) return -1;
        if (app.landmark_ == LandmarkKind::Shrine) return app.patron() == Patron::None ? 0 : static_cast<int>(choices.size()) - 1;
        if (app.landmark_ == LandmarkKind::ChainedDemon) return 1;
        for (int i = 0; i < static_cast<int>(choices.size()); ++i) if (choices[static_cast<std::size_t>(i)].affordable) return i;
        return -1;
    }

    // ------------------------------------------------------------ one run
    struct RunResult { PlayerClass cls; int floor = 1, level = 1, turns = 0; bool won = false; std::string end; };

    std::string recentLog(int n) {
        std::string s;
        const int size = static_cast<int>(app.logMessages_.size());
        for (int i = std::max(0, size - n); i < size; ++i) s += "      " + app.logMessages_[static_cast<std::size_t>(i)] + '\n';
        return s;
    }
    void scanLog(std::size_t& seen) {
        static const std::vector<std::pair<const char*, const char*>> keys{
            {"falls into darkness", "monsters dropped into chasms"}, {"fall into the chasm", "player chasm falls"},
            {"knocks you back", "knockbacks taken"}, {"Vampire Lord", "Vampire Lord sightings (log lines)"},
            {"old rite opens", "Blood Magic unlocked"}, {"You swear yourself", "oaths sworn"},
            {"The wrath of", "divine wrath"}, {"kicks a brazier", "Warlord brazier kicks"},
            {"breathes out the light", "Lich darkness"}, {"A unique item falls", "uniques dropped"},
            {"Level up!", "level-ups"}, {"lands in the flames", "creatures pushed into fire"},
            {"slams into the wall", "wall slams"}, {"is pleased", "favor gained (events)"}, {"is displeased", "favor lost (events)"},
            {"Failed", "\"Failed\" messages"}, {"Unknown", "\"Unknown\" messages"}, {"Invalid", "\"Invalid\" messages"}};
        // logMessages_ keeps only the latest lines; logTotal_ counts them all.
        const std::size_t size = app.logMessages_.size();
        const std::size_t fresh = std::min(size, app.logTotal_ - std::min(seen, app.logTotal_));
        for (std::size_t i = size - fresh; i < size; ++i)
            for (const auto& [needle, label] : keys)
                if (app.logMessages_[i].find(needle) != std::string::npos) ++events[label];
        seen = app.logTotal_;
    }
    void checkInvariants() {
        const auto me = app.player_.position();
        const auto& s = app.player_.stats();
        if (!app.map_.isWalkable(me.x, me.y)) issue("player standing on an unwalkable tile");
        if (s.hp > s.maxHp) issue("player life above maximum");
        if (s.mana < 0 || s.mana > s.maxMana) issue("player mana out of range");
        std::set<std::pair<int, int>> tiles{{me.x, me.y}};
        for (auto& m : app.monsters_) {
            if (m->stats().hp <= 0) continue;
            const auto p = m->position();
            if (!app.map_.isWalkable(p.x, p.y)) issue(std::string("monster on an unwalkable tile: ") + m->name());
            if (!tiles.insert({p.x, p.y}).second) issue("two creatures on one tile");
            if (m->stats().hp > m->stats().maxHp) issue("monster life above maximum");
        }
    }
    bool saveRoundTrip() {
        const auto path = (out / "bot-save.txt").string();
        return saveGame(app.captureState(), path) && loadGame(path).has_value();
    }

    // The character at the end of a run: trees, ranks and unspent points.
    std::string buildSummary() {
        std::ostringstream b;
        b << "    trees:";
        for (const auto& t : app.player_.trees()) b << ' ' << t.id << (t.specialized ? "*" : "");
        b << "   (* specialised)\n    abilities:";
        int spent = 0;
        for (std::size_t i = 0; i < app.player_.talents().knownTalents().size(); ++i) {
            const auto& t = app.player_.talents().knownTalents()[i];
            if (t.id.rfind("basic.", 0) == 0) continue;
            const int rank = app.player_.talents().rank(i);
            spent += rank;
            b << ' ' << t.id << '=' << rank;
        }
        b << "\n    ability points spent " << spent << ", unspent " << app.player_.abilityPoints()
          << " (of " << earnedAbilityPoints(app.player_.level()) << " earned); tree points unspent " << app.player_.treePoints()
          << " (of " << earnedTreePoints(app.player_.level()) << ")\n";
        return b.str();
    }
    RunResult playRun(PlayerClass cls, int runNumber) {
        RunResult result; result.cls = cls;
        const char* name = cls == PlayerClass::Warrior ? "warrior" : cls == PlayerClass::Mage ? "mage" : "thief";
        chase.clear(); ignoredUntil.clear();
        app.adventureMode_ = false;
        app.selectClass(cls);
        spendPoints(cls);
        std::size_t logSeen = app.logTotal_;
        std::ostringstream levels; levels << " 1:" << app.player_.level();
        int floor = app.currentFloor_, floorActions = 0, idle = 0;
        Position lastPos = app.player_.position(); int lastHp = app.player_.stats().hp;
        std::map<int, bool> landmarkTried;
        bool shotFloor = false;
        for (int action = 0; action < 30000; ++action) {
            scanLog(logSeen);
            if (app.mode_ == GameMode::GameOver) {
                result.won = app.wonGame_;
                result.end = app.wonGame_ ? "victory" : "died";
                if (!app.wonGame_) snapshot(std::string("death-") + name + "-" + std::to_string(runNumber));
                report << "  " << name << " run " << runNumber << ": " << (app.wonGame_ ? "WON" : "died") << " on floor " << app.currentFloor_
                       << " at level " << app.player_.level() << " after " << action << " actions\n";
                if (!app.wonGame_) report << "    last messages:\n" << recentLog(6);
                report << buildSummary() << "    level on arrival at each floor:" << levels.str() << '\n';
                break;
            }
            if (app.mode_ == GameMode::Town) { app.travelFloor(app.currentFloor_ + 1, true); continue; }
            if (app.mode_ != GameMode::Playing) { spendPoints(cls); if (app.mode_ != GameMode::Playing) app.mode_ = GameMode::Playing; continue; }
            if (app.shrineMenu_) { const int c = landmarkChoice(); if (c >= 0) app.chooseBlessing(c); app.shrineMenu_ = false; continue; }
            if (app.exitMenu_) app.exitMenu_ = false;
            if (app.vaultMenu_) app.vaultMenu_ = 0;
            if (app.inventoryOpen_) app.inventoryOpen_ = false;

            // A new floor: check the save, take a picture of the first few.
            if (app.currentFloor_ != floor) {
                if (!saveRoundTrip()) {
                    issue("save/load failed on floor " + std::to_string(app.currentFloor_));
                    std::filesystem::copy_file(out / "bot-save.txt", out / ("save-fail-" + std::string(name) + "-floor" + std::to_string(app.currentFloor_) + ".txt"),
                                               std::filesystem::copy_options::overwrite_existing);
                }
                report << "    floor " << floor << " took " << floorActions << " actions\n";
                floor = app.currentFloor_; floorActions = 0; shotFloor = false;
                levels << ' ' << floor << ':' << app.player_.level();
            }
            if (!shotFloor && runNumber == 1) { snapshot(std::string(name) + "-floor" + std::to_string(floor)); shotFloor = true; }
            if (++floorActions > 3000) { issue("a floor took over 3000 actions (stuck?)"); snapshot(std::string("stuck-") + name); result.end = "stuck"; break; }
            if (app.pointsToSpend()) spendPoints(cls);
            equipBetter(cls);

            ++clock;
            const auto enemies = visibleEnemies();
            bool acted = false;
            if (!enemies.empty()) acted = fight(enemies);
            else if (const auto* unseen = nearestAlertFoe()) {
                // Shot at from the dark: go toward the arrows.
                if (const auto step = stepToward(unseen->position(), true, true)) acted = move(*step);
            }
            else {
                const auto& s = app.player_.stats();
                const auto me = app.player_.position();
                std::optional<Position> goal;
                bool pick = false, altar = false;
                // Rest when hurt.
                if (s.hp * 10 < s.maxHp * 7) { wait(); acted = true; }
                // Loot underfoot, then loot in sight, then the chest.
                if (!acted && !app.player_.inventory().full()) for (const auto& item : app.groundItems_) {
                    const auto p = item->position();
                    if (app.exploredMap_.at(p.x, p.y) == Visibility::Hidden) continue;
                    if (same(p, me)) { app.pickupItem(); acted = true; break; }
                    if (stepToward(p, false)) { goal = p; pick = true; break; }
                }
                if (!acted && !goal && !app.player_.inventory().full() && app.chestExists_ && !app.chestClaimed_ && app.exploredMap_.at(app.chestPosition_.x, app.chestPosition_.y) != Visibility::Hidden) {
                    if (same(app.chestPosition_, me)) { app.pickupItem(); acted = true; } else if (stepToward(app.chestPosition_, false)) { goal = app.chestPosition_; pick = true; }
                }
                // The landmark, once.
                if (!acted && !goal && app.landmark_ != LandmarkKind::None && !app.landmarkUsed_ && !landmarkTried[app.currentFloor_] &&
                    app.exploredMap_.at(app.landmarkAltar_.x, app.landmarkAltar_.y) != Visibility::Hidden) {
                    if (app.nearAltar()) { landmarkTried[app.currentFloor_] = true; ++events[std::string("landmarks used: ") + landmarkName(app.landmark_, floorTheme(app.currentFloor_).region)];
                        app.openShrine(); acted = true; }
                    else if (stepToward(app.landmarkAltar_, true)) { goal = app.landmarkAltar_; altar = true; }
                    else landmarkTried[app.currentFloor_] = true;
                }
                if (!acted && goal) {
                    if (const auto step = stepToward(*goal, altar)) acted = move(*step);
                    (void)pick;
                }
                if (!acted && !goal) {
                    if (const auto step = exploreStep()) acted = move(*step);
                    else if (app.floorExit_.x >= 0) {
                        if (same(me, app.floorExit_)) {
                            if (app.currentFloor_ >= kRunFinalFloor) { issue("reached the last floor's exit without winning"); result.end = "end"; break; }
                            app.travelFloor(app.currentFloor_ + 1); acted = true;
                        } else if (const auto step = stepToward(app.floorExit_, false)) acted = move(*step);
                        else if (const auto step2 = stepToward(app.floorExit_, false, true)) acted = move(*step2);
                    }
                }
                if (!acted) { wait(); acted = true; }
            }
            checkInvariants();
            const auto now = app.player_.position();
            if (same(now, lastPos) && app.player_.stats().hp == lastHp && enemies.empty()) ++idle; else idle = 0;
            lastPos = now; lastHp = app.player_.stats().hp;
            if (idle > 400) { issue("bot stood still for 400 quiet actions (nothing left it could reach?)"); snapshot(std::string("idle-") + name + "-floor" + std::to_string(floor)); result.end = "idle"; break; }
            result.turns = action;
        }
        result.floor = app.currentFloor_; result.level = app.player_.level();
        if (result.end.empty()) result.end = "action cap";
        if (result.end != "died" && result.end != "victory")
            report << "  " << name << " run " << runNumber << " ended (" << result.end << ") on floor " << app.currentFloor_
                   << " at level " << app.player_.level() << '\n' << buildSummary() << "    level on arrival at each floor:" << levels.str() << '\n';
        return result;
    }

    // Plays every run and writes the report (a member, for the hidden window).
    static int play(int runs) {
        const auto out = std::filesystem::current_path() / "build" / "playtest";
        std::filesystem::create_directories(out);
        Application app;
        app.window_.setVisible(false);
        app.window_.setFramerateLimit(0);
        PlaytestBot bot(app, out);
        std::vector<PlaytestBot::RunResult> results;
        const auto started = std::chrono::steady_clock::now();
        for (auto cls : {PlayerClass::Warrior, PlayerClass::Mage, PlayerClass::Thief}) {
            for (int r = 1; r <= runs; ++r) {
                results.push_back(bot.playRun(cls, r));
                std::cout << "run done: class " << static_cast<int>(cls) << " #" << r << " -> floor " << results.back().floor << " (" << results.back().end << ")" << std::endl;
            }
        }
        std::ostringstream summary;
        summary << "PLAYTEST BOT REPORT\n" << runs << " runs per class, "
                << std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - started).count() << "s\n\n";
        for (auto cls : {PlayerClass::Warrior, PlayerClass::Mage, PlayerClass::Thief}) {
            int total = 0, best = 0, n = 0, levels = 0, wins = 0;
            for (const auto& r : results) if (r.cls == cls) { total += r.floor; best = std::max(best, r.floor); levels += r.level; wins += r.won; ++n; }
            const char* name = cls == PlayerClass::Warrior ? "Warrior" : cls == PlayerClass::Mage ? "Mage" : "Thief";
            summary << name << ": average floor " << (n ? static_cast<double>(total) / n : 0) << ", best " << best
                    << ", average level " << (n ? static_cast<double>(levels) / n : 0) << ", wins " << wins << "/" << n << '\n';
        }
        summary << "\nEvents seen:\n";
        for (const auto& [k, v] : bot.events) summary << "  " << k << ": " << v << '\n';
        summary << "\nProblems (first sighting printed in the run log; count = times seen):\n";
        if (bot.issues.empty()) summary << "  none\n";
        for (const auto& [k, v] : bot.issues) summary << "  " << k << " x" << v << '\n';
        summary << "\nRun log:\n" << bot.report.str();
        std::ofstream(out / "report.txt") << summary.str();
        std::cout << summary.str();
        app.window_.close();
        return 0;
    }
};
} // namespace engine

int main(int argc, char** argv) {
    // playtest_bot --load <file>: just try to load a save (for debugging failures).
    if (argc > 2 && std::string(argv[1]) == "--load") {
        const bool ok = engine::loadGame(argv[2]).has_value();
        std::cout << (ok ? "loads fine" : "load FAILED") << std::endl;
        return ok ? 0 : 1;
    }
    return engine::PlaytestBot::play(argc > 1 ? std::max(1, std::atoi(argv[1])) : 4);
}
