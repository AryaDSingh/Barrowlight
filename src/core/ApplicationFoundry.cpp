#include "core/Application.hpp"
#include "entities/MonsterFactory.hpp"

#include <algorithm>

namespace engine {

// The Ashen Foundry: heat, the fires the imps fan, and what its monsters
// leave behind when they die.

namespace {
constexpr int kMaxHeat = 15, kBurningHeat = 10;
}

void Application::addHeat(int amount) {
    const int heat = std::clamp(player_.statusEffects().magnitudeOf(StatusEffectType::Heat) + amount, 0, kMaxHeat);
    player_.statusEffects().remove(StatusEffectType::Heat);
    if (heat > 0) player_.statusEffects().apply({StatusEffectType::Heat, 10000, heat});
    if (amount > 0) heatFresh_ = true;
}

// As your turn begins: furnaces in the 8 tiles around you, or fire under your
// feet, heat you; away from them you cool; water quenches you at once. Too
// hot, and it burns.
void Application::tickHeat() {
    const auto me = player_.position();
    const int heat = player_.statusEffects().magnitudeOf(StatusEffectType::Heat);
    if (surfaceAt(me) == SurfaceType::Water) {
        if (heat > 0) { addHeat(-heat); log("The water hisses as you cool."); }
        return;
    }
    bool hot = surfaceAt(me) == SurfaceType::Fire;
    for (const auto& prop : props_)
        if (prop.kind == PropKind::Furnace && std::max(std::abs(prop.pos.x - me.x), std::abs(prop.pos.y - me.y)) <= 1) hot = true;
    const auto held = std::find_if(player_.statusEffects().active().begin(), player_.statusEffects().active().end(),
                                    [](const StatusEffectInstance& e) { return e.type == StatusEffectType::Forgeheart; });
    const bool holding = held != player_.statusEffects().active().end();
    const int heldTurns = holding ? held->turnsRemaining : 0; // read now: changing Heat rewrites the list
    const bool fresh = heatFresh_;
    heatFresh_ = false;
    if (hot) { addHeat(1); heatFresh_ = false; } // the furnace's heat follows the usual rule
    else if (heat > 0 && !holding && !fresh) addHeat(-1);
    // Forgeheart's last turn: the heat it held comes out all at once.
    if (holding && heldTurns <= 1) {
        const int vent = player_.statusEffects().magnitudeOf(StatusEffectType::Heat);
        addHeat(-vent);
        if (vent) {
            log("Your forgeheart bursts!");
            for (auto& m : monsters_)
                if (!m->allied && m->stats().hp > 0 && std::max(std::abs(m->position().x - me.x), std::abs(m->position().y - me.y)) <= 2) {
                    m->stats().hp -= 2 * vent; flashActor(*m); checkAndHandleDeath(*m);
                }
            for (int dy = -2; dy <= 2; ++dy) for (int dx = -2; dx <= 2; ++dx)
                if ((dx || dy) && map_.isWalkable(me.x + dx, me.y + dy)) setSurface({me.x + dx, me.y + dy}, SurfaceType::Fire, kSpilledFireTurns);
            removeDeadMonsters();
        }
    }
    if (player_.statusEffects().magnitudeOf(StatusEffectType::Heat) >= kBurningHeat &&
        !player_.talents().passiveValue(PassiveKind::HeatSink, player_.stats())) {
        player_.stats().hp -= 2;
        flashActor(player_);
        harmSource_ = "the heat";
        log("The heat sears you.");
        checkAndHandleDeath(player_);
        harmSource_.clear();
    }
}

// A Bellows Imp's turn: every fire within three tiles of it creeps a tile.
void Application::fanFires(Position imp) {
    std::vector<Position> burning;
    for (int dy = -3; dy <= 3; ++dy)
        for (int dx = -3; dx <= 3; ++dx)
            if (surfaceAt({imp.x + dx, imp.y + dy}) == SurfaceType::Fire) burning.push_back({imp.x + dx, imp.y + dy});
    int spread = 0;
    for (const auto& p : burning) {
        for (const Position n : {Position{p.x + 1, p.y}, Position{p.x - 1, p.y}, Position{p.x, p.y + 1}, Position{p.x, p.y - 1}}) {
            if (!map_.isWalkable(n.x, n.y) || surfaceAt(n) == SurfaceType::Fire || surfaceAt(n) == SurfaceType::Water) continue;
            setSurface(n, SurfaceType::Fire, kSpilledFireTurns);
            if (++spread >= 3) return;
            break;
        }
    }
}

// What the Foundry's monsters leave behind: a Slag Golem splits into two
// Slaglings, a Slagling leaves the ground burning, and Grik drops the
// Foreman's key. (Called for the Forgemaster at half life, too: its slaglings.)
void Application::foundryDeath(Monster& monster) {
    const auto at = monster.position();
    const auto spawnSlaglings = [&](int count, Position near) {
        for (int i = 0; i < count; ++i) {
            std::optional<Position> spot;
            for (int r = 1; r <= 3 && !spot; ++r)
                for (int dy = -r; dy <= r && !spot; ++dy)
                    for (int dx = -r; dx <= r && !spot; ++dx)
                        if (std::max(std::abs(dx), std::abs(dy)) == r && map_.isWalkable(near.x + dx, near.y + dy) && !isOccupied({near.x + dx, near.y + dy}, nullptr))
                            spot = Position{near.x + dx, near.y + dy};
            if (!spot) return;
            auto slag = createMonster(MonsterType::Slagling, *spot);
            scaleDungeonMonster(*slag, floorDepth(currentFloor_));
            slag->lastObservedHp = slag->stats().hp;
            slag->tactics.alert = 8; slag->tactics.lastKnown = player_.position(); slag->voicedAlert = true;
            scheduler_.add(*slag);
            monsters_.push_back(std::move(slag));
        }
    };
    if (monster.type() == MonsterType::SlagGolem && monster.stats().hp <= 0) {
        log("The golem cracks apart, and its molten heart crawls out in pieces!");
        spawnSlaglings(2, at);
    }
    if (monster.type() == MonsterType::Slagling && monster.stats().hp <= 0) setSurface(at, SurfaceType::Fire, kSpilledFireTurns);
    if (monster.type() == MonsterType::Forgemaster && monster.stats().hp > 0) {
        // Out of the two furnaces nearest you.
        std::vector<Position> furnaces;
        for (const auto& prop : props_) if (prop.kind == PropKind::Furnace) furnaces.push_back(prop.pos);
        const auto me = player_.position();
        std::sort(furnaces.begin(), furnaces.end(), [&](Position a, Position b) {
            return std::abs(a.x - me.x) + std::abs(a.y - me.y) < std::abs(b.x - me.x) + std::abs(b.y - me.y); });
        log("The Forgemaster roars, and the furnaces spill out their slag!");
        if (furnaces.empty()) spawnSlaglings(2, at);
        for (std::size_t i = 0; i < std::min<std::size_t>(2, furnaces.size()); ++i) spawnSlaglings(1, furnaces[i]);
    }
    if (monster.type() == MonsterType::GoblinCaptain && monster.stats().hp <= 0 && !player_.knowsLore("foreman_key") &&
        std::none_of(loreDrops_.begin(), loreDrops_.end(), [](const LoreDrop& d) { return d.id == "foreman_key"; })) {
        loreDrops_.push_back({at, "foreman_key"});
        log("An iron key clatters from Grik's belt.");
    }
}

} // namespace engine
