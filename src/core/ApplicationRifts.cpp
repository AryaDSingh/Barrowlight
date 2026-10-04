#include "core/Application.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "core/PlayLayout.hpp"
#include "entities/MonsterFactory.hpp"
#include "world/FloorTheme.hpp"

namespace engine {

// Path of Exile's Breach and Essence encounters.
//   Breach: touch the rift and it opens for kBreachTurns; creatures pour out
//   of its growing edge, and a Rift-keeper comes. Slay it to seal the breach
//   early and claim its spoils; otherwise the rift drags its creatures back.
//   Essence: a monster sealed in crystal. Free it (or corrupt it) and it
//   fights with its essence's power, then drops gear bearing that essence.

namespace {
constexpr const char* kRiftArt = "dcss/breach_rift.png";
constexpr int kKeeperTurn = 6;
const sf::Color kRiftColour(170, 110, 255);
int chebyshev(Position a, Position b) { return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y)); }

LootTheme lootTheme(int floor) { return floor <= 3 ? LootTheme::Barracks : floor <= 6 ? LootTheme::Sanctum : LootTheme::Crypts; }
} // namespace

// --- Names -----------------------------------------------------------------

std::string Application::plainName(const Monster& monster) {
    std::string name = monster.name();
    const std::string prefix = namePrefixForTier(monster.tier());
    if (!prefix.empty() && name.rfind(prefix, 0) == 0) name.erase(0, prefix.size());
    return name;
}

// Names, tints and powers that aren't saved directly: set on creation and
// again on load.
void Application::dressEventMonster(Monster& m) {
    if (m.rift == 2) m.setName("Rift-keeper");
    else if (m.rift == 1) m.setName("Riftborn " + plainName(m));
    if (m.essence) {
        m.setName(std::string(essenceInfo(static_cast<Essence>(m.essence)).look) + " " + plainName(m));
        const bool hasted = static_cast<Essence>(m.essence) == Essence::Haste;
        if (hasted || m.corrupted) m.stats().speed = m.stats().speed * (hasted && m.corrupted ? 2 : 3) / (hasted && m.corrupted ? 1 : 2);
    }
}

// --- Essence -----------------------------------------------------------------

Captive Application::essenceAt(Position altar) const {
    const unsigned h = static_cast<unsigned>(altar.x * 83492791) ^ static_cast<unsigned>(altar.y * 73856093) ^
                       static_cast<unsigned>(currentFloor_ * 19349663);
    const auto essence = static_cast<Essence>(1 + static_cast<int>(h % kEssenceKinds));
    std::array<MonsterType, 3> held{MonsterType::GoblinRaider, MonsterType::Ogre, MonsterType::GoblinBulwark};
    if (cathedralFloor(currentFloor_)) held = {MonsterType::DeepLurker, MonsterType::DrownedOne, MonsterType::DeepLurker};
    else if (floorTheme(currentFloor_).region == FloorRegion::Crypts) held = {MonsterType::CryptSentinel, MonsterType::SkeletonGuard, MonsterType::Skeleton};
    return {essence, held[(h / 7) % held.size()], (h / 31) % 5 == 0};
}

int Application::crystalHits(Position altar) const {
    for (const auto& [at, hits] : crystalCracks_) if (at.x == altar.x && at.y == altar.y) return hits;
    return 0;
}

void Application::releaseEssence() {
    const auto [essence, type, corrupt] = essenceAt(landmarkAltar_);
    // The nearest open tile beside the crystal.
    std::optional<Position> spot;
    for (int r = 1; r <= 3 && !spot; ++r)
        for (int dy = -r; dy <= r && !spot; ++dy)
            for (int dx = -r; dx <= r && !spot; ++dx) {
                const Position p{landmarkAltar_.x + dx, landmarkAltar_.y + dy};
                if (chebyshev(p, landmarkAltar_) == r && map_.isWalkable(p.x, p.y) && !isOccupied(p, nullptr)) spot = p;
            }
    if (!spot) return;
    auto m = createMonster(type, *spot, corrupt ? MonsterTier::Nightmare : MonsterTier::Elite);
    scaleDungeonMonster(*m, floorDepth(currentFloor_));
    m->essence = static_cast<int>(essence);
    m->corrupted = corrupt;
    m->stats().maxHp = m->stats().maxHp * (corrupt ? 5 : 4) / 2;
    m->stats().hp = m->stats().maxHp;
    m->lastObservedHp = m->stats().hp;
    dressEventMonster(*m);
    m->tactics.concealed = false;
    m->tactics.alert = 8;
    m->tactics.lastKnown = player_.position();
    m->recoveryActions = 1;
    log("The crystal shatters!");
    scheduler_.add(*m);
    monsters_.push_back(std::move(m));
}

