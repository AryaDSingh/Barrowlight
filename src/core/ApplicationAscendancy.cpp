#include "core/Application.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>

#include "core/GameIcons.hpp"
#include "core/ScreenLayout.hpp"
#include "entities/Ascendancy.hpp"
#include "entities/MonsterFactory.hpp"
#include "entities/TalentCatalog.hpp"
#include "world/FloorTheme.hpp"

namespace engine {

// Ascendancy and its trials (entities/Ascendancy.hpp). The Goblin Warlord
// and the Lich drop sigils; each sigil opens a trial at the obelisk in
// town; each trial won grants an ascendancy point, the first also the
// class's ascendancy itself.

namespace {
using namespace screen;
constexpr int kTrialArenaWidth = 31, kTrialArenaHeight = 21;
}

void Application::onBossDefeated(const Monster& boss) {
    if (trialGuardianChampion(boss.eventChampion)) { completeTrial(trial_); return; }
    if (boss.type() == MonsterType::GoblinWarlord && !(player_.trialKeys & 1)) {
        player_.trialKeys |= 1;
        log("The Warlord drops the ", trialSigil(1), "! It opens the ", trialName(1), " at the obelisk in town.");
    } else if (boss.type() == MonsterType::TheSleeper) {
        log("The Sleeper Below sinks into the dark water, and the Cathedral falls silent.");
        grantUnique(boss.position());
        if (patron() == Patron::Sleeper) {
            log("You have slain the god you swore yourself to. Its favor is gone with it.");
            player_.patron = 0; player_.favor = 0;
        }
        log("Its stairs lead back to town.");
    } else if (boss.type() == MonsterType::Lich && !(player_.trialKeys & 2)) {
        player_.trialKeys |= 2;
        log("The Lich drops the ", trialSigil(2), "! It opens the ", trialName(2), " at the obelisk in town.");
    }
}

void Application::completeTrial(int trial) {
    if (trial < 1 || trial > kTrialCount) return;
    const int bit = 1 << (trial - 1);
    if (player_.trialsCleared & bit) return;
    player_.trialsCleared |= bit;
    ++player_.ascendancyPoints;
    soundManager_.play(SoundEffect::LevelUp);
    log(trialName(trial), " is won! You gain an ascendancy point. Leave by the stairs (G) when ready.");
    openAscendancy(); // the first trial asks which ascendancy to take
}

std::string Application::trialAvailability(int trial) const {
    const int bit = 1 << (trial - 1);
    if (player_.trialsCleared & bit) return "Won.";
    if (!(player_.trialKeys & bit))
        return std::string("Sealed: needs the ") + trialSigil(trial) + (trial == 1 ? ", carried by the Goblin Warlord." : ", carried by the Lich.");
    if (trial == 2 && !(player_.trialsCleared & 1)) return std::string("Win the ") + trialName(1) + " first.";
    return {};
}

// A round stone arena: pillars around the edge, the guardian at the far
// end, and the stairs home behind the player.
bool Application::enterTrial(int trial) {
    if (mode_ != GameMode::Town || trial < 1 || trial > kTrialCount || !trialAvailability(trial).empty()) return false;
    dissolveMinions();
    floorCache_[currentFloor_] = captureState(false);
    trialReturnFloor_ = currentFloor_;
    trial_ = trial;
    currentFloor_ = trialDifficultyFloor(trial);

    autoExploring_ = false; exploreSeenInterests_.clear(); cancelTargeting();
    inventoryOpen_ = false; trialMenu_ = false; merchantOpen_ = false; dungeonMenu_ = false;
    vaultExists_ = vaultOpened_ = vaultClaimed_ = false; vaultRewards_.clear();
    landmark_ = LandmarkKind::None; landmarkUsed_ = false; shrineMenu_ = false; exitMenu_ = false; extraLandmarks_.clear(); decals_.clear();
    chestExists_ = chestClaimed_ = false;
    groundItems_.clear();

    Map arena(kTrialArenaWidth, kTrialArenaHeight);
    const int cx = kTrialArenaWidth / 2, cy = kTrialArenaHeight / 2;
    for (int y = 0; y < kTrialArenaHeight; ++y)
        for (int x = 0; x < kTrialArenaWidth; ++x) {
            // An ellipse of open floor inside thick walls.
            const float dx = (x - cx) / (kTrialArenaWidth / 2.f - 1.5f), dy = (y - cy) / (kTrialArenaHeight / 2.f - 1.5f);
            const bool open = dx * dx + dy * dy <= 1.f;
            arena.setTile(x, y, Tile{open ? TileType::Floor : TileType::Wall, open, open});
        }
    for (const Position p : {Position{cx - 7, cy - 4}, Position{cx + 7, cy - 4}, Position{cx - 7, cy + 4}, Position{cx + 7, cy + 4},
                             Position{cx - 10, cy}, Position{cx + 10, cy}})
        arena.setTile(p.x, p.y, Tile{TileType::Wall, false, false});
    map_ = arena;
    clearSurfaces();
    lightOrbs_.clear();
    actorAnims_.clear(); corpses_.clear(); previousCameraX_ = previousCameraY_ = INT_MIN; vfx_.clear(); hitFlash_.clear();
    setProps({});

    const Position start{cx, kTrialArenaHeight - 3};
    player_.setPosition(start);
    floorEntrance_ = start;
    floorExit_ = {-1, -1};

    boss_ = nullptr;
    monsters_.clear();
    const bool stone = trial == 1;
    auto guardian = createMonster(stone ? MonsterType::GoblinWarlord : MonsterType::Lich, {cx, 3});
    scaleDungeonMonster(*guardian, currentFloor_);
    guardian->eventChampion = stone ? kChampionStoneWarden : kChampionFallenSaint;
    guardian->setName(championName(guardian->eventChampion));
    guardian->stats().maxHp = guardian->stats().maxHp * (stone ? 3 : 4) / 2;
    guardian->stats().hp = guardian->stats().maxHp;
    guardian->lastObservedHp = guardian->stats().hp;
    boss_ = guardian.get();
    monsters_.push_back(std::move(guardian));
    // Braziers ring the arena: light to fight by, and coals to kick.
    placeBraziers({cx, cy}, {{-4, -2}, {4, -2}, {-4, 3}, {4, 3}});

    exploredMap_ = ExploredMap(map_);
    scheduler_ = TurnScheduler{};
    scheduler_.add(player_);
    for (auto& m : monsters_) scheduler_.add(*m);
    currentActor_ = &scheduler_.nextTurn();
    mode_ = GameMode::Playing;
    updateFieldOfView();
    log("You enter the ", trialName(trial), ". ", trialGuardian(trial), " awaits. There is no leaving while it lives.");
    return true;
}

// Puts back the dungeon floor the trial was entered from, exactly as it was.
bool Application::leaveTrialState() {
    if (!trial_) return false;
    const auto found = floorCache_.find(trialReturnFloor_);
    trial_ = 0;
    if (found == floorCache_.end()) {
        currentFloor_ = trialReturnFloor_;
        regenerateLevel(std::random_device{}());
        return true;
    }
    auto next = captureState(false);
    importFloor(next, found->second);
    next.currentFloor = trialReturnFloor_;
    next.playerPosition = found->second.playerPosition;
    next.trial = 0;
    next.trialReturnFloor = 0;
    return restoreState(next, false);
}

void Application::exitTrial() {
    if (!trial_) return;
    if (boss_ && boss_->stats().hp > 0) {
        log(boss_->name(), " bars the way. Win, or die trying.");
        return;
    }
    const std::string name = trialName(trial_);
    if (!leaveTrialState()) return;
    mode_ = GameMode::Town; selling_ = false; shopSelection_ = 0; merchantOpen_ = false; dungeonMenu_ = false; trialMenu_ = false;
    log("You leave the ", name, " and return to town.");
}

// --- The ascendancy screen -------------------------------------------------------

void Application::chooseAscendancy(const std::string& id) {
    const auto* a = findAscendancy(id);
    if (!a || !ascendancyAllowed(*a, playerClass_) || !player_.ascendancy.empty() || !player_.trialsCleared) return;
    player_.ascendancy = a->id;
    ascendancyChoice_ = false;
    log("You ascend: you are now a ", a->name, "!");
    openAscendancy();
}

void Application::openAscendancy() {
    const auto* a = findAscendancy(player_.ascendancy);
    if (!a && player_.trialsCleared) {
        // A trial won but no ascendancy chosen yet: choose first.
        cancelTargeting(); inventoryOpen_ = false;
        ascendancyChoice_ = true; ascendancyMenu_ = true; ascendancyChoiceSelection_ = 0;
        return;
    }
    if (!a) { log("You have no ascendancy yet. Win the ", trialName(1), " to earn one."); return; }
    ascendancyChoice_ = false;
    cancelTargeting();
    inventoryOpen_ = false;
    ascendancyMenu_ = true;
    ascendancySelection_ = 0;
    for (std::size_t i = 0; i < a->nodes.size(); ++i)
        if (!player_.talents().rankOf(a->nodes[i])) { ascendancySelection_ = i; break; }
}

bool Application::learnAscendancyNode(std::size_t node) {
    const auto* a = findAscendancy(player_.ascendancy);
    if (!a || node >= a->nodes.size()) return false;
    if (player_.talents().rankOf(a->nodes[node])) { log("You already have that node."); return false; }
    if (player_.ascendancyPoints <= 0) { log("No ascendancy points. Win another trial to earn one."); return false; }
    const auto* d = findTalentDefinition(a->nodes[node]);
    if (!d) return false;
    player_.talents().learnTalent(d->ranks[0]);
    --player_.ascendancyPoints;
    player_.refreshEquipmentStats();
    soundManager_.play(SoundEffect::LevelUp);
    log(a->name, ": you learn ", d->ranks[0].name, ".", d->ranks[0].passive ? "" : " It is on your hotbar.");
    return true;
}

void Application::handleAscendancyKey(sf::Keyboard::Key key) {
    using K = sf::Keyboard::Key;
    if (ascendancyChoice_) {
        // The choice is permanent and can't be skipped.
        const auto options = ascendanciesFor(playerClass_);
        if (key >= K::Num1 && key <= K::Num4 && static_cast<std::size_t>(static_cast<int>(key) - static_cast<int>(K::Num1)) < options.size())
            ascendancyChoiceSelection_ = static_cast<std::size_t>(static_cast<int>(key) - static_cast<int>(K::Num1));
        if (key == K::Left && ascendancyChoiceSelection_ > 0) --ascendancyChoiceSelection_;
        if (key == K::Right && ascendancyChoiceSelection_ + 1 < options.size()) ++ascendancyChoiceSelection_;
        if (key == K::Enter && ascendancyChoiceSelection_ < options.size()) chooseAscendancy(options[ascendancyChoiceSelection_]->id);
        return;
    }
    if (key == K::Escape || key == K::Y) { ascendancyMenu_ = false; return; }
    if (key >= K::Num1 && key <= K::Num6) ascendancySelection_ = static_cast<std::size_t>(static_cast<int>(key) - static_cast<int>(K::Num1));
    if (key == K::Left) ascendancySelection_ = (ascendancySelection_ + 5) % 6;
    if (key == K::Right) ascendancySelection_ = (ascendancySelection_ + 1) % 6;
    if (key == K::Up || key == K::Down) ascendancySelection_ = (ascendancySelection_ + 3) % 6;
    if (key == K::Enter) learnAscendancyNode(ascendancySelection_);
}

void Application::handleAscendancyMouse(const sf::Event& event) {
    if (const auto* move = event.getIf<sf::Event::MouseMoved>()) mousePixel_ = move->position;
    const auto* click = event.getIf<sf::Event::MouseButtonPressed>();
    if (!click || click->button != sf::Mouse::Button::Left) return;
    const auto p = sf::Vector2f(click->position);
    if (ascendancyChoice_) {
        const auto options = ascendanciesFor(playerClass_);
        for (std::size_t i = 0; i < options.size(); ++i) if (ascendChoice(static_cast<int>(i)).contains(p)) { ascendancyChoiceSelection_ = i; return; }
        if (kAscendLearn.contains(p) && ascendancyChoiceSelection_ < options.size()) chooseAscendancy(options[ascendancyChoiceSelection_]->id);
        return;
    }
    if (kAscendClose.contains(p)) { ascendancyMenu_ = false; return; }
    if (kAscendLearn.contains(p)) { learnAscendancyNode(ascendancySelection_); return; }
    for (std::size_t i = 0; i < 6; ++i) if (ascendNode(static_cast<int>(i)).contains(p)) { ascendancySelection_ = i; return; }
}

// The first trial's reward: choose one of the class's four ascendancies.
void Application::renderAscendancyChoice() {
    const auto mouse = mousePixel_ ? std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)) : std::nullopt;
    const auto hovered = [&](const sf::FloatRect& r) { return mouse && r.contains(*mouse); };
    beginMenu(160);
    ui_.panel(window_, kAscendDialog, true, sf::Color(150, 135, 150));
    const float x = kAscendDialog.position.x, w = kAscendDialog.size.x, top = kAscendDialog.position.y;
    ui_.textCentered(window_, "Choose your ascendancy", {{x, top + 22}, {w, 44}}, 34, ui::kUnique, ui::Font::Title);
    ui_.textCentered(window_, "This choice is permanent. Each trial won buys one of its six nodes.", {{x, top + 70}, {w, 22}}, 16, ui::kMuted);
    const auto options = ascendanciesFor(playerClass_);
    const Talent* tip = nullptr;
    sf::Vector2f tipAt;
    for (std::size_t i = 0; i < options.size(); ++i) {
        const auto& a = *options[i];
        const auto r = ascendChoice(static_cast<int>(i));
        const bool selected = i == ascendancyChoiceSelection_;
        ui_.inset(window_, r, selected ? ui::kUnique : hovered(r) ? ui::kBronze : sf::Color::Transparent);
        const sf::FloatRect emblem{{r.position.x + r.size.x / 2 - 34, r.position.y + 16}, {68, 68}};
        ui_.inset(window_, emblem, selected ? ui::kUnique : sf::Color::Transparent);
        ui_.icon(window_, a.icon, {{emblem.position.x + 10, emblem.position.y + 10}, {48, 48}}, selected ? ui::kUnique : ui::kText);
        ui_.textCentered(window_, std::to_string(i + 1) + ".  " + a.name, {{r.position.x, r.position.y + 92}, {r.size.x, 30}}, 22,
                         selected ? ui::kUnique : ui::kGold, ui::Font::Title);
        ui_.textCentered(window_, a.attributes, {{r.position.x, r.position.y + 122}, {r.size.x, 20}}, 13, ui::kInfo, ui::Font::Bold);
        float y = r.position.y + 150;
        ui_.paragraph(window_, a.tagline, r.position.x + 14, y, r.size.x - 28, 14, ui::kText);
        y += 10;
        for (const char* nodeId : a.nodes)
            if (const auto* d = findTalentDefinition(nodeId)) {
                const sf::FloatRect line{{r.position.x + 12, y - 1}, {r.size.x - 24, 20}};
                const bool over = hovered(line);
                ui_.text(window_, std::string(d->ranks[0].passive ? "- " : "+ ") + d->ranks[0].name, {r.position.x + 18, y}, 14,
                         over ? ui::kGold : d->ranks[0].passive ? ui::kMuted : ui::kMagic);
                if (over) { tip = &d->ranks[0]; tipAt = {line.position.x + line.size.x, line.position.y}; }
                y += 20;
            }
    }
    if (tip) {
        std::vector<ui::Line> lines{{tip->name, ui::kGold, 16, ui::Font::Bold},
                                    {tip->passive ? "Passive" : "Active, " + std::to_string(tip->manaCost) + " mana, cooldown " + std::to_string(tip->cooldownTurns),
                                     tip->passive ? ui::kInfo : ui::kMagic, 13},
                                    {tip->description, ui::kText, 14}};
        ui_.tooltip(window_, lines, tipAt, 280);
    }
    const auto& chosen = *options[std::min(ascendancyChoiceSelection_, options.size() - 1)];
    ui_.button(window_, kAscendLearn, std::string("Become a ") + chosen.name + " (Enter)", hovered(kAscendLearn), true, 16);
    ui_.text(window_, "1-4 or click to choose; hover a node to read it. + active, - passive.", {x + 30, kAscendLearn.position.y + 12}, 14, ui::kMuted);
}

