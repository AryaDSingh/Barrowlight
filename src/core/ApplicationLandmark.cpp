#include "core/Application.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "core/PlayLayout.hpp"
#include "core/ScreenLayout.hpp"
#include "entities/MonsterFactory.hpp"
#include "entities/StatusEffectLogic.hpp"
#include <limits>
#include "world/FloorTheme.hpp"

namespace engine {

namespace {

constexpr const char* kTileset = "calciumtrice/tiles/dungeon_tileset_calciumtrice.png";
constexpr const char* kWaterFountain = "evildungeon/water-fountain.png";
constexpr const char* kBloodFountain = "evildungeon/blood-fountain.png";
constexpr const char* kPentagram = "evildungeon/pentagramm.png";

// Statue altars from the tileset: a warrior in the Barracks, the antlered
// idol in the Sanctum and the beast altar in the Crypts.
SpriteFrame altarFrame(FloorRegion region) {
    if (region == FloorRegion::Barracks) return {kTileset, sf::IntRect({304, 304}, {16, 32})};
    if (region == FloorRegion::Sanctum) return {kTileset, sf::IntRect({336, 304}, {16, 32})};
    return {kTileset, sf::IntRect({384, 304}, {32, 32})};
}

constexpr int kMightLifePercent = 15;
constexpr int kGraceGold = 25;
constexpr int kBloodFontLife = 4;
constexpr int kBloodFontCostPercent = 30;
constexpr int kRitualDoomPercent = 30;
constexpr int kRitualDoomTurns = 10;
constexpr int kPickLockDexterity = 12;
constexpr int kCageAlertRadius = 14;

int handfulGold(int floor) { return 10 + 3 * floor; }
int peddlerGold(int floor) { return 150 + 10 * floor; }
constexpr int kPeddlerLife = 10;
constexpr int kDemonDoomPercent = 40, kDemonDoomTurns = 12, kDemonDrainTurns = 40, kDemonDrain = 2;
int hoardGold(int floor) { return 40 + 12 * floor; }

const char* verb(LandmarkKind kind) {
    switch (kind) {
        case LandmarkKind::HealingFountain: case LandmarkKind::BloodFont: return "drink";
        case LandmarkKind::RitualCircle: return "step into the circle";
        case LandmarkKind::TreasureHoard: return "search the hoard";
        case LandmarkKind::PrisonerCage: return "open the cage";
        case LandmarkKind::ChampionPit: return "light the brazier";
        case LandmarkKind::SealedTomb: return "break the seal";
        case LandmarkKind::PalePeddler: return "speak with him";
        case LandmarkKind::ChainedDemon: return "face it";
        default: return "kneel";
    }
}

} // namespace

bool Application::nearAltar() const {
    if (landmark_ == LandmarkKind::None) return false;
    const auto p = player_.position();
    return std::abs(p.x - landmarkAltar_.x) <= 1 && std::abs(p.y - landmarkAltar_.y) <= 1;
}

void Application::openShrine() {
    if (landmark_ == LandmarkKind::None) return;
    if (landmarkUsed_) { log("The ", landmarkName(landmark_, floorTheme(currentFloor_).region), " is spent."); return; }
    cancelTargeting();
    shrineMenu_ = true;
}

// What this floor's landmark offers. Each choice is used at most once,
// since any choice spends the landmark.
std::vector<Application::LandmarkChoice> Application::landmarkChoices() const {
    const auto& stats = player_.stats();
    switch (landmark_) {
        case LandmarkKind::Shrine: {
            const int might = std::max(1, stats.maxHp * kMightLifePercent / 100);
            return {{"Restoration", "healing", "Restore all life and mana and reset every cooldown.", "Free", true},
                    {"Might", "glowing-hands", "+3 damage on direct attacks for 80 turns.",
                     "Costs " + std::to_string(might) + " life", stats.hp > might},
                    {"Grace", "dodging", "+10% dodge for 80 turns.", "Costs " + std::to_string(kGraceGold) + " gold",
                     gold_ >= kGraceGold}};
        }
        case LandmarkKind::HealingFountain:
            return {{"Drink", "healing", "Restore all life and mana, and wash away poison, burns, chills and curses.", "Free", true}};
        case LandmarkKind::BloodFont: {
            const int cost = std::max(1, stats.hp * kBloodFontCostPercent / 100);
            return {{"Drink deep", "bleeding-heart", "+" + std::to_string(kBloodFontLife) + " maximum life, for the rest of the run.",
                     "Costs " + std::to_string(cost) + " life now", stats.hp > cost}};
        }
        case LandmarkKind::RitualCircle: {
            const int doom = std::max(1, stats.maxHp * kRitualDoomPercent / 100);
            return {{"Accept the pact", "skull-crossed-bones", "Gain an attribute point to spend at once.",
                     "Doom: lose " + std::to_string(doom) + " life in " + std::to_string(kRitualDoomTurns) +
                         " turns unless you cleanse it", true}};
        }
        case LandmarkKind::TreasureHoard:
            return {{"Take a handful", "knapsack", "+" + std::to_string(handfulGold(currentFloor_)) + " gold. Nothing stirs.", "Free", true},
                    {"Seize it all", "locked-chest", "+" + std::to_string(hoardGold(currentFloor_)) +
                     " gold and a rare item. Its guardians wake: three foes, one elite, come for you.", "Guardians attack", true}};
        case LandmarkKind::PrisonerCage:
            return {{"Break the lock", "hammer-drop", "The prisoner repays you with a rare item. The clang wakes every enemy within " +
                     std::to_string(kCageAlertRadius) + " steps.", "Loud", true},
                    {"Pick the lock", "stiletto", "Quietly. The prisoner repays you with a magic item.",
                     "Needs " + std::to_string(kPickLockDexterity) + " Dexterity (you have " + std::to_string(stats.dexterity) + ")",
                     stats.dexterity >= kPickLockDexterity}};
        case LandmarkKind::ChampionPit:
            return {{"Light the brazier", "campfire", "A nightmare champion answers the challenge. It drops a rare item when it falls.",
                     "One champion", true},
                    {"Stoke it high", "sword-clash", "Two nightmare champions answer at once, each with a rare item.", "Two champions", true}};
        case LandmarkKind::SealedTomb:
            return {{"Break the seal", "tombstone", "The Risen King climbs out with two skeletal guards. Slay him for a unique item.",
                     "A nightmare champion", true}};
        case LandmarkKind::PalePeddler:
            return {{"Pay in gold", "locked-chest", "He sells you one unique item, sight unseen.",
                     "Costs " + std::to_string(peddlerGold(currentFloor_)) + " gold", gold_ >= peddlerGold(currentFloor_)},
                    {"Pay in blood", "bleeding-heart", "He sells you one unique item, sight unseen.",
                     "Costs " + std::to_string(kPeddlerLife) + " maximum life, forever", player_.baseStats().maxHp > 3 * kPeddlerLife}};
        case LandmarkKind::ChainedDemon: {
            const int doom = std::max(1, stats.maxHp * kDemonDoomPercent / 100);
            return {{"Accept its bargain", "skull-crossed-bones", "It hands you a unique item from its hoard.",
                     "Doom: lose " + std::to_string(doom) + " life in " + std::to_string(kDemonDoomTurns) +
                         " turns unless cleansed, and mana drains for " + std::to_string(kDemonDrainTurns) + " turns", true},
                    {"Slay it in its chains", "decapitation", "The chains snap and it fights. Kill it for a unique item.",
                     "A nightmare champion", true}};
        }
        case LandmarkKind::None: break;
    }
    return {};
}

int Application::spawnLandmarkFoes(int count, MonsterTier firstTier, MonsterTier restTier, std::optional<MonsterType> firstType) {
    // The floor's own roster, so the guardians fit the region.
    std::vector<MonsterType> roster;
    for (const auto& m : monsters_)
        if (!m->allied && !m->vaultGuard && m.get() != boss_ && !isUniqueMonster(m->type()) && m->rewardsEligible())
            roster.push_back(m->type());
    if (roster.empty()) roster.push_back(floorTheme(currentFloor_).region == FloorRegion::Crypts ? MonsterType::Skeleton : MonsterType::Goblin);

    // Walkable tiles 3-7 steps from the altar, away from the player.
    std::vector<int> steps(static_cast<std::size_t>(map_.width() * map_.height()), -1);
    std::vector<Position> frontier, candidates;
    const auto index = [&](Position p) { return static_cast<std::size_t>(p.y * map_.width() + p.x); };
    for (const Position d : {Position{1, 0}, Position{-1, 0}, Position{0, 1}, Position{0, -1}}) {
        const Position p{landmarkAltar_.x + d.x, landmarkAltar_.y + d.y};
        if (map_.inBounds(p.x, p.y) && map_.isWalkable(p.x, p.y)) { steps[index(p)] = 1; frontier.push_back(p); }
    }
    for (std::size_t i = 0; i < frontier.size(); ++i) {
        const Position p = frontier[i];
        const int s = steps[index(p)];
        const auto player = player_.position();
        if (s >= 3 && s <= 7 && std::max(std::abs(p.x - player.x), std::abs(p.y - player.y)) > 2 && !isOccupied(p, nullptr))
            candidates.push_back(p);
        if (s >= 7) continue;
        for (const Position d : {Position{1, 0}, Position{-1, 0}, Position{0, 1}, Position{0, -1}}) {
            const Position n{p.x + d.x, p.y + d.y};
            if (!map_.inBounds(n.x, n.y) || !map_.isWalkable(n.x, n.y) || steps[index(n)] >= 0) continue;
            steps[index(n)] = s + 1;
            frontier.push_back(n);
        }
    }
    int spawned = 0;
    std::vector<Position> chosen;
    for (int attempt = 0; attempt < 64 && spawned < count && !candidates.empty(); ++attempt) {
        const Position p = candidates[loot_.roll(static_cast<unsigned>(candidates.size()))];
        if (std::any_of(chosen.begin(), chosen.end(), [&](Position q) { return std::abs(p.x - q.x) + std::abs(p.y - q.y) < 2; })) continue;
        const MonsterType type = spawned == 0 && firstType ? *firstType : roster[loot_.roll(static_cast<unsigned>(roster.size()))];
        auto monster = createMonster(type, p, spawned == 0 ? firstTier : restTier);
        scaleDungeonMonster(*monster, currentFloor_);
        monster->tactics.concealed = false;
        monster->tactics.alert = 8;
        monster->tactics.lastKnown = player_.position();
        monster->recoveryActions = 1; // no attack before the player can respond
        scheduler_.add(*monster);
        monsters_.push_back(std::move(monster));
        chosen.push_back(p);
        ++spawned;
    }
    return spawned;
}

const ItemDefinition& Application::pickUnique() {
    const auto uniques = uniqueItemDefinitions();
    std::vector<const ItemDefinition*> unowned;
    const auto& inventory = player_.inventory();
    for (const auto* d : uniques) {
        bool owned = std::any_of(inventory.items().begin(), inventory.items().end(), [&](const auto& item) { return item->definition() == d; });
        for (int slot = 0; slot < kEquipmentSlotCount && !owned; ++slot)
            if (const auto* worn = inventory.equipped(static_cast<EquipmentSlot>(slot))) owned = worn->definition() == d;
        if (!owned) unowned.push_back(d);
    }
    const auto& pool = unowned.empty() ? uniques : unowned;
    return *pool[loot_.roll(static_cast<unsigned>(pool.size()))];
}

void Application::grantUnique(std::optional<Position> ground) {
    if (nextItemId_ == std::numeric_limits<std::uint64_t>::max()) return;
    auto item = std::make_unique<Item>(pickUnique(), nextItemId_++, ground.value_or(player_.position()), std::vector<RolledAffix>{}, 5);
    soundManager_.play(SoundEffect::LevelUp);
    if (ground) {
        log("A unique item falls: ", item->name(), "!");
        groundItems_.push_back(std::move(item));
    } else {
        log("Unique item: ", item->name(), " (in your bag).");
        player_.inventory().add(std::move(item));
    }
}

void Application::raiseChampion(MonsterType type, int champion) {
    if (!spawnLandmarkFoes(1, MonsterTier::Nightmare, MonsterTier::Nightmare, type)) return;
    auto& m = *monsters_.back();
    m.eventChampion = champion;
    m.setName(championName(champion));
    m.stats().maxHp = m.stats().maxHp * 3 / 2;
    m.stats().hp = m.stats().maxHp;
    m.lastObservedHp = m.stats().hp;
    log(m.name(), " rises to face you!");
}

void Application::landmarkReward(ItemRarity rarity) {
    if (nextItemId_ == std::numeric_limits<std::uint64_t>::max()) return;
    const auto theme = currentFloor_ <= 3 ? LootTheme::Barracks : currentFloor_ <= 6 ? LootTheme::Sanctum : LootTheme::Crypts;
    auto item = loot_.generate(currentFloor_, rarity == ItemRarity::Rare ? 2 : 1, nextItemId_++, player_.position(), rarity, theme);
    log("You receive ", item->name(), " (in your bag).");
    player_.inventory().add(std::move(item)); // kept in overflow rather than lost
}

void Application::chooseBlessing(int choice) {
    const auto choices = landmarkChoices();
    if (!shrineMenu_ || landmarkUsed_ || choice < 0 || choice >= static_cast<int>(choices.size())) return;
    if (!choices[static_cast<std::size_t>(choice)].affordable) {
        log("You can't pay that price right now.");
        return;
    }
    auto& stats = player_.stats();
    bool attributePoint = false;
    switch (landmark_) {
        case LandmarkKind::Shrine:
            if (choice == 0) {
                stats.hp = stats.maxHp; stats.mana = stats.maxMana;
                player_.talents().resetCooldowns();
            } else if (choice == 1) {
                stats.hp -= std::max(1, stats.maxHp * kMightLifePercent / 100);
                player_.statusEffects().apply({StatusEffectType::Empowered, 80, 3});
            } else {
                gold_ -= kGraceGold;
                player_.statusEffects().apply({StatusEffectType::Evasion, 80, 10});
            }
            break;
        case LandmarkKind::HealingFountain: {
            stats.hp = stats.maxHp; stats.mana = stats.maxMana;
            auto& effects = player_.statusEffects().active();
            effects.erase(std::remove_if(effects.begin(), effects.end(),
                [](const StatusEffectInstance& e) { return isCleansable(e.type); }), effects.end());
            break;
        }
        case LandmarkKind::BloodFont: {
            const int cost = std::max(1, stats.hp * kBloodFontCostPercent / 100);
            player_.baseStats().maxHp += kBloodFontLife;
            const int hp = stats.hp - cost;
            player_.refreshEquipmentStats();
            player_.stats().hp = std::max(1, hp);
            break;
        }
        case LandmarkKind::RitualCircle:
            player_.statusEffects().apply({StatusEffectType::Doom, kRitualDoomTurns, std::max(1, stats.maxHp * kRitualDoomPercent / 100)});
            ++player_.unspentAttributePoints();
            attributePoint = true;
            break;
        case LandmarkKind::TreasureHoard:
            gold_ += choice == 0 ? handfulGold(currentFloor_) : hoardGold(currentFloor_);
            if (choice == 1) {
                landmarkReward(ItemRarity::Rare);
                const int woke = spawnLandmarkFoes(3, MonsterTier::Elite, MonsterTier::Base);
                if (woke) log("The hoard's guardians wake! ", woke, " foes close in.");
            }
            break;
        case LandmarkKind::PrisonerCage:
            landmarkReward(choice == 0 ? ItemRarity::Rare : ItemRarity::Magic);
            if (choice == 0) {
                int woke = 0;
                for (auto& m : monsters_) {
                    const int dx = m->position().x - player_.position().x, dy = m->position().y - player_.position().y;
                    if (m->allied || m->stats().hp <= 0 || dx * dx + dy * dy > kCageAlertRadius * kCageAlertRadius) continue;
                    woke += m->tactics.alert == 0;
                    m->tactics.alert = 8;
                    m->tactics.lastKnown = player_.position();
                }
                log("The lock shatters with a clang that echoes down the halls.", woke ? " Enemies are coming." : "");
            } else {
                log("The lock clicks open without a sound.");
            }
            break;
        case LandmarkKind::ChampionPit: {
            const int answered = spawnLandmarkFoes(choice == 0 ? 1 : 2, MonsterTier::Nightmare, MonsterTier::Nightmare);
            log(answered == 1 ? "A champion steps into the pit!" : "Champions step into the pit!");
            break;
        }
        case LandmarkKind::SealedTomb:
            raiseChampion(MonsterType::CryptSentinel, kChampionRevenant);
            spawnLandmarkFoes(2, MonsterTier::Base, MonsterTier::Base, MonsterType::Skeleton);
            break;
        case LandmarkKind::PalePeddler:
            if (choice == 0) gold_ -= peddlerGold(currentFloor_);
            else {
                player_.baseStats().maxHp -= kPeddlerLife;
                const int hp = std::min(stats.hp, player_.baseStats().maxHp);
                player_.refreshEquipmentStats();
                player_.stats().hp = std::max(1, std::min(hp, player_.stats().maxHp));
            }
            grantUnique(std::nullopt);
            log("\"A pleasure.\" The peddler and his stall are gone.");
            break;
        case LandmarkKind::ChainedDemon:
            if (choice == 0) {
                grantUnique(std::nullopt);
                player_.statusEffects().apply({StatusEffectType::Doom, kDemonDoomTurns, std::max(1, stats.maxHp * kDemonDoomPercent / 100)});
                player_.statusEffects().apply({StatusEffectType::ManaDrain, kDemonDrainTurns, kDemonDrain});
                log("The demon laughs as it fades back into its chains.");
            } else {
                raiseChampion(MonsterType::Ogre, kChampionDemon);
            }
            break;
        case LandmarkKind::None: return;
    }
    landmarkUsed_ = true;
    shrineMenu_ = false;
    soundManager_.play(SoundEffect::LevelUp);
    const std::string name = landmarkName(landmark_, floorTheme(currentFloor_).region);
    log(name.rfind("The ", 0) == 0 ? "" : "The ", name, " grants you ",
        choices[static_cast<std::size_t>(choice)].name, ".");
    finishInventoryTurn(); // using a landmark takes a turn
    if (attributePoint && mode_ == GameMode::Playing) mode_ = GameMode::AttributeAllocation;
}

// Which dialog card a choice uses: a single offer sits in the middle.
static int choiceSlot(int choice, int count) { return count == 1 ? 1 : count == 2 ? choice * 2 : choice; }

void Application::handleShrineKey(sf::Keyboard::Key key) {
    if (key == sf::Keyboard::Key::Escape) { shrineMenu_ = false; return; }
    if (key == sf::Keyboard::Key::Num1) chooseBlessing(0);
    else if (key == sf::Keyboard::Key::Num2) chooseBlessing(1);
    else if (key == sf::Keyboard::Key::Num3) chooseBlessing(2);
    else if (key == sf::Keyboard::Key::Enter && landmarkChoices().size() == 1) chooseBlessing(0);
}

void Application::handleShrineMouse(const sf::Event& event) {
    if (const auto* move = event.getIf<sf::Event::MouseMoved>()) mousePixel_ = move->position;
    const auto* click = event.getIf<sf::Event::MouseButtonPressed>();
    if (!click || click->button != sf::Mouse::Button::Left) return;
    const auto p = sf::Vector2f(click->position);
    if (screen::kShrineLeave.contains(p)) { shrineMenu_ = false; return; }
    const int count = static_cast<int>(landmarkChoices().size());
    for (int i = 0; i < count; ++i)
        if (screen::shrineChoice(choiceSlot(i, count)).contains(p)) { chooseBlessing(i); return; }
}

// The landmark's art, glow and nearby hint; drawn after walls, before actors.
void Application::renderLandmark() {
    if (landmark_ == LandmarkKind::None) return;
    const auto vis = exploredMap_.at(landmarkAltar_.x, landmarkAltar_.y);
    if (vis == Visibility::Hidden) return;
    const auto at = worldToScreen(landmarkAltar_.x, landmarkAltar_.y);
    const float tile = static_cast<float>(playLayout::tileSize);
    const bool lit = vis == Visibility::Visible;
    const float now = animationClock_.getElapsedTime().asSeconds();
    const sf::Color tint = !lit ? sf::Color(90, 90, 100) : landmarkUsed_ ? sf::Color(150, 150, 160) : sf::Color::White;
    const int frame = static_cast<int>(now * 6.f) % 4;

    switch (landmark_) {
        case LandmarkKind::Shrine: {
            if (lit && !landmarkUsed_) {
                // A slow pulse marks a shrine that still answers.
                const float pulse = 0.5f + 0.5f * std::sin(now * 2.2f);
                sf::CircleShape glow(tile * 0.95f);
                glow.setOrigin({tile * 0.95f, tile * 0.95f});
                glow.setPosition({at.x + tile / 2, at.y + tile / 2});
                glow.setFillColor(sf::Color(255, 210, 120, static_cast<std::uint8_t>(40 + 50 * pulse)));
                window_.draw(glow);
            }
            const float size = tile * 1.6f;
            sprites_.draw(window_, altarFrame(floorTheme(currentFloor_).region), {at.x + (tile - size) / 2, at.y + tile - size},
                          size, tint);
            break;
        }
        case LandmarkKind::HealingFountain: case LandmarkKind::BloodFont: {
            // A gargoyle in the wall face, spouting onto the floor below it.
            // Rows of the sheet: idle, starting, flowing, stopping.
            const char* sheet = landmark_ == LandmarkKind::BloodFont ? kBloodFountain : kWaterFountain;
            const SpriteFrame gargoyle{sheet, sf::IntRect({(landmarkUsed_ ? 0 : frame) * 32, (landmarkUsed_ ? 0 : 2) * 96}, {32, 96})};
            sprites_.draw(window_, gargoyle, {at.x - tile, at.y - tile}, tile * 3, tint);
            break;
        }
        case LandmarkKind::RitualCircle: {
            // The circle on the floor; candles and flames burn while it's unspent.
            const SpriteFrame circle{kPentagram, sf::IntRect({(landmarkUsed_ ? 0 : frame) * 110, (landmarkUsed_ ? 0 : 2) * 120}, {110, 120})};
            sprites_.draw(window_, circle, {at.x + tile / 2 - 60, at.y + tile / 2 - 85}, 120.f, tint);
            break;
        }
        case LandmarkKind::TreasureHoard: {
            // Coins heaped between two chests; a handful is left once it's spent.
            if (lit && !landmarkUsed_) {
                const float pulse = 0.5f + 0.5f * std::sin(now * 2.6f);
                sf::CircleShape glow(tile * 0.8f);
                glow.setOrigin({tile * 0.8f, tile * 0.8f});
                glow.setPosition({at.x + tile / 2, at.y + tile * 0.6f});
                glow.setFillColor(sf::Color(255, 200, 80, static_cast<std::uint8_t>(30 + 40 * pulse)));
                window_.draw(glow);
            }
            sprites_.draw(window_, {kTileset, sf::IntRect({32, 304}, {16, 16})}, {at.x - tile * 0.45f, at.y - tile * 0.35f}, tile * 0.85f, tint);
            sprites_.draw(window_, {kTileset, sf::IntRect({48, 304}, {16, 16})}, {at.x + tile * 0.6f, at.y - tile * 0.35f}, tile * 0.85f, tint);
            const SpriteFrame coins{kTileset, sf::IntRect({landmarkUsed_ ? 0 : 16, 352}, {16, 16})};
            sprites_.draw(window_, coins, {at.x - tile * 0.05f, at.y + tile * 0.05f}, tile * 1.1f, tint);
            break;
        }
        case LandmarkKind::PrisonerCage: {
            // The prisoner behind iron bars; once freed, the door hangs open.
            if (!landmarkUsed_) {
                const int idle = static_cast<int>(now * 6.f) % 10;
                sprites_.draw(window_, {"calciumtrice/heroes/Simpleton.png", sf::IntRect({idle * 32, 0}, {32, 32})}, at, tile, tint);
            }
            sprites_.draw(window_, {kTileset, sf::IntRect({64, 176}, {16, 32})}, {at.x - tile * 0.5f, at.y - tile * 0.6f}, tile * 1.6f, tint);
            if (!landmarkUsed_)
                sprites_.draw(window_, {kTileset, sf::IntRect({80, 176}, {16, 32})}, {at.x + tile * 0.3f, at.y - tile * 0.6f}, tile * 1.6f, tint);
            break;
        }
        case LandmarkKind::ChampionPit: {
            // A brazier on a stone plinth; it burns until the challenge is made.
            sprites_.draw(window_, {kTileset, sf::IntRect({416, 304}, {32, 32})}, {at.x - tile * 0.25f, at.y - tile * 0.4f}, tile * 1.5f, tint);
            if (!landmarkUsed_) {
                const int flame = static_cast<int>(now * 8.f) % 3;
                sprites_.draw(window_, {kTileset, sf::IntRect({224 + flame * 16, 512}, {16, 16})}, {at.x, at.y - tile * 0.5f},
                              tile, tint);
            }
            break;
        }
        case LandmarkKind::SealedTomb: {
            // A stone tomb between two candles; violet light seeps from the seal.
            if (lit && !landmarkUsed_) {
                const float pulse = 0.5f + 0.5f * std::sin(now * 1.8f);
                sf::CircleShape glow(tile * 1.1f);
                glow.setOrigin({tile * 1.1f, tile * 1.1f});
                glow.setPosition({at.x + tile / 2, at.y + tile * 0.6f});
                glow.setFillColor(sf::Color(170, 110, 255, static_cast<std::uint8_t>(35 + 45 * pulse)));
                window_.draw(glow);
            }
            const sf::Color stone = landmarkUsed_ ? sf::Color(tint.r * 2 / 3, tint.g * 2 / 3, tint.b * 2 / 3) : tint;
            // Two plinth slabs end to end make the long tomb; bones on its lid.
            for (const float left : {-0.8f, 0.5f})
                sprites_.draw(window_, {kTileset, sf::IntRect({416, 304}, {32, 32})}, {at.x + left * tile, at.y - tile * 0.45f}, tile * 1.3f, stone);
            sprites_.draw(window_, {kTileset, sf::IntRect({112, 240}, {16, 16})}, {at.x + tile * 0.1f, at.y - tile * 0.35f}, tile * 0.8f, stone);
            for (const float side : {-0.55f, 1.05f}) {
                const int flicker = (static_cast<int>(now * 6.f) + (side > 0 ? 1 : 0)) % 3;
                if (!landmarkUsed_)
                    sprites_.draw(window_, {kTileset, sf::IntRect({176 + flicker * 16, 304}, {16, 16})}, {at.x + side * tile * 1.2f, at.y - tile * 0.5f},
                                  tile * 0.6f, tint);
            }
            break;
        }
        case LandmarkKind::PalePeddler: {
            // A pale, hooded figure at a chest of wares; gone once you've traded.
            if (!landmarkUsed_) {
                sprites_.draw(window_, {kTileset, sf::IntRect({48, 304}, {16, 16})}, {at.x + tile * 0.45f, at.y + tile * 0.1f}, tile * 0.9f, tint);
                const int idle = static_cast<int>(now * 5.f) % 10;
                const sf::Color pale(tint.r * 190 / 255, tint.g * 205 / 255, tint.b, 215);
                sprites_.draw(window_, {"calciumtrice/heroes/Priest.png", sf::IntRect({idle * 32, 0}, {32, 32})},
                              {at.x - tile * 0.35f, at.y - tile * 0.35f}, tile * 1.35f, pale);
            }
            break;
        }
        case LandmarkKind::ChainedDemon: {
            // The demon between two hanging chains; only the broken chains remain once it's gone.
            const SpriteFrame chain{"evildungeon/evildungeon_0.png", sf::IntRect({64, 224}, {32, 32})};
            if (!landmarkUsed_) {
                if (lit) {
                    const float pulse = 0.5f + 0.5f * std::sin(now * 2.4f);
                    sf::CircleShape glow(tile * 1.2f);
                    glow.setOrigin({tile * 1.2f, tile * 1.2f});
                    glow.setPosition({at.x + tile / 2, at.y + tile * 0.4f});
                    glow.setFillColor(sf::Color(255, 60, 40, static_cast<std::uint8_t>(30 + 45 * pulse)));
                    window_.draw(glow);
                }
                const int idle = static_cast<int>(now * 5.f) % 10;
                sprites_.draw(window_, {"calciumtrice/monsters/RedMinotaur.png", sf::IntRect({idle * 48, 0}, {48, 52})},
                              {at.x - tile * 0.5f, at.y - tile}, tile * 2.f, tint);
            }
            sprites_.draw(window_, chain, {at.x - tile * 0.9f, at.y - tile * 0.6f}, tile * 1.1f, tint);
            sprites_.draw(window_, chain, {at.x + tile * 0.8f, at.y - tile * 0.6f}, tile * 1.1f, tint, true);
            break;
        }
        case LandmarkKind::None: break;
    }
    if (nearAltar())
        mapHints_.push_back({std::string(landmarkName(landmark_, floorTheme(currentFloor_).region)) +
            (landmarkUsed_ ? ". Its power is spent." : std::string(". G or click it to ") + verb(landmark_) + "."), ui::kRare});
}

sf::Color Application::landmarkLightColor() const {
    switch (landmark_) {
        case LandmarkKind::HealingFountain: return sf::Color(110, 180, 255);
        case LandmarkKind::BloodFont: return sf::Color(255, 70, 60);
        case LandmarkKind::RitualCircle: return sf::Color(200, 110, 255);
        case LandmarkKind::TreasureHoard: return sf::Color(255, 210, 90);
        case LandmarkKind::ChampionPit: return sf::Color(255, 130, 60);
        case LandmarkKind::SealedTomb: return sf::Color(170, 120, 255);
        case LandmarkKind::PalePeddler: return sf::Color(190, 220, 255);
        case LandmarkKind::ChainedDemon: return sf::Color(255, 70, 50);
        default: return sf::Color(255, 205, 120);
    }
}

void Application::renderShrine() {
    if (!shrineMenu_) return;
    using namespace screen;
    const auto mouse = mousePixel_ ? std::optional<sf::Vector2f>(sf::Vector2f(*mousePixel_)) : std::nullopt;
    const auto hovered = [&](const sf::FloatRect& r) { return mouse && r.contains(*mouse); };
    sf::RectangleShape dim({1280, 720}); dim.setFillColor(sf::Color(0, 0, 0, 140)); window_.draw(dim);
    ui_.panel(window_, kShrineDialog, true, sf::Color(150, 140, 130));
    const float x = kShrineDialog.position.x, w = kShrineDialog.size.x;
    ui_.textCentered(window_, landmarkName(landmark_, floorTheme(currentFloor_).region),
        {{x, kShrineDialog.position.y + 20}, {w, 44}}, 34, ui::kRare, ui::Font::Title);
    const char* subtitle = landmark_ == LandmarkKind::Shrine ? "Kneel and choose one blessing. The shrine answers only once."
        : landmark_ == LandmarkKind::RitualCircle ? "Power, for a price. The circle burns out once used."
        : "The water runs only once for each traveller.";
    if (landmark_ == LandmarkKind::BloodFont) subtitle = "It flows red, and only once.";
    if (landmark_ == LandmarkKind::TreasureHoard) subtitle = "Gold, heaped and unguarded. Surely unguarded.";
    if (landmark_ == LandmarkKind::PrisonerCage) subtitle = "\"Get me out of here! I'll make it worth your while.\"";
    if (landmark_ == LandmarkKind::ChampionPit) subtitle = "Light the brazier and the pit's champions will answer.";
    if (landmark_ == LandmarkKind::SealedTomb) subtitle = "A king was buried here with his treasure. He is not resting.";
    if (landmark_ == LandmarkKind::PalePeddler) subtitle = "\"Rare things, for rare prices. One sale, then I'm gone.\"";
    if (landmark_ == LandmarkKind::ChainedDemon) subtitle = "\"Free me, or bargain with me. Either way, you leave richer.\"";
    if (rareLandmark(landmark_))
        ui_.textCentered(window_, "A very rare event", {{x, kShrineDialog.position.y + 4}, {w, 20}}, 13, ui::kUnique, ui::Font::Bold);
    ui_.textCentered(window_, subtitle, {{x, kShrineDialog.position.y + 66}, {w, 24}}, 16, ui::kMuted);
    const auto choices = landmarkChoices();
    const int count = static_cast<int>(choices.size());
    for (int i = 0; i < count; ++i) {
        const auto& c = choices[static_cast<std::size_t>(i)];
        const auto r = shrineChoice(choiceSlot(i, count));
        ui_.inset(window_, r, hovered(r) && c.affordable ? ui::kRare : sf::Color::Transparent);
        ui_.text(window_, std::to_string(i + 1), {r.position.x + 10, r.position.y + 6}, 15, ui::kMuted, ui::Font::Bold);
        const sf::FloatRect art{{r.position.x + r.size.x / 2 - 38, r.position.y + 18}, {76, 76}};
        ui_.inset(window_, art, sf::Color(255, 214, 96, 70));
        ui_.icon(window_, c.icon, {{art.position.x + 12, art.position.y + 12}, {52, 52}}, c.affordable ? ui::kRare : ui::kMuted);
        ui_.textCentered(window_, c.name, {{r.position.x, r.position.y + 104}, {r.size.x, 32}}, 22,
            c.affordable ? ui::kGold : ui::kMuted, ui::Font::Title);
        float y = r.position.y + 144;
        ui_.paragraph(window_, c.effect, r.position.x + 18, y, r.size.x - 36, 15, ui::kText);
        float costY = r.position.y + r.size.y - 50;
        ui_.paragraph(window_, c.cost, r.position.x + 18, costY, r.size.x - 36, 14, c.affordable ? ui::kGood : ui::kBad, ui::Font::Bold);
    }
    ui_.button(window_, kShrineLeave, "Leave (Esc)", hovered(kShrineLeave), true, 16);
}

} // namespace engine