// Its blows carry its essence.
void Application::essenceStrike(const Monster& m, AIDecision& decision) const {
    if (!m.essence || decision.type != AIActionType::Attack || decision.effectToApply) return;
    switch (static_cast<Essence>(m.essence)) {
        case Essence::Flame: decision.effectToApply = StatusEffectInstance{StatusEffectType::Burn, 3, 3}; break;
        case Essence::Frost: decision.effectToApply = StatusEffectInstance{StatusEffectType::Chill, 2, 30}; break;
        case Essence::Storms: decision.effectToApply = StatusEffectInstance{StatusEffectType::Shock, 2, 0}; break;
        default: break;
    }
}

// A rare item bearing the essence's affix at its highest roll.
std::unique_ptr<Item> Application::essenceItem(Essence essence, Position at) {
    const auto* wanted = findAffix(essenceInfo(essence).affix);
    const int depth = floorDepth(currentFloor_);
    const auto id = nextItemId_++;
    std::unique_ptr<Item> item;
    for (int tries = 0; tries < 24; ++tries) {
        item = loot_.generate(depth, 2, id, at, ItemRarity::Rare, lootTheme(currentFloor_));
        if (wanted && item->definition() && (wanted->slots & (1u << static_cast<unsigned>(item->definition()->slot)))) break;
    }
    if (!item->definition()) return item;
    const int tier = item->rollTier();
    std::vector<RolledAffix> affixes;
    const bool fits = wanted && (wanted->slots & (1u << static_cast<unsigned>(item->definition()->slot)));
    if (fits) {
        affixes.push_back({wanted->id, wanted->maximum + tier * wanted->perTier});
        for (const auto& a : item->affixes())
            if (affixes.size() < 2 && findAffix(a.id)->stat != wanted->stat) affixes.push_back(a);
    } else {
        // No room for its own affix: the first one is perfected instead.
        affixes = item->affixes();
        if (!affixes.empty()) { const auto* first = findAffix(affixes[0].id); affixes[0].value = first->maximum + tier * first->perTier; }
    }
    return std::make_unique<Item>(*item->definition(), id, at, affixes, tier);
}

// --- Breach ------------------------------------------------------------------

int Application::breachRadius() const { return std::min(6, 2 + (kBreachTurns - breachTurns_) / 2); }

void Application::openBreach() {
    breachAt_ = landmarkAltar_;
    breachTurns_ = kBreachTurns;
    breachKills_ = 0;
    log("The rift tears open.");
    spawnRiftborn(2, false);
}

void Application::spawnRiftborn(int count, bool keeper) {
    std::vector<MonsterType> roster{MonsterType::Goblin, MonsterType::GoblinRaider, MonsterType::GoblinStalker};
    MonsterType brute = MonsterType::Ogre;
    if (cathedralFloor(currentFloor_)) { roster = {MonsterType::DrownedOne, MonsterType::DeepLurker}; brute = MonsterType::DeepLurker; }
    else if (floorTheme(currentFloor_).region == FloorRegion::Crypts) { roster = {MonsterType::Skeleton, MonsterType::CryptShade, MonsterType::SkeletonGuard}; brute = MonsterType::CryptSentinel; }
    const int radius = breachRadius();
    std::vector<Position> edge;
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
            const Position p{breachAt_.x + dx, breachAt_.y + dy};
            const int d = chebyshev(p, breachAt_);
            if (d < std::max(1, radius - 1) || d > radius || !map_.isWalkable(p.x, p.y) || isOccupied(p, nullptr) ||
                chebyshev(p, player_.position()) < 2) continue;
            edge.push_back(p);
        }
    for (int n = 0; n < count && !edge.empty(); ++n) {
        const auto i = loot_.roll(static_cast<unsigned>(edge.size()));
        const Position p = edge[i];
        edge.erase(edge.begin() + static_cast<std::ptrdiff_t>(i));
        const bool lord = keeper && n == 0;
        auto m = createMonster(lord ? brute : roster[loot_.roll(static_cast<unsigned>(roster.size()))], p,
                               lord ? MonsterTier::Nightmare : MonsterTier::Base);
        scaleDungeonMonster(*m, floorDepth(currentFloor_));
        m->rift = lord ? 2 : 1;
        if (lord) { m->stats().maxHp = m->stats().maxHp * 3 / 2; m->stats().hp = m->stats().maxHp; m->lastObservedHp = m->stats().hp; }
        dressEventMonster(*m);
        m->tactics.concealed = false;
        m->tactics.alert = 8;
        m->tactics.lastKnown = player_.position();
        m->recoveryActions = 1;
        scheduler_.add(*m);
        monsters_.push_back(std::move(m));
    }
    if (keeper) log("Something vast steps through the rift.");
}