void Application::renderAscendancy() {
    if (ascendancyMenu_ && ascendancyChoice_) { renderAscendancyChoice(); return; }
    const auto* a = findAscendancy(player_.ascendancy);
    if (!ascendancyMenu_ || !a) return;
    const auto mouse = mousePixel_ ? std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)) : std::nullopt;
    const auto hovered = [&](const sf::FloatRect& r) { return mouse && r.contains(*mouse); };
    beginMenu(150);
    ui_.panel(window_, kAscendDialog, true, sf::Color(150, 135, 150));
    const float x = kAscendDialog.position.x, w = kAscendDialog.size.x, top = kAscendDialog.position.y;
    const sf::FloatRect emblem{{x + 30, top + 22}, {72, 72}};
    ui_.inset(window_, emblem, ui::kUnique);
    ui_.icon(window_, a->icon, {{emblem.position.x + 12, emblem.position.y + 12}, {48, 48}}, ui::kUnique);
    ui_.text(window_, std::string("Ascendancy: ") + a->name, {x + 120, top + 22}, 32, ui::kUnique, ui::Font::Title);
    ui_.text(window_, a->tagline, {x + 122, top + 64}, 16, ui::kMuted);
    const int points = player_.ascendancyPoints;
    const std::string pointText = points ? std::to_string(points) + (points == 1 ? " point to spend." : " points to spend.")
                                         : "No points to spend.";
    ui_.text(window_, pointText + " Each trial won grants one; each point buys one node, for good.",
             {x + 122, top + 88}, 15, points ? ui::kRare : ui::kMuted);

    for (std::size_t i = 0; i < a->nodes.size(); ++i) {
        const auto r = ascendNode(static_cast<int>(i));
        const auto* d = findTalentDefinition(a->nodes[i]);
        if (!d) continue;
        const Talent& t = d->ranks[0];
        const bool learned = player_.talents().rankOf(d->id) > 0;
        const bool selected = i == ascendancySelection_;
        ui_.inset(window_, r, learned ? ui::kUnique : selected ? ui::kGold : hovered(r) ? ui::kBronze : sf::Color::Transparent);
        const sf::FloatRect icon{{r.position.x + 14, r.position.y + 14}, {56, 56}};
        ui_.inset(window_, icon, learned ? ui::kUnique : sf::Color::Transparent);
        ui_.icon(window_, talentIcon(t), {{icon.position.x + 8, icon.position.y + 8}, {40, 40}}, learned ? ui::kUnique : ui::kText);
        ui_.text(window_, std::to_string(i + 1) + ".  " + t.name, {r.position.x + 82, r.position.y + 14}, 20,
                 learned ? ui::kUnique : ui::kGold, ui::Font::Title);
        std::string kind = t.passive ? "Passive" : "Active";
        if (!t.passive) kind += "   " + std::to_string(t.manaCost) + " mana, cooldown " + std::to_string(t.cooldownTurns);
        ui_.text(window_, kind, {r.position.x + 84, r.position.y + 44}, 13, t.passive ? ui::kInfo : ui::kMagic, ui::Font::Bold);
        float y = r.position.y + 80;
        ui_.paragraph(window_, t.description, r.position.x + 16, y, r.size.x - 32, 15, ui::kText, ui::Font::Body, r.position.y + r.size.y - 26);
        if (learned) ui_.text(window_, "Learned", {r.position.x + r.size.x - 74, r.position.y + r.size.y - 26}, 14, ui::kUnique, ui::Font::Bold);
    }
    const auto* chosen = findTalentDefinition(a->nodes[ascendancySelection_]);
    const bool canLearn = points > 0 && chosen && !player_.talents().rankOf(chosen->id);
    ui_.button(window_, kAscendLearn, chosen ? "Learn " + chosen->ranks[0].name + " (Enter)" : "Learn", hovered(kAscendLearn), canLearn, 16);
    ui_.button(window_, kAscendClose, "Close (Esc)", hovered(kAscendClose), true, 16);
    ui_.text(window_, "1-6 or click to choose. Your nodes are permanent. Y opens this screen.",
             {x + 30, kAscendLearn.position.y + 12}, 14, ui::kMuted);
}