// Each turn the rift widens and more pour out.
void Application::tickBreach() {
    if (breachTurns_ <= 0 || mode_ != GameMode::Playing) return;
    const int elapsed = kBreachTurns - breachTurns_;
    --breachTurns_;
    spawnRiftborn(elapsed % 3 == 2 ? 2 : 1, false);
    if (elapsed == kKeeperTurn) spawnRiftborn(1, true);
    if (breachTurns_ == 0) closeBreach(false);
}

void Application::closeBreach(bool keeperSlain) {
    breachTurns_ = 0;
    int vanished = 0;
    for (auto& m : monsters_)
        if (m->rift && m->stats().hp > 0) {
            m->setRewardsEligible(false);
            m->stats().hp = 0;
            scheduler_.remove(*m);
            ++vanished;
        }
    const int magic = std::min(3, breachKills_ / 3);
    if (keeperSlain) spillLoot(ItemRarity::Rare, 2, breachAt_);
    if (magic) spillLoot(ItemRarity::Magic, magic, breachAt_);
    (void)vanished;
    log(keeperSlain ? "The rift snaps shut." : "The rift collapses.");
}

// Deaths that end or reward an event.
void Application::eventDeath(Monster& m) {
    if (m.essence) {
        const int count = m.corrupted ? 2 : 1;
        for (int i = 0; i < count && nextItemId_ < std::numeric_limits<std::uint64_t>::max(); ++i) {
            auto item = essenceItem(static_cast<Essence>(m.essence), m.position());
            log(m.name(), " drops ", item->name(), ".");
            groundItems_.push_back(std::move(item));
        }
    }
    if (m.rift == 1 && breachTurns_ > 0) ++breachKills_;
    if (m.rift == 2 && breachTurns_ > 0) closeBreach(true);
}

// The rift's reach: a violet stain that spreads as it widens.
void Application::renderBreach() {
    if (breachTurns_ <= 0) return;
    const float tile = static_cast<float>(playLayout::tileSize);
    const float now = animationClock_.getElapsedTime().asSeconds();
    const int radius = breachRadius();
    sf::RectangleShape cell({tile, tile});
    for (int dy = -radius; dy <= radius; ++dy)
        for (int dx = -radius; dx <= radius; ++dx) {
            const Position p{breachAt_.x + dx, breachAt_.y + dy};
            if (!map_.inBounds(p.x, p.y) || !map_.isWalkable(p.x, p.y) || exploredMap_.at(p.x, p.y) == Visibility::Hidden) continue;
            const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            if (d > radius + 0.5f) continue;
            const bool rim = d > radius - 0.7f;
            const float pulse = 0.5f + 0.5f * std::sin(now * 3.f - d * 0.8f);
            // Near its end the rift falters and flickers.
            const float falter = breachTurns_ <= 4 ? 0.35f + 0.65f * std::abs(std::sin(now * 9.f + d)) : 1.f;
            const auto at = worldToScreen(p.x, p.y);
            cell.setPosition(at);
            cell.setFillColor(sf::Color(kRiftColour.r, kRiftColour.g, kRiftColour.b,
                                        static_cast<std::uint8_t>(((rim ? 70 : 34) + (rim ? 50 : 22) * pulse) * falter)));
            window_.draw(cell);
        }
}

// The archway on the altar tile: a skull-ringed gate, swirling while it waits or rages.
void Application::drawBreachRift(sf::Vector2f at, float tile, sf::Color tint, bool active) {
    const float now = animationClock_.getElapsedTime().asSeconds();
    if (active) {
        const float pulse = 0.5f + 0.5f * std::sin(now * 2.8f);
        sf::CircleShape glow(tile * 1.0f);
        glow.setOrigin({tile, tile});
        glow.setPosition({at.x + tile / 2, at.y + tile / 2});
        glow.setFillColor(sf::Color(kRiftColour.r, kRiftColour.g, kRiftColour.b, static_cast<std::uint8_t>(45 + 55 * pulse)));
        window_.draw(glow);
    }
    sprites_.draw(window_, {kRiftArt, sf::IntRect({0, 0}, {32, 32})}, {at.x - tile * 0.2f, at.y - tile * 0.4f}, tile * 1.4f,
                  active ? tint : sf::Color(tint.r / 2, tint.g / 2, tint.b / 2));
}

} // namespace engine