// --- The trial obelisk in town ---------------------------------------------------------

void Application::handleTrialMenuKey(sf::Keyboard::Key key) {
    if (key == sf::Keyboard::Key::Escape) { trialMenu_ = false; return; }
    if (key == sf::Keyboard::Key::Num1) enterTrial(1);
    if (key == sf::Keyboard::Key::Num2) enterTrial(2);
}

void Application::handleTrialMenuMouse(const sf::Event& event) {
    if (const auto* move = event.getIf<sf::Event::MouseMoved>()) mousePixel_ = move->position;
    const auto* click = event.getIf<sf::Event::MouseButtonPressed>();
    if (!click || click->button != sf::Mouse::Button::Left) return;
    const auto p = sf::Vector2f(click->position);
    if (kTrialClose.contains(p)) { trialMenu_ = false; return; }
    for (int i = 0; i < kTrialCount; ++i)
        if (trialEnter(i).contains(p)) { enterTrial(i + 1); return; }
}

void Application::renderTrialMenu() {
    if (!trialMenu_) return;
    const auto mouse = mousePixel_ ? std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)) : std::nullopt;
    const auto hovered = [&](const sf::FloatRect& r) { return mouse && r.contains(*mouse); };
    beginMenu(140);
    ui_.panel(window_, kTrialDialog, true, sf::Color(140, 130, 150));
    const float x = kTrialDialog.position.x, w = kTrialDialog.size.x, top = kTrialDialog.position.y;
    ui_.textCentered(window_, "The Trial Obelisk", {{x, top + 18}, {w, 44}}, 34, ui::kUnique, ui::Font::Title);
    const auto* a = findAscendancy(player_.ascendancy);
    ui_.textCentered(window_, std::string("Sigils from the great bosses open its trials. Win them to ascend") +
                     (a ? std::string(" further as a ") + a->name + "." : "; the first lets you choose how."), {{x, top + 64}, {w, 22}}, 16, ui::kMuted);
    const char* rewards[]{"Reward: your ascendancy and its first point.", "Reward: a second ascendancy point."};
    const char* fights[]{"A living statue that slams the ground and quakes the arena. Its blows grow wilder as it cracks.",
                         "A saint who fell to the Lich: bolts, curses and rituals that raise the dead."};
    for (int i = 0; i < kTrialCount; ++i) {
        const int trial = i + 1;
        const auto r = trialCard(i);
        const auto status = trialAvailability(trial);
        const bool won = player_.trialsCleared & (1 << i);
        ui_.inset(window_, r, status.empty() ? ui::kUnique : won ? ui::kGood : sf::Color::Transparent);
        ui_.text(window_, std::to_string(trial) + ".  " + trialName(trial), {r.position.x + 18, r.position.y + 14}, 24,
                 status.empty() ? ui::kUnique : ui::kGold, ui::Font::Title);
        ui_.text(window_, std::string("Guardian: ") + trialGuardian(trial), {r.position.x + 20, r.position.y + 50}, 15, ui::kText, ui::Font::Bold);
        float y = r.position.y + 78;
        ui_.paragraph(window_, fights[i], r.position.x + 20, y, r.size.x - 40, 15, ui::kText);
        y += 8;
        ui_.paragraph(window_, rewards[i], r.position.x + 20, y, r.size.x - 40, 15, ui::kRare);
        y += 8;
        ui_.paragraph(window_, status.empty() ? "Ready. Once inside, there is no leaving while the guardian lives." : status,
                      r.position.x + 20, y, r.size.x - 40, 14, status.empty() ? ui::kInfo : won ? ui::kGood : ui::kBad);
        ui_.button(window_, trialEnter(i), status.empty() ? std::string("Enter the trial (") + std::to_string(trial) + ")" : won ? "Won" : "Sealed",
                   hovered(trialEnter(i)), status.empty(), 16);
    }
    ui_.button(window_, kTrialClose, "Leave (Esc)", hovered(kTrialClose), true, 16);
}

} // namespace engine
